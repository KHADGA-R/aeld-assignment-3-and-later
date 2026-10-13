#!/bin/sh

# Author : Khadga Parajuli
# Check that exactly two command-line arguments were provided.
# Argument 1: Directory to search
# Argument 2: String to search for

if [ $# -ne 2 ] 
then 
	echo "Error : two arguments are required"
	exit 1
fi

# Store the first argument as the directory path.
filesdir=$1

# Store the second argument as the string to search for.
searchstr=$2

# Check whether the provided path exists and is a directory.
if [ ! -d "$filesdir" ]
then
	echo "Error : $filesdir is not a directory"
	exit 1

fi

# Find all regular files (-type f) inside the specified directory
# and count them using wc -l.
numfiles=$(find "$filesdir" -type f | wc -l)

# Recursively search for the specified string in the directory.
# grep -r searches recursively through files.
# wc -l counts the number of matching output lines.
num_matching_lines=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $numfiles and the  number of matching lines are $num_matching_lines"
