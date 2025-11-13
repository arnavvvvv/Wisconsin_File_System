# Part 1 - FILE IT UP! (Core FUSE Operations)

## Objective
This is the foundation. Your goal is to complete the FUSE stubs in `wfs.c`. When you're done, you should be able to create files, write to them, read them back, create nested directories, and list their contents.

Most of your FUSE stubs currently just return `-ENOSYS`; (Function Not Implemented). Your task is to replace those with working code.

```c
static struct fuse_operations wfs_ops = {
    .getattr = wfs_getattr,
    .mknod   = wfs_mknod,
    .mkdir   = wfs_mkdir,
    .read    = wfs_read,
    .write   = wfs_write,
    .readdir = wfs_readdir,
    .unlink  = wfs_unlink,
    .rmdir   = wfs_rmdir,
    /* For later parts */
    .statfs  = wfs_statfs,
    .setxattr = wfs_setxattr,
    .getxattr = wfs_getxattr,
    .removexattr = wfs_removexattr,
};
```
To do this, you'll first need to implement the core mechanics of your filesystem. We've provided helper function stubs (marked TODO) in wfs.c for these.

## Implementation Guidance:

### Inode Structure (Incremental Features)
```c
struct wfs_inode {
  int    num;
  mode_t mode;
  uid_t  uid;
  gid_t gid;
  off_t  size;
  int nlinks;
  off_t  blocks[N_BLOCKS];     
};
/* NOTE: The starter file intentionally omits time and color from the inode so you must think about where and how to update them. This also forces a deliberate modification to the on-disk layout. */
```

### Path resolution:
- The Problem: How do you get from a path string like /a/b/c to the specific struct wfs_inode for file c?
Think Recursion? Maybe?
- The prototype for `get_inode_from_path(const char *path, struct wfs_inode **inode)` is provided. You will need to call this helper from every path‑based FUSE function (getattr, read, write, mkdir, mknod, unlink, rmdir, xattr) so path code lives in one place.

### Pointer vs. Offset:
- An Offset (an off_t) is an integer number of bytes from the start of the disk image. You store these on-disk (e.g., in inode->blocks).
- A Pointer (a char* or struct wfs_inode*) is a C memory address. You get this by adding an offset to the global mregion pointer.
- Rule: Never store a pointer on disk. Always store offsets.

### Disk Mapping:
- The starter code mmaps the disk image once into a global mapping `mregion`. You will use this mapping to access on‑disk structures.
- On‑disk structures must store offsets, not host pointers. Compute addresses as `(char*)mregion + <on_disk_offset>` when you need to access memory.

### Allocation Strategy
- Store block offsets, not raw pointers, in inode->blocks so the image is position independent.
- Indirect block: allocate an entire block to hold an array of `off_t` entries when needed.

### Zero Your Memory:
- When you allocate_data_block or allocate_inode, you get a chunk of disk that may contain old, stale data.
- Use memset(..., 0, ...) to zero it out immediately. Failing to do this can cause "ghost" directory entries or garbage file content.

### Error Handling
- Return negative errno through FUSE methods. 
- If any of the following issues occur during the execution of a registered function, it's essential to return the respective error code. These error code macros are accessible by including header `<errno.h>`.
- File/directory does not exist while trying to read/write a file/directory\
    return -ENOENT`
- A file or directory with the same name already exists while trying to create a file/directory.\
return -EEXIST
- There is insufficient disk space while trying to create or write to a file.\
return -ENOSPC

### Debugging
- Your filesystem will print to stdout if it is running in the foreground (-f). We recommend adding a print statement to the beginning of each callback function so that you know which functions are being called (e.g. watch how many times getattr is called). You may, of course, add more detailed print statements to debug further.
- To run the filesystem in gdb, use gdb --args ./wfs disk.img -f -s mnt, and type run once inside gdb.

## Deliverables:
- Create/write/read/remove files and directories.
- Path traversal works through nested directories.
- You should be able to interact with your filesystem once you mount it: 

```sh
$ stat mnt
$ mkdir mnt/a
$ stat mnt/a
$ mkdir mnt/a/b
$ ls mnt
$ echo asdf > mnt/x
$ cat mnt/x
```

## Important Notes

1. Manually inspecting your filesystem (see Debugging section above), before running the tests, is highly encouraged. You should also experiment with your filesystem using simple utilities such as `mkdir`, `ls`, `touch`, `echo`, `rm`, etc. 
2. Directories will not use the indirect block. That is, directories entries will be limited to the number that can fit in the direct data blocks.
3. Directories may contain blank entries up to the size as marked in the directory inode. That is, a directory with size 512 bytes will use one data block, both of the following entry layouts would be legal -- 15 blank entries followed by a valid directory entry, or a valid directory entry followed by 15 blank entries. You should free all directory data blocks with `rmdir`, but you do not need to free directory data blocks when unlinking files in a directory.
4. A valid file/directory name consists of letters (both uppercase and lowercase), numbers, and underscores (_). Path names are always separated by forward-slash. You do not need to worry about escape sequences for other characters.
5. The maximum file name length is 28
6. Please make sure your code can be compiled using the commands in the provided Makefile.
7. We recommend using `mmap` to map the entire disk image into memory when the filesystem is mounted. Mapping the image into memory simplifies reading and writing the on-disk structures a great deal, compared to `read` and `write` system calls.
8. Think carefully about the interfaces you will need to build to manipulate on-disk data structures. For example, you might have an `allocate_inode()` function which allocates an inode using the bitmap and returns a pointer to a new inode, or returns an error if there are no more inodes available in the system.
9. You must use the superblock and inode structs as they are defined in the header file, but the actual allocation and free strategies are up to you. Our tests will evaluate whether or not you have the correct number of blocks allocated, but we do not assume they are in a particular location on-disk.





