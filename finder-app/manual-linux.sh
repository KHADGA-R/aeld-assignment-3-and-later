#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"
OUTDIR=$(realpath "${OUTDIR}")

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
  
    # Build the ARM64 Linux kernel
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} Image
fi

echo "Adding the Image in outdir"

# Copy the compiled kernel image to the output directory
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}/Image"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"

if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf "${OUTDIR}/rootfs"
fi

# TODO: Create necessary base directories

# Create the root filesystem directory structure
mkdir -p "${OUTDIR}/rootfs"

cd "${OUTDIR}/rootfs"

mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin usr/lib
mkdir -p var/log

# Set permissions for the temporary directory
chmod 1777 tmp

# Download and configure BusyBox
cd "${OUTDIR}"

if [ ! -d "${OUTDIR}/busybox" ]; then
    echo "Cloning BusyBox version ${BUSYBOX_VERSION}"

    git clone https://git.busybox.net/busybox.git busybox
fi

cd "${OUTDIR}/busybox"

# Check out the required BusyBox version
git checkout "${BUSYBOX_VERSION}"

# Configure BusyBox for ARM64
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

# TODO: Make and install busybox

# Build BusyBox for ARM64
echo "Building BusyBox for ${ARCH}"

make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" -j"$(nproc)"

# Install BusyBox into the root filesystem
echo "Installing BusyBox into ${OUTDIR}/rootfs"

make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" \
    CONFIG_PREFIX="${OUTDIR}/rootfs" install

# Return to the root filesystem directory
cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

# Add ARM64 shared library dependencies to rootfs
echo "Copying ARM64 shared libraries"

SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ ! -d "${SYSROOT}/lib" ]; then
    echo "ERROR: ARM64 sysroot libraries not found: ${SYSROOT}/lib"
    exit 1
fi

# Copy runtime libraries, preserving symbolic links
cp -a "${SYSROOT}/lib/." "${OUTDIR}/rootfs/lib/"

# Copy additional libraries if present
if [ -d "${SYSROOT}/lib64" ]; then
    cp -a "${SYSROOT}/lib64/." "${OUTDIR}/rootfs/lib64/"
fi

echo "ARM64 libraries copied successfully"

# TODO: Make device nodes

# Create essential device nodes for the root filesystem
echo "Creating device nodes"

mkdir -p "${OUTDIR}/rootfs/dev"

sudo mknod -m 666 "${OUTDIR}/rootfs/dev/null" c 1 3
sudo mknod -m 600 "${OUTDIR}/rootfs/dev/console" c 5 1

echo "Device nodes created successfully"

# TODO: Clean and build the writer utility
# Clean and cross-compile the writer application for ARM64
echo "Building writer application for ARM64"

cd "${FINDER_APP_DIR}"

make clean
make CROSS_COMPILE="${CROSS_COMPILE}" writer

# Copy the ARM64 executable into the target root filesystem
cp "${FINDER_APP_DIR}/writer" "${OUTDIR}/rootfs/home/writer"

echo "Writer application installed successfully"

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs

# Copy finder scripts and configuration files into the target rootfs
echo "Copying finder scripts and configuration files"

mkdir -p "${OUTDIR}/rootfs/home/conf"

cp "${FINDER_APP_DIR}/finder.sh" "${OUTDIR}/rootfs/home/"
cp "${FINDER_APP_DIR}/finder-test.sh" "${OUTDIR}/rootfs/home/"
cp "${FINDER_APP_DIR}/autorun-qemu.sh" "${OUTDIR}/rootfs/home/"

cp "${FINDER_APP_DIR}/../conf/username.txt" "${OUTDIR}/rootfs/home/conf/"
cp "${FINDER_APP_DIR}/../conf/assignment.txt" "${OUTDIR}/rootfs/home/conf/"

# Update the copied test script to use the target filesystem layout
sed -i 's|\.\./conf/assignment\.txt|conf/assignment.txt|g' \
    "${OUTDIR}/rootfs/home/finder-test.sh"

# Make the scripts executable
chmod +x "${OUTDIR}/rootfs/home/finder.sh"
chmod +x "${OUTDIR}/rootfs/home/finder-test.sh"
chmod +x "${OUTDIR}/rootfs/home/autorun-qemu.sh"

echo "Finder scripts copied successfully"

# TODO: Chown the root directory
# Set root ownership for the target filesystem
echo "Setting root filesystem ownership"

sudo chown -R root:root "${OUTDIR}/rootfs"

# TODO: Create initramfs.cpio.gz
# Create the compressed initramfs archive
echo "Creating initramfs.cpio.gz"

cd "${OUTDIR}/rootfs"

find . -print0 | cpio --null -ov --format=newc \
    --owner=0:0 | gzip -9 > "${OUTDIR}/initramfs.cpio.gz"

echo "Initramfs created successfully"
echo "Kernel image: ${OUTDIR}/Image"
echo "Root filesystem: ${OUTDIR}/rootfs"
echo "Initramfs: ${OUTDIR}/initramfs.cpio.gz"
