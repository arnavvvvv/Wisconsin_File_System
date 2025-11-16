#define FUSE_USE_VERSION 30
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <fuse.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "wfs.h"

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

/* --------------------------- Globals / Mount ------------------------------ */
void *mregion; // mapped disk image
int wfs_error; // last error to return through FUSE

struct color_entry { const char *name; uint8_t code; };
static const struct color_entry color_table[] = {
    {"none",    WFS_COLOR_NONE},
    {"red",     WFS_COLOR_RED},
    {"green",   WFS_COLOR_GREEN},
    {"blue",    WFS_COLOR_BLUE},
    {"yellow",  WFS_COLOR_YELLOW},
    {"magenta", WFS_COLOR_MAGENTA},
    {"cyan",    WFS_COLOR_CYAN},
    {"white",   WFS_COLOR_WHITE},
    {"black",   WFS_COLOR_BLACK},
    {"orange",  WFS_COLOR_ORANGE},
    {"purple",  WFS_COLOR_PURPLE},
    {"gray",    WFS_COLOR_GRAY},
};

int parse_color_name(const char *s, uint8_t *out_code) {
    if (!s || !out_code) return 0;
    char buf[32]; size_t n = 0;
    while (s[n] && n + 1 < sizeof(buf)) { buf[n] = (char)tolower((unsigned char)s[n]); n++; }
    buf[n] = '\0';
    for (size_t i = 0; i < sizeof(color_table)/sizeof(color_table[0]); i++) {
        if (strcmp(buf, color_table[i].name) == 0) { *out_code = color_table[i].code; return 1; }
    }
    return 0;
}

/* Return the color name decorated with ANSI escape codes so terminals
 * show the name itself in that color. Note: this means any consumer
 * of the xattr will receive the escape sequences. If you want raw
 * names for scripting, keep a separate helper returning undecorated
 * names. */
typedef struct { const char *ansi; const char *name; } wfs_color_info;

static inline const wfs_color_info* wfs_color_from_code(uint8_t code) {
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

// Helper: strip ANSI color sequences from names (useful when colorizing ls output)
void strip_ansi_codes(const char *in, char *out, size_t out_len)
{
    /*TODO*/
}

int get_inode_from_path(char *path, struct wfs_inode **inode)
{
    /* TODO: Resolve absolute paths by splitting on '/' and walking from the root inode.
     * For each component, scan the current directory's dentries (via data_offset) to find the child inode;
     * on missing component return -ENOENT, otherwise set *inode to the final inode and return 0. */
    (void)path; (void)inode;
    return 0;

}
void free_bitmap(uint32_t position, uint32_t* bitmap) {
    /*TODO: Clear the bit at 'position' in the bitmap */
}

struct wfs_inode *retrieve_inode(int inum) {
    (void)inum;
    /* TODO:
     * Use superblock fields (i_blocks_ptr, BLOCK_SIZE stride) to compute a pointer to inode 'inum
     * Also validate 'inum' via the inode bitmap before returning. */
    return NULL;
}

ssize_t allocate_block(uint32_t* bitmap, size_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t bm_region = bitmap[i];
        if (bm_region == 0xFFFFFFFF) {
            continue;
        }
        for (uint32_t k = 0; k < 32; k++) {
            if (!((bm_region >> k) & 0x1)) { // it is free
                // allocate
                bitmap[i] = bitmap[i] | (0x1 << k);
                return 32*i + k;
                //return block_region + (BLOCK_SIZE * (32*i + k));
            }
        }
    }
    return -1; // no free blocks found
}

struct wfs_inode *allocate_inode(void) {
    /* TODO: Allocate an inode slot by marking the inode bitmap and return a
     * pointer to the inode block within the mapped image (or NULL on failure). */
    wfs_error = -ENOSPC;
    return NULL;
}

off_t allocate_data_block(void) {
    /* TODO: Use the data bitmap to allocate a free data block and return its
     * on-disk byte OFFSET. Handle error appropriately. */
    return 0;
}

void free_inode(struct wfs_inode *inode) {
    (void)inode;
    /* TODO: Clear the inode bitmap entry and zero the inode block. */
}

void free_block(off_t blk_offset) {
    (void)blk_offset;
    /* TODO: Mark the data block free in the data bitmap and zero it. */
}

/* Return pointer to file offset; alloc if requested. Supports direct + single indirect. */
char *data_offset(struct wfs_inode *inode, off_t offset, int alloc) {
    (void)inode; (void)offset; (void)alloc;
    /*
    - Translate a file byte offset into a location within the on-disk storage.
    - Support the inode’s addressing model (direct blocks plus a single level of indirection).
    - Enforce capacity limits and report errors appropriately.
    - Optionally provision storage for missing pieces when requested.
    - Return a pointer into the mapped image at the resolved location within a block.
    */
    wfs_error = -ENOSPC;
    return NULL;
}

void fillin_inode(struct wfs_inode* inode, mode_t mode)
{
    inode->mode = mode;
    inode->uid = getuid();
    inode->gid = getgid();
    inode->size = 0;
    inode->nlinks = 1;
    memset(inode->blocks, 0, sizeof(inode->blocks));
    /* TODO PART 3: Initialize time fields to now. */
    /* TODO PART 4: Initialize color = none. */
}

int add_dentry(struct wfs_inode* parent, int num, char* name)
{
   /*TODO: insert dentry if there is an empty slot.
    We will not do indirect blocks with directories*/
    return 0;
}

int remove_dentry(struct wfs_inode *dir, int inum)
{
    /*TODO: Use inode 0 as a "deleted" inode. 
    So any directory entry could be marked as 0 to indicate it is deleted. 
    Removed dentries can result in "holes" in the dentry list, thus it is
    important to use the first available slot in add_dentry() */
    return 0;
}

/* --------------------------- FUSE Operations ------------------------------ */
int wfs_getattr(const char *path, struct stat *st)
{
    /* TODO */
    return -ENOSYS;
}
int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    (void)dev; /* TODO */
    return -ENOSYS;
}
int wfs_mkdir(const char *path, mode_t mode)
{ 
    /* TODO */
    return -ENOSYS;
}
int wfs_read(const char *path, char *buf, size_t len, off_t off, struct fuse_file_info *fi)
{
    (void)fi; /* TODO */
    return -ENOSYS;
}
int wfs_write(const char *path, const char *buf, size_t len, off_t off, struct fuse_file_info *fi)
{
    (void)fi; /* TODO */
    return -ENOSYS;
}
int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi)
{
    (void)off;
    (void)fi; /* TODO */
    return -ENOSYS;
}
int wfs_unlink(const char *path)
{ 
    /* TODO */
    return -ENOSYS;
}
int wfs_rmdir(const char *path)
{ 
    /* TODO */
    return -ENOSYS;
}

/* TODO PART 2: statfs implementation */
int wfs_statfs(const char *path, struct statvfs *st)
{
    (void)path; /* TODO */
    return -ENOSYS;
}

/* TODO PART 3: ensure time updates in read/write/readdir/add/remove operations
Atime: successful read of file data or directory list;
Mtime/Ctime: upon content/metadata change */

/* TODO PART 4: xattr user.color + colored names when process name == "ls" */
int wfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    return -ENOSYS;
}
int wfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    return -ENOSYS;
}
int wfs_removexattr(const char *path, const char *name)
{
    (void)path;
    (void)name;
    return -ENOSYS;
}

static struct fuse_operations wfs_ops = {
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .read = wfs_read,
    .write = wfs_write,
    .readdir = wfs_readdir,
    .unlink = wfs_unlink,
    .rmdir = wfs_rmdir,
    .statfs = wfs_statfs,
    .setxattr = wfs_setxattr,
    .getxattr = wfs_getxattr,
    .removexattr = wfs_removexattr,
};

/* ------------------------------ Mount Entry ------------------------------- */
int main(int argc, char *argv[])
{
    int fuse_stat;
    struct stat sb;
    int fd;
    char* diskimage = strdup(argv[1]);

    // shift args down by one for fuse
    for (int i = 2; i < argc; i++) {
        argv[i-1] = argv[i];
    }
    argc -= 1;

    // open the file
    if ((fd = open(diskimage, O_RDWR, 0666)) < 0) {
        perror("open failed main\n");
        return 1;
    }

    // stat so we know how large the mmap needs to be
    if (fstat(fd, &sb) < 0) {
        perror("stat");
        return 1;
    }

    // setup mmap
    mregion = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mregion == NULL) {
        printf("error mmaping file\n");
        return 1;
    }

    assert(retrieve_inode(0) != NULL);
    fuse_stat = fuse_main(argc, argv, &wfs_ops, NULL);

    munmap(mregion, sb.st_size);
    close(fd);
    return fuse_stat;
}
