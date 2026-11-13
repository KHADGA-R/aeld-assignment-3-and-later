# Assignment 7: Faulty Kernel Module Oops Analysis

## Test Environment
- Build system: Yocto Project (Poky 6.0.4)
- Target: QEMU ARM64 (qemuarm64)
- Kernel: 6.18.54-yocto-standard
- Driver: faulty.ko

## Steps to Reproduce
After booting QEMU and verifying that the faulty module and /dev/faulty were present, I executed:
    echo "hello_world" > /dev/faulty

## Relevant Kernel Oops Output
    Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
    ESR = 0x0000000096000044
    EC = 0x25: DABT (current EL)
    FSC = 0x04: level 0 translation fault
    WnR = 1
    Internal error: Oops: 0000000096000044 [#1] SMP
    Modules linked in: scull(O) faulty(O) hello(O)
    pc : faulty_write+0xc/0x20 [faulty]
    lr : vfs_write+0xd8/0x398
    Call trace:
     faulty_write+0xc/0x20 [faulty] (P)
     ksys_write+0x6c/0x118
     __arm64_sys_write+0x20/0x40

## Root Cause Analysis
Writing to /dev/faulty invokes the faulty_write() callback in the faulty kernel module.
The driver deliberately attempts to write through a NULL pointer, accessing virtual address 0x0.
Because this address is not mapped, the ARM64 processor raises a data abort and Linux reports a kernel oops.

The exception details identify the problem:
- EC = 0x25 indicates a data abort at the current exception level.
- WnR = 1 indicates that the invalid memory access was a write.
- FSC = 0x04 indicates a level 0 address translation fault.
- The program counter identifies faulty_write() as the faulting function.
- The call trace connects the user-space write system call to the driver.

## Conclusion
The kernel oops was intentionally triggered by writing to /dev/faulty.
This confirms that the faulty driver was successfully built, installed, and loaded in the Yocto ARM64 image.
