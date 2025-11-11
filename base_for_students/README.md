# CS537 Project P6 – Mini WFS Filesystem

Welcome to the mini WFS filesystem project. You will build a small, block-based userspace filesystem using FUSE over four incremental parts. Each part adds specific functionality. The final product supports inode/data allocation, file and directory operations, filesystem statistics, POSIX timestamps, and per-file color tagging via extended attributes (xattrs) with colored `ls` output.

## Repository Layout (Student View)
```
/ (repo root)
├── mkfs.c              # Provided: builds an empty disk image with superblock/bitmaps/inode 0 (root)
├── disk.img            # Generated: initial empty filesystem image (size configurable)
├── starter/
│   ├── wfs_student.c   # YOUR WORK: skeleton FUSE implementation (fill TODOs)
│   └── Makefile        # Build targets: mkfs + wfs
└── tests/              # (Optional) instructor or self-written tests
```

## Build & Run Quick Start

To help you run your filesystem, we provided several scripts: 

- `create_disk.sh` creates a file named `disk` with size 1MB whose content is zeroed. You can use this file as your disk image. We may test your filesystem with images of different sizes, so please do not assume the image is always 1MB.
- `umount.sh` unmounts a mount point whose path is specified in the first argument. 
- `Makefile` is a template makefile used to compile your code. It will also be used for grading. Please make sure your code can be compiled using the commands in this makefile. 

A typical way to compile and launch your filesystem is: 

```sh
$ make
$ ./create_disk.sh                 # creates file `disk.img`
$ ./mkfs -d disk.img -i 32 -b 200  # initialize `disk.img`
$ mkdir mnt
$ ./wfs disk.img -f -s mnt             # mount. -f runs FUSE in foreground
```

You should be able to interact with your filesystem once you mount it: 

```sh
$ stat mnt
$ mkdir mnt/a
$ stat mnt/a
$ mkdir mnt/a/b
$ ls mnt
$ echo asdf > mnt/x
$ cat mnt/x
```

## Disk Layout (On-disk)
```
+---------+----------+----------+---------+------------------+
| SB      | I_BITMAP | D_BITMAP | INODES  | DATA BLOCKS      |
+---------+----------+----------+---------+------------------+
0         ^          ^          ^
          |          |          |
          i_bitmap_ptr  d_bitmap_ptr
                      i_blocks_ptr
                                  d_blocks_ptr
```
- Superblock (struct wfs_sb) at offset 0.
- Bitmaps are packed (1 bit per inode/block).
- Inodes are BLOCK_SIZE-aligned; each inode occupies an entire block for simplicity.
- Data blocks hold file content and directory entries.

## Inode Structure (Incremental Features)
```c
struct wfs_inode {
  int    num;
  mode_t mode;
  uid_t  uid; gid_t gid;
  off_t  size; int nlinks;
  off_t  blocks[N_BLOCKS];      // 6 direct + 1 indirect pointer block
  /* PART 3: YOU add time fields: time_t atim, mtim, ctim */
  /* PART 4: YOU add: unsigned char color; (0 = none; more palette values below) */
};
// NOTE: The starter file intentionally omits time and color from the inode so you must
// think about where and how to update them. This also forces a deliberate modification
// to the on-disk layout — document this change in DESIGN.md.
```
Implement in `wfs_student.c`:
- Path resolution of absolute paths (`/a/b/c`).
Deliverables:
- All stubs in Part 1 converted from `-ENOSYS` to working implementations.
- Create/write/read/remove files and directories.
- Path traversal works through nested directories.

Checklist:
- File spanning multiple blocks returns consistent content.

### Part 2 – Show me the Big Picture (statfs)
Goal: Report aggregate filesystem statistics.
Implement `statfs()`:
- `f_blocks` = total data block capacity.
- `f_files` = total inode capacity.
- `f_bfree`/`f_bavail` = number of free data blocks.

Use bitmap popcount to calculate used/free. After allocations, free counts should drop.

- Creating files reduces `f_ffree`; writing allocating blocks reduces `f_bfree`.

### Part 3 – Tick Tok Tick Tok (File Times)
Extend your inode with three timestamps: `atim` (last access), `mtim` (last content modification), and `ctim` (last metadata/status change). Keep them consistent with familiar POSIX behavior so `stat` and friends look reasonable.

What’s expected:
- Creation: initialize all three to “now” when a file or directory is first made.
- Reporting: `getattr` returns these values; it never changes them.
- Reading: when file contents are actually read, advance the file’s access time.
- Listing directories: after producing a directory listing, advance that directory’s access time.
- Writing: when file contents change, advance modification time; metadata-affecting operations should advance change time.
- Directory entry changes: when you add or remove a child, the parent directory’s modification and change times should advance.
- Removal: parent directory times should reflect the change; it’s reasonable to also advance the removed object’s change time during teardown.

Hints
- Place time updates where you’re certain the operation succeeded to avoid spurious changes.
- Keep behavior uniform across similar code paths (e.g., all writes update the same set of times).

Sanity checks
- Immediately after creation, the three times match.
- A read followed by a write shows access time advancing on the read, and modification/change times advancing on the write.
- Listing a directory advances that directory’s access time; creating a child advances its modification/change times.

### Part 4 – Colour Colour which Colour do you choose? (xattrs + Colored ls)
Add a simple per-file “color” tag and surface it via an extended attribute. Show colored names in human-facing listings without baking escape codes into the on-disk names.

Guidance
- Store a compact palette code in each inode (0 = none). Expose it through the `user.color` xattr: set/get/list/remove.
- When listing, render names with color only for `ls`; programmatic consumers should see plain names.
- Treat changing color as a metadata update (consider the timestamp it should affect).

## Implementation Guidance

### Allocation Strategy
- Store block offsets, not raw pointers, in inode->blocks so the image is position independent.
- Indirect block: allocate an entire block to hold an array of `off_t` entries when needed.

### Error Handling
Return negative errno through FUSE methods. Use helper `wfs_error` only if you wrap deeper functions; otherwise return directly.

### Testing Suggestions
Create small C test programs or shell scripts:
- Stress path resolution (`/a/b/c` creation).
- Validate file spanning direct + indirect blocks.
- Confirm timestamps using `stat()` before/after operations.
- Check `statvfs()` counts after batches of allocations.
- Color tests: use `setxattr`/`getxattr` then run `ls` vs. `find`.

### Performance Notes
This is educational: no caching beyond what the OS provides. Algorithms may be O(n) on directory size; that’s acceptable within constraints.

## Common Pitfalls
- Forgetting to subtract `D_BLOCK` (not `IND_BLOCK`) when indexing indirect blocks.
- Colorizing names for non-`ls` processes.
- Updating `mtime` on read or failing to update parent times on directory entry changes.
- Not zeroing freed blocks/inodes (leads to stale dentries appearing after reuse). (Design a test that creates many files, deletes them, then lists to ensure freed entries don’t leak names.)


Good luck – build it step by step. FILE IT UP, then zoom out for the Big Picture, keep time with Tick Tok Tick Tok, and finish with a splash of Colour!
