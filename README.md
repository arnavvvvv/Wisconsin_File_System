# CS537 Project P6 – Mini WFS Filesystem

Welcome to the mini WFS (Wisconsin File System) project! We're going to build a small, block-based userspace filesystem similar to those you have seen in class such as FFS or ext2, from the ground up using FUSE. 

The project has four incremental parts. Each part adds specific functionality. The final product supports file and directory operations, filesystem statistics, POSIX timestamps, and per-file color tagging via extended attributes (xattrs) with colored `ls` output.

## Objectives

- To understand how filesystem operations are implemented.
- To implement a traditional block-based filesystem.
- To learn to build a user-level filesystem using FUSE.
- To learn how to add new capabilities to existing filesystem paradigms.

## Repository Layout
```
/ (repo root)
├── Makefile
├── create_disk.sh      # Helper script to create a blank 1MB disk image 
├── umount.sh           # Helper script to unmount the 'mnt' directory
├── mkfs.c              # Initializes the disk image with the filesystem layout
├── wfs.c               # The FUSE driver you will build
├── wfs.h               # Header file with all on-disk structures
├── instructions/       # Specs for Parts 1-4
│   ├── 01_Fuse_Operations.md
│   ├── 02_Show_me_the_Big_Picture.md
│   ├── 03_Tick_Tok_Tick_Tok.md
│   └── 04_Colour_Colour_which_Colour_do_you_choose.md
└── tests/              # A set of tests to check your work
```

## Build & Run Quick Start

To help you run your filesystem, we provided several scripts: 

- `create_disk.sh` creates a file named `disk.img` with size 1MB whose content is zeroed. You can use this file as your disk image. We may test your filesystem with images of different sizes, so please do not assume the image is always 1MB.
- `umount.sh` unmounts a mount point whose path is specified in the first argument. 
- `Makefile` is a template makefile used to compile your code. It will also be used for grading. Please make sure your code can be compiled using the commands in this makefile. 

A typical way to compile and launch your filesystem is: 

```sh
$ make
$ ./create_disk.sh                 
# # This creates a 1MB file named disk.img
$ ./mkfs -d disk.img -i 32 -b 200  
# # This initializes disk.img with 32 inodes and 200 data blocks
$ mkdir mnt
$ ./wfs disk.img -f -s mnt         
# This mounts your WFS implementation on the 'mnt' directory.
# -f runs FUSE in the foreground (so you can see printf logs)
# -s runs in single-threaded mode (which we require)
```

You should be able to interact with your filesystem once you mount it (in a second terminal): 
```sh
$ ls mnt
# This will likely fail or do nothing... because you haven't
# implemented 'readdir' yet!
```
## Background: What is FUSE?

FUSE (Filesystem in Userspace) is a powerful framework that lets you create your own filesystems in user space, without having to modify the Linux kernel.

To do this, you define callback functions for various filesystem operations (like `getattr`, `read`, `write`, `mkdir`, etc.). You register these functions in a single `struct fuse_operations`, which you then pass to FUSE's `main` function. 
When a user runs `ls mnt`, the kernel sees this, triggers the FUSE driver, and your `wfs_readdir` function is called.

A minimal example looks like this:

```c
#include <fuse.h>
#include <errno.h>

// 1. Your callback implementation
static int my_getattr(const char *path, struct stat *stbuf) {
    // ... your logic to find the file and fill 'stbuf' ...
    return 0; // or -ENOENT if not found
}

// 2. Your list of operations
static struct fuse_operations ops = {
    .getattr = my_getattr,
    // ... other ops like .mknod, .read, etc.
};

// 3. The main function
int main(int argc, char *argv[]) {
    // FUSE takes over, processes its own args,
    // and calls functions from 'ops' when needed.
    return fuse_main(argc, argv, &ops, NULL);
}
```

### Disk Layout

Our filesystem will have a superblock, inode and data block bitmaps, and inodes and data blocks. There are two types of files in our filesystem -- directories and regular files. You may presume the block size is always 512 bytes. 

Given `mkfs.c` tool creates this exact structure. Your `wfs.c` must read and write to it. The layout of a disk is shown below.

![filesystem layout on disk](base_for_students/instructions/disk-layout.svg)

- Superblock: Located at offset 0. This is the "map of maps." It tells you the total number of inodes and data blocks, and (most importantly) the on-disk offsets to the other sections.
- Inode Bitmap (IBITMAP): A packed bitmap (1 bit per inode). If bit i is 1, inode i is in use
- Data Bitmap (DBITMAP): A packed bitmap (1 bit per data block). If bit j is 1, data block j is in use.
- Inodes are BLOCK_SIZE-aligned; each inode occupies an entire block for simplicity. Each inode contains pointers to a fixed number of direct data blocks and a single indirect block to support larger files.
- Data Blocks (DATA BLOCKS): The rest of the disk. These blocks store the actual contents of files and the entries for directories.

Let's Get Building!

Right now, your filesystem doesn't do anything. The wfs.c file is just a skeleton. Every FUSE operation you try (like ls, mkdir, touch) will fail because its function stub just returns -ENOSYS (Function Not Implemented).

Your job is to implement these functions, one part at a time.

**[Part 1 - FILE IT UP! (Fuse Operations)](instructions/01_File_it_Up.md)**

**[Part 2 - Show me the Big Picture (statfs)](instructions/02_Show_me_the_Big_Picture.md)**

**[Part 3 - Tick Tok Tick Tok (File Times)](instructions/03_Tick_Tok_Tick_Tok.md)**

**[Part 4 - Color Color which Color do you choose? (xattrs + Colored ls)](instructions/04_Color_Color_which_Color_do_you_choose.md)**

Good luck – build it step by step. FILE IT UP, then zoom out for the Big Picture, keep time with Tick Tok Tick Tok, and finish with a splash of Colour!


## Useful Reading and References

* https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201001/homework/fuse/fuse_doc.html
* https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/html/index.html
* http://libfuse.github.io/doxygen/index.html
* https://github.com/fuse4x/fuse/tree/master/example
* `/usr/include/asm-generic/errno-base.h`