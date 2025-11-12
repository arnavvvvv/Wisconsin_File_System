#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>

#include "../wfs.h"


// Helper: strip ANSI color sequences from names (useful when colorizing ls output)
static void strip_ansi_codes(const char* in, char* out, size_t out_len) {
    const char* src = in; char* dst = out; if (out_len == 0) return; char* end = out + out_len - 1;
    while (*src && dst < end) {
        if ((unsigned char)*src == '\033') {
            src++;
            if (*src == '[') { src++; while (*src && *src != 'm') src++; if (*src == 'm') src++; }
        } else { *dst++ = *src++; }
    }
    *dst = '\0';
}

// Rich color palette (students: you will wire this into Part 4 xattr + readdir colorization)
typedef struct { const char* ansi; const char* name; } wfs_color_info;

static inline const wfs_color_info* wfs_color_from_code(unsigned char code) {
    static const wfs_color_info table[] = {
        [WFS_COLOR_NONE]    = { "",               "none"    },
        [WFS_COLOR_RED]     = { "\033[31m",       "red"     },
        [WFS_COLOR_GREEN]   = { "\033[32m",       "green"   },
        [WFS_COLOR_BLUE]    = { "\033[34m",       "blue"    },
        [WFS_COLOR_YELLOW]  = { "\033[33m",       "yellow"  },
        [WFS_COLOR_MAGENTA] = { "\033[35m",       "magenta" },
        [WFS_COLOR_CYAN]    = { "\033[36m",       "cyan"    },
        [WFS_COLOR_WHITE]   = { "\033[37m",       "white"   },
        [WFS_COLOR_BLACK]   = { "\033[30m",       "black"   },
        [WFS_COLOR_ORANGE]  = { "\033[38;5;208m", "orange"  },
        [WFS_COLOR_PURPLE]  = { "\033[35m",       "purple"  },
        [WFS_COLOR_GRAY]    = { "\033[90m",       "gray"    },
    };
    if (code < WFS_COLOR_MAX) return &table[code];
    return &table[WFS_COLOR_NONE];
}

static int parse_color_name(const char* s, unsigned char* out_code) {
    if (!s || !out_code) return 0; char buf[32]; size_t n=0;
    while (s[n] && n+1<sizeof(buf)) { buf[n] = (char)tolower((unsigned char)s[n]); n++; } buf[n] = '\0';
    for (unsigned char c = 0; c < WFS_COLOR_MAX; c++) {
        if (strcmp(buf, wfs_color_from_code(c)->name) == 0) { *out_code = c; return 1; }
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * CS537 P6 Filesystem Project Starter
 *
 * You will implement the mini WFS filesystem in FOUR STAGES:
 *  1. FILE IT UP           : Basic filesystem (superblock, inode alloc, data blocks,
 *                            create files/directories, read/write, readdir, unlink).
 *  2. Show me the Big Picture : statfs reporting global filesystem statistics.
 *  3. Tick Tok Tick Tok    : Correct atime/mtime/ctime handling on operations.
 *  4. Colour Colour ...    : Extended attribute user.color and colored ls output.
 *
 * This file provides the skeleton you must complete. Every TODO marker corresponds
 * to required functionality. Keep code modular; do not monolithically grow one
 * function. You are encouraged to add helper functions and files as needed.
 * --------------------------------------------------------------------------
 */

/* ----------------------- On-disk structure definitions -------------------- */
/* Use shared on-disk definitions from ../wfs.h. Do NOT redefine them here. */

/* --------------------------- Globals / Mount ------------------------------ */
static void* mregion;        // mapped disk image
static int   wfs_error;      // last error to return through FUSE

/* NOTE: Color codes are defined in ../wfs.h (wfs_color_t and WFS_COLOR_*).
 * The helper table above maps code -> ANSI + name for Part 4 convenience. */

/* -------------------------- Helper Prototypes ----------------------------- */
static struct wfs_sb* sb();
static struct wfs_inode* retrieve_inode(int inum);
static struct wfs_inode* allocate_inode(void);
static off_t allocate_data_block(void);
static void free_inode(struct wfs_inode* inode);
static void free_block(off_t blk_offset);
static char* data_offset(struct wfs_inode* inode, off_t offset, int alloc);
static int add_dentry(struct wfs_inode* dir, int inum, const char* name);
static int remove_dentry(struct wfs_inode* dir, int inum);
static int path_lookup(const char* path, struct wfs_inode** out);
static void fill_inode(struct wfs_inode* inode, mode_t mode);

/* ------------------------------ BITMAP Utils ------------------------------ */
static void bitmap_set(uint32_t* bm, uint32_t index) { bm[index/32] |=  (1u << (index%32)); }
static void bitmap_clear(uint32_t* bm, uint32_t index){ bm[index/32] &= ~(1u << (index%32)); }
static int  bitmap_test(uint32_t* bm, uint32_t index){ return (bm[index/32] & (1u << (index%32))) != 0; }

static ssize_t bitmap_alloc(uint32_t* bm, size_t total) {
    for (size_t i=0;i<total;i++) if (!bitmap_test(bm,i)) { bitmap_set(bm,i); return (ssize_t)i; }
    return -1;
}

/* ------------------------------ Core Helpers ------------------------------ */
static struct wfs_sb* sb() { return (struct wfs_sb*)mregion; }

static struct wfs_inode* retrieve_inode(int inum) {
    struct wfs_sb* s = sb();
    uint32_t* ibm = (uint32_t*)((char*)mregion + s->i_bitmap_ptr);
    if (!bitmap_test(ibm, inum)) return NULL;
    return (struct wfs_inode*)((char*)mregion + s->i_blocks_ptr + inum*BLOCK_SIZE);
}

static struct wfs_inode* allocate_inode(void) {
    struct wfs_sb* s = sb();
    uint32_t* ibm = (uint32_t*)((char*)mregion + s->i_bitmap_ptr);
    ssize_t idx = bitmap_alloc(ibm, s->num_inodes);
    if (idx < 0) { wfs_error = -ENOSPC; return NULL; }
    struct wfs_inode* inode = retrieve_inode((int)idx);
    inode->num = (int)idx; // rest set by fill_inode()
    return inode;
}

static off_t allocate_data_block(void) {
    struct wfs_sb* s = sb();
    uint32_t* dbm = (uint32_t*)((char*)mregion + s->d_bitmap_ptr);
    ssize_t idx = bitmap_alloc(dbm, s->num_data_blocks);
    if (idx < 0) return 0; // 0 marks failure
    return s->d_blocks_ptr + idx * BLOCK_SIZE;
}

static void free_inode(struct wfs_inode* inode) {
    struct wfs_sb* s = sb();
    uint32_t* ibm = (uint32_t*)((char*)mregion + s->i_bitmap_ptr);
    bitmap_clear(ibm, inode->num);
    memset(inode, 0, BLOCK_SIZE);
}

static void free_block(off_t blk_offset) {
    struct wfs_sb* s = sb();
    off_t rel = (blk_offset - s->d_blocks_ptr) / BLOCK_SIZE;
    uint32_t* dbm = (uint32_t*)((char*)mregion + s->d_bitmap_ptr);
    bitmap_clear(dbm, (uint32_t)rel);
    memset((char*)mregion + blk_offset, 0, BLOCK_SIZE);
}

/* Return pointer to file offset; alloc if requested. Supports direct + single indirect. */
static char* data_offset(struct wfs_inode* inode, off_t offset, int alloc) {
    int file_block = offset / BLOCK_SIZE;
    if (file_block > D_BLOCK + (BLOCK_SIZE/sizeof(off_t))) { wfs_error = -EFBIG; return NULL; }
    off_t* arr;
    if (file_block > D_BLOCK) { // indirect region
        file_block -= D_BLOCK; // adjust index
        if (inode->blocks[IND_BLOCK] == 0) {
            if (!alloc) { wfs_error = -ENOENT; return NULL; }
            inode->blocks[IND_BLOCK] = allocate_data_block();
            if (inode->blocks[IND_BLOCK] == 0) { wfs_error = -ENOSPC; return NULL; }
        }
        arr = (off_t*)((char*)mregion + inode->blocks[IND_BLOCK]);
    } else {
        arr = inode->blocks;
    }
    if (alloc && arr[file_block] == 0) {
        arr[file_block] = allocate_data_block();
        if (arr[file_block] == 0) { wfs_error = -ENOSPC; return NULL; }
    }
    if (arr[file_block] == 0) { wfs_error = -ENOENT; return NULL; }
    return (char*)mregion + arr[file_block] + (offset % BLOCK_SIZE);
}

static void fill_inode(struct wfs_inode* inode, mode_t mode) {
    inode->mode = mode;
    inode->uid  = getuid();
    inode->gid  = getgid();
    inode->size = 0;
    inode->nlinks = 1;
    memset(inode->blocks, 0, sizeof(inode->blocks));
    /* TODO PART 3: Initialize time fields to now. */
    /* TODO PART 4: Initialize color = none. */
}

/* Directory entry management (simplified; no deallocation of directory blocks) */
static int add_dentry(struct wfs_inode* dir, int inum, const char* name) {
    off_t off = 0; struct wfs_dentry* dent;
    while (off < dir->size) {
        dent = (struct wfs_dentry*)data_offset(dir, off, 0);
        if (dent->num == 0) {
            dent->num = inum; strncpy(dent->name, name, MAX_NAME);
            dir->nlinks++; // optional for directories
            // TODO PART 3: update directory mtime/ctime on metadata change
            return 0;
        }
        off += sizeof(struct wfs_dentry);
    }
    // Need a new block
    dent = (struct wfs_dentry*)data_offset(dir, dir->size, 1);
    if (!dent) return wfs_error;
    memset(dent, 0, BLOCK_SIZE);
    dent->num = inum; strncpy(dent->name, name, MAX_NAME);
    dir->size += BLOCK_SIZE;
    // TODO PART 3: update directory mtime/ctime on growth
    return 0;
}

static int remove_dentry(struct wfs_inode* dir, int inum) {
    for (off_t off=0; off<dir->size; off+=sizeof(struct wfs_dentry)) {
        struct wfs_dentry* dent = (struct wfs_dentry*)data_offset(dir, off, 0);
        if (dent->num == inum) { dent->num = 0; /* TODO PART 3: bump mtime/ctime */ return 0; }
    }
    return -ENOENT;
}

/* Simplistic path resolution: absolute paths only; no color-filter virtual dirs yet. */
static int path_lookup(const char* path, struct wfs_inode** out) {
    if (strcmp(path, "/") == 0) { *out = retrieve_inode(0); return *out?0:-ENOENT; }
    if (path[0] != '/') return -ENOENT;
    struct wfs_inode* cur = retrieve_inode(0);
    if (!cur) return -ENOENT;
    char* p = strdup(path); char* seg = strtok(p+1, "/");
    while (seg) {
        int found = -1;
        for (off_t off=0; off<cur->size; off+=sizeof(struct wfs_dentry)) {
            struct wfs_dentry* dent = (struct wfs_dentry*)data_offset(cur, off, 0);
            if (dent->num != 0 && strncmp(dent->name, seg, MAX_NAME) == 0) { found = dent->num; break; }
        }
        if (found < 0) { free(p); return -ENOENT; }
        cur = retrieve_inode(found);
        if (!cur) { free(p); return -ENOENT; }
        seg = strtok(NULL, "/");
    }
    free(p); *out = cur; return 0;
}

/* --------------------------- FUSE Operations ------------------------------ */
static int wfs_getattr(const char* path, struct stat* st) {
    struct wfs_inode* inode; if (path_lookup(path, &inode) < 0) return -ENOENT;
    st->st_mode = inode->mode; st->st_uid = inode->uid; st->st_gid = inode->gid;
    st->st_size = inode->size; st->st_nlink = inode->nlinks;
    // TODO PART 3: return real times once you add them to inode
    st->st_atime = 0; st->st_mtime = 0; st->st_ctime = 0;
    return 0;
}

/* TODO PART 1: implement create (mknod), mkdir, read, write, readdir, unlink/rmdir */
static int wfs_mknod(const char* path, mode_t mode, dev_t dev) { (void)dev; /* TODO */ return -ENOSYS; }
static int wfs_mkdir(const char* path, mode_t mode) { /* TODO */ return -ENOSYS; }
static int wfs_read(const char* path, char* buf, size_t len, off_t off, struct fuse_file_info* fi){ (void)fi; /* TODO */ return -ENOSYS; }
static int wfs_write(const char* path, const char* buf, size_t len, off_t off, struct fuse_file_info* fi){ (void)fi; /* TODO */ return -ENOSYS; }
static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info* fi){ (void)off;(void)fi; /* TODO */ return -ENOSYS; }
static int wfs_unlink(const char* path){ /* TODO */ return -ENOSYS; }
static int wfs_rmdir(const char* path){ /* TODO */ return -ENOSYS; }

/* TODO PART 2: statfs implementation */
static int wfs_statfs(const char* path, struct statvfs* st){ (void)path; /* TODO */ return -ENOSYS; }

/* TODO PART 3: ensure time updates in read/write/readdir/add/remove operations */
/* Atime: successful read of file data or directory list; Mtime/Ctime: upon content/metadata change */

/* TODO PART 4: xattr user.color + colored names when process name == "ls" */
static int wfs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags){ (void)path;(void)name;(void)value;(void)size;(void)flags; return -EOPNOTSUPP; }
static int wfs_getxattr(const char* path, const char* name, char* value, size_t size){ (void)path;(void)name;(void)value;(void)size; return -EOPNOTSUPP; }
static int wfs_removexattr(const char* path, const char* name){ (void)path;(void)name; return -EOPNOTSUPP; }

static struct fuse_operations wfs_ops = {
    .getattr = wfs_getattr,
    .mknod   = wfs_mknod,
    .mkdir   = wfs_mkdir,
    .read    = wfs_read,
    .write   = wfs_write,
    .readdir = wfs_readdir,
    .unlink  = wfs_unlink,
    .rmdir   = wfs_rmdir,
    .statfs  = wfs_statfs,
    .setxattr = wfs_setxattr,
    .getxattr = wfs_getxattr,
    .removexattr = wfs_removexattr,
};

/* ------------------------------ Mount Entry ------------------------------- */
int main(int argc, char* argv[]) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <disk.img> [FUSE opts]\n", argv[0]); return 1; }
    const char* img = argv[1];
    int fd = open(img, O_RDWR); if (fd < 0) { perror("open disk"); return 1; }
    struct stat sbuf; if (fstat(fd, &sbuf) < 0) { perror("stat"); return 1; }
    mregion = mmap(NULL, sbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (mregion == MAP_FAILED) { perror("mmap"); return 1; }
    assert(retrieve_inode(0) != NULL && "Root inode must exist (mkfs must create it)");

    // Shift FUSE args left so disk path is hidden from fuse_main.
    for (int i=2;i<argc;i++) argv[i-1] = argv[i]; argc--;
    int ret = fuse_main(argc, argv, &wfs_ops, NULL);
    munmap(mregion, sbuf.st_size); close(fd); return ret;
}
