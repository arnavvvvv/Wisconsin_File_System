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

#define MMAP_PTR(offset) ((char*)mregion + offset)

void* mregion;
int wfs_error;

// =========================
// Color tag helpers (enum-based palette)
// =========================
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

static int parse_color_name(const char *s, uint8_t *out_code) {
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

static void strip_ansi_codes(const char* path, char* clean_path_out, size_t out_len) {
    const char* src = path;
    char* dst = clean_path_out;
    char* const end = clean_path_out + out_len - 1; // leave room for NUL

    while (*src && dst < end) {
        if (*src == '\033') {
            // Skip ANSI escape sequence: \033[...m
            src++;
            if (*src == '[') {
                src++;
                while (*src && *src != 'm') src++;
                if (*src == 'm') src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}
// presume inode is a directory
// return inode number corresponding to dentry name
int dentry_to_num(char* name, struct wfs_inode* inode) {
    char clean_name[1024]; // Use a local, stack-allocated buffer
    strip_ansi_codes(name, clean_name, sizeof(clean_name));
    size_t sz = inode->size;
    struct wfs_dentry* dent;
    
    for (off_t off = 0; off < sz; off += sizeof(struct wfs_dentry)) {
        dent = (struct wfs_dentry*)data_offset(inode, off, 0);

        if (dent->num != 0 && !strcmp(dent->name, clean_name)) { // match
            return dent->num;
        }
    }
    return -1;
}

int get_inode_rec(struct wfs_inode* enclosing, char* path, struct wfs_inode** inode) {
    if (!strcmp(path, "")) {
        *inode = enclosing;
        return 0;
    }

    char* next = path;
    while (*path != '/' && *path != '\0') {
        path++;
    }
    if (*path != '\0') {
        *path++ = '\0';
    }

    int inum = dentry_to_num(next, enclosing);
    if (inum < 0) {
        wfs_error = -ENOENT;
        return -1;
    }
    return get_inode_rec(retrieve_inode(inum), path, inode);
}

int get_inode_from_path(char* path, struct wfs_inode** inode) {
    // all paths must start at root, thus path+1 is safe
    char clean[1024]; // Use a local, stack-allocated buffer
    strip_ansi_codes(path, clean, sizeof(clean));
    char* clean_copy = strdup(clean);
    int result = get_inode_rec(retrieve_inode(0), clean_copy + 1, inode);
    free(clean_copy);
    return result;
}

int wfs_mknod(const char* path, mode_t mode, dev_t dev) {
    (void)dev;
    printf("wfs_mknod: %s\n", path);

    struct wfs_inode* parent_inode = NULL;
    char *base = strdup(path);
    char *name = strdup(path);
    
    if (get_inode_from_path(dirname(base), &parent_inode) < 0) {
        return wfs_error;
    }

    struct wfs_inode *inode = allocate_inode();
    if (inode == NULL) { return wfs_error; }
    fillin_inode(inode, S_IFREG | mode);

    // add dentry to parent
    if (add_dentry(parent_inode, inode->num, basename(name)) < 0) {
        return wfs_error;
    }

    free(base);
    free(name);
    return 0;
}

int add_dentry(struct wfs_inode* parent, int num, char* name) {

    // Make sure we only store the clean name on disk
    char clean_name[MAX_NAME];
    strip_ansi_codes(name, clean_name, sizeof(clean_name));

    // insert dentry if there is an empty slot
    int numblks = parent->size / BLOCK_SIZE;
    struct wfs_dentry* dent;
    off_t offset = 0;
    
    while (offset < parent->size) {
        dent = (struct wfs_dentry*)data_offset(parent, offset, 0);

        if (dent->num == 0) {
            dent->num = num;
            strncpy(dent->name, name, MAX_NAME);
            parent->nlinks += 1;
            // update directory mtime/ctime because its entries changed
            struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
            parent->mtim = now.tv_sec; parent->ctim = now.tv_sec;
            return 0;
        }
        offset += sizeof(struct wfs_dentry);
    }

    // careful this will not work with indirect blocks for now
    // We will not do indirect blocks with directories
    dent = (struct wfs_dentry*)data_offset(parent, numblks*BLOCK_SIZE, 1);
    if (!dent) {
        return -1;
    }
    dent->num = num;
    strncpy(dent->name, name, MAX_NAME);
    parent->nlinks += 1;
    parent->size += BLOCK_SIZE;
    // directory grew: update mtime/ctime
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
    parent->mtim = now.tv_sec; parent->ctim = now.tv_sec;

    return 0;
}

void fillin_inode(struct wfs_inode* inode, mode_t mode) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    
    inode->mode = mode;
    inode->uid = getuid();
    inode->gid = getgid();
    inode->size = 0;
    inode->nlinks = 1;
    inode->atim = t.tv_sec;
    inode->mtim = t.tv_sec;
    inode->ctim = t.tv_sec;
    inode->color = WFS_COLOR_NONE; // default: no color
}

int wfs_mkdir(const char* path, mode_t mode) {
    printf("wfs_mkdir: %s\n", path);

    struct wfs_inode* parent_inode = NULL;
    
    char *base = strdup(path);
    char *name = strdup(path);

    if (get_inode_from_path(dirname(base), &parent_inode) < 0) {
        return wfs_error;
    }
    struct wfs_inode* inode = allocate_inode();
    if (inode == NULL) { return wfs_error; }
    fillin_inode(inode, S_IFDIR | mode);

    // add dentry to parent
    if (add_dentry(parent_inode, inode->num, basename(name)) < 0) {
        return wfs_error;
    }

    free(name);
    free(base);

    return 0;
}

int wfs_getattr(const char* path, struct stat *statbuf) {
    printf("wfs_getattr: %s\n", path);
    struct wfs_inode* inode;
    char* searchpath = strdup(path);
    if (get_inode_from_path(searchpath, &inode) < 0) {
        return wfs_error;
    }
    
    statbuf->st_mode = inode->mode;
    statbuf->st_uid  = inode->uid;
    statbuf->st_gid  = inode->gid;
    statbuf->st_size = inode->size;
    statbuf->st_atime = inode->atim;
    statbuf->st_mtime = inode->mtim;
    statbuf->st_ctime = inode->ctim;
    statbuf->st_nlink = inode->nlinks;

    free(searchpath);
    return 0;
}

// =========================
// xattr: expose color tag as "user.color"
// =========================
static int wfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
    (void)flags;
    if (!path || !name) return -EINVAL;
    if (strcmp(name, "user.color") != 0) return -EOPNOTSUPP;
    struct wfs_inode *inode; char *p = strdup(path);
    if (get_inode_from_path(p, &inode) < 0) { free(p); return wfs_error; }
    // value may not be NUL-terminated; ensure it is
    char valbuf[64];
    size_t n = size < sizeof(valbuf)-1 ? size : sizeof(valbuf)-1;
    if (value && n > 0) memcpy(valbuf, value, n);
    valbuf[n] = '\0';

    uint8_t code;
    if (!parse_color_name(valbuf, &code)) { free(p); return -EINVAL; }
    inode->color = code;
    inode->ctim = time(NULL);
    free(p);
    return 0;
}

static int wfs_getxattr(const char *path, const char *name, char *value, size_t size) {
    if (!path || !name) return -EINVAL;
    if (strcmp(name, "user.color") != 0) return -EOPNOTSUPP;
    struct wfs_inode *inode; char *p = strdup(path);
    if (get_inode_from_path(p, &inode) < 0) { free(p); return wfs_error; }

    const wfs_color_info *ci = wfs_color_from_code(inode->color);
    const char *name_out = ci->name;
    size_t need = strlen(name_out) + 1;
    if (size == 0 || value == NULL) { free(p); return (int)need; }
    if (size < need) { free(p); return -ERANGE; }
    memcpy(value, name_out, need);

    free(p);
    return (int)need;
}

static int wfs_listxattr(const char *path, char *list, size_t size) {
    (void)path; // we always expose user.color
    const char *attr = "user.color";
    size_t need = strlen(attr) + 1; // null-terminated list entry
    if (size == 0 || list == NULL) return (int)need;
    if (size < need) return -ERANGE;
    memcpy(list, attr, need);
    return (int)need;
}

static int wfs_removexattr(const char *path, const char *name) {
    printf("wfs_removexattr: %s %s\n", path, name);
    if (!path || !name) return -EINVAL;
    if (strcmp(name, "user.color") != 0) return -EOPNOTSUPP;
    struct wfs_inode *inode; char *p = strdup(path);
    if (get_inode_from_path(p, &inode) < 0) { free(p); return wfs_error; }
    inode->color = 0; // none
    inode->ctim = time(NULL);
    free(p);
    return 0;
}

// removes a dentry from the directory inode
// if this results in an empty data block, we will not deallocate it.
// removed dentries can result in "holes" in the dentry list, thus it
// is important to use the first available slot in add_dentry()
int remove_dentry(struct wfs_inode* inode, int inum) {
    size_t sz = inode->size;
    struct wfs_dentry* dent;

    for (off_t off = 0; off < sz; off += sizeof(struct wfs_dentry)) {
        dent = (struct wfs_dentry*)data_offset(inode, off, 0);
        
        if (dent->num == inum) { // match
            dent->num = 0;
            // directory entries changed: update mtime/ctime
            struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
            inode->mtim = now.tv_sec; inode->ctim = now.tv_sec;
            return 0;
        }
    }
    return -1; // not found
}

// returns a pointer to offset for this inode
// be careful, won't work well if reading across block boundaries
// dirents are guaranteed to not cross block boundaries
char* data_offset(struct wfs_inode* inode, off_t offset, int alloc) {
    int blocknum = offset / BLOCK_SIZE;
    off_t* blks_arr;

    // out of range
    if (blocknum > D_BLOCK + (BLOCK_SIZE / sizeof(off_t))) {
        printf("data_offset() bad blocknum, too big!\n");
        return NULL;
    } else if (blocknum > D_BLOCK) { // indirect block
        blocknum -= IND_BLOCK;
        if (inode->blocks[IND_BLOCK] == 0) {
            inode->blocks[IND_BLOCK] = allocate_data_block();
        }
        blks_arr = (off_t*)MMAP_PTR(inode->blocks[IND_BLOCK]);
    } else { // direct block
        blks_arr = inode->blocks;
    }

    if (alloc && *(blks_arr + blocknum) == 0) {
        *(blks_arr + blocknum) = allocate_data_block();
    }
    if (*(blks_arr + blocknum) == 0) {
        if (alloc) { wfs_error = -ENOSPC; }
        return NULL;
    }
    
    return MMAP_PTR(blks_arr[blocknum]) + (offset % BLOCK_SIZE);
}

int wfs_read(const char* path, char *buf, size_t length, off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    printf("wfs_read: %s\n", path);
    struct wfs_inode* inode;
    char* searchpath = strdup(path);
    if (get_inode_from_path(searchpath, &inode) < 0) {
        return wfs_error;
    }
    size_t have_read = 0;
    size_t pos = offset;

    while (have_read < length && pos < inode->size) {
        size_t to_read = BLOCK_SIZE - (pos % BLOCK_SIZE);
        // length might be larger than file or block size
        size_t eof = inode->size - pos; 
        if (to_read > eof) { to_read = eof; }

        char* addr = data_offset(inode, pos, 0);
        memcpy(buf + have_read, addr, to_read);
        pos += to_read;
        have_read += to_read;
    }

    free(searchpath);
    // Update atime only if we actually read some bytes (POSIX allows updating on any access; this avoids pure EOF bumps)
    if (have_read > 0) {
        struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
        inode->atim = now.tv_sec;
    }
// inode->ctim = now.tv_sec; // optional

    return have_read;
}

int wfs_write(const char *path, const char *buf, size_t length, off_t offset, struct fuse_file_info *fi) {
    (void)fi;
    printf("wfs_write: %s\n", path);
    struct wfs_inode* inode;
    char* searchpath = strdup(path);
    if (get_inode_from_path(searchpath, &inode) < 0) {
        return wfs_error;
    }

    // size will be increased by the amount of data we add to the file
    ssize_t newdatalen = length - (inode->size - offset);

    size_t have_written = 0;
    size_t pos = offset;

    while (have_written < length) {
        size_t to_write = BLOCK_SIZE - (pos % BLOCK_SIZE);
        if (to_write + have_written > length) {
            to_write = length - have_written;
        }

        char* addr = data_offset(inode, pos, 1);
        if (addr == NULL) {
            return wfs_error;
        }
        memcpy(addr, buf + have_written, to_write);
        pos += to_write;
        have_written += to_write;
    }

    inode->size += newdatalen > 0 ? newdatalen : 0;
    // Writing updates mtime and ctime
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
    inode->mtim = now.tv_sec; inode->ctim = now.tv_sec;
    free(searchpath);
    return have_written;
}

int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    (void)fi;
    (void)offset;

    printf("wfs_readdir: %s\n", path);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
    struct wfs_inode* inode;
    char* searchpath = strdup(path);
    if (get_inode_from_path(searchpath, &inode) < 0) {
        return wfs_error;
    }

    // Detect if caller is 'ls' by checking process name
    struct fuse_context *ctx = fuse_get_context();
    int is_ls = 0;
    if (ctx) {
        char comm_path[64], comm_name[256];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", ctx->pid);
        FILE *fp = fopen(comm_path, "r");
        if (fp) {
            if (fgets(comm_name, sizeof(comm_name), fp)) {
                // Remove trailing newline
                comm_name[strcspn(comm_name, "\n")] = '\0';
                printf("DEBUG: comm_name = %s\n", comm_name);
                is_ls = (strcmp(comm_name, "ls") == 0);
            }
            fclose(fp);
        }
    }
    printf("DEBUG: is_ls = %d\n", is_ls); 
    size_t sz = inode->size;
    struct wfs_dentry* dent;
        
    for (off_t off = 0; off < sz; off += sizeof(struct wfs_dentry)) {
        dent = (struct wfs_dentry*)data_offset(inode, off, 0);

        if (dent->num != 0) {
            struct wfs_inode *file_inode = retrieve_inode(dent->num);
            printf("DEBUG: file %s, color = %d\n", dent->name, file_inode ? file_inode->color : -1);
            if (is_ls && file_inode && file_inode->color != WFS_COLOR_NONE) {
                const wfs_color_info *ci = wfs_color_from_code(file_inode->color);
                char colored_name[MAX_NAME + 64];
                snprintf(colored_name, sizeof(colored_name), "%s%s\033[0m",
                         ci->ansi, dent->name);
                printf("DEBUG: returning colored name: %s\n", colored_name);
                filler(buf, colored_name, NULL, 0);
            } else {
                filler(buf, dent->name, NULL, 0);
            }
        }
    }

    free(searchpath);
    // Reading a directory updates its atime
    struct wfs_inode* dir_inode;
    char* p2 = strdup(path);
    if (get_inode_from_path(p2, &dir_inode) == 0) {
        struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
        dir_inode->atim = now.tv_sec;
    }
    free(p2);
    return 0;
}

int wfs_unlink(const char* path)
{
    printf("wfs_unlink: %s\n", path);
    struct wfs_inode* parent_inode;
    struct wfs_inode* inode;
    char* base = strdup(path);
    char* searchpath = strdup(path);
    
    // parent inode
    if (get_inode_from_path(dirname(base), &parent_inode) < 0) {
        return wfs_error;
    }
    
    // inode
    if (get_inode_from_path(searchpath, &inode) < 0) {
        return wfs_error;
    }

    // free all the data blocks
    off_t* blks_arr;
    if (inode->blocks[IND_BLOCK] != 0) { // free indirect blocks
        blks_arr = (off_t*)MMAP_PTR(inode->blocks[IND_BLOCK]);
        for (int i = 0; i < BLOCK_SIZE / sizeof(off_t); i++) {
            if (blks_arr[i] != 0) { free_block(blks_arr[i]); }
        }
    }
    blks_arr = inode->blocks;
    for (int i = 0; i < N_BLOCKS; i++) { // free all the direct blocks
        if (blks_arr[i] != 0) { free_block(blks_arr[i]); }
    }
        
    // remove dentry from parent
    remove_dentry(parent_inode, inode->num);

    // free inode
    // Update parent's ctime/mtime already handled in remove_dentry; set inode's ctime to now before freeing (for completeness if observed)
    struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
    inode->ctim = now.tv_sec;
    free_inode(inode);

    free(base);
    free(searchpath);
    return 0;
}

int wfs_rmdir(const char *path)
{
    printf("wfs_rmdir: %s\n", path);
    // wfs_unlink updates parent directory times; rmdir should also adjust parent atime (access) minimally handled by getattr/read elsewhere
    wfs_unlink(path);
    return 0;
}

static int wfs_statfs(const char *path, struct statvfs *st) {
    (void)path;
    struct wfs_sb* sb = (struct wfs_sb*)mregion;
    memset(st, 0, sizeof(*st));

    st->f_bsize = BLOCK_SIZE;
    st->f_frsize = BLOCK_SIZE;
    st->f_blocks = sb->num_data_blocks;
    st->f_files  = sb->num_inodes;

    uint32_t* i_bm = (uint32_t*)((char*)mregion + sb->i_bitmap_ptr);
    uint32_t* d_bm = (uint32_t*)((char*)mregion + sb->d_bitmap_ptr);

    int used_inodes = 0, used_blocks = 0;
    for (size_t i = 0; i < sb->num_inodes / 32; i++)
        used_inodes += __builtin_popcount(i_bm[i]);
    for (size_t i = 0; i < sb->num_data_blocks / 32; i++)
        used_blocks += __builtin_popcount(d_bm[i]);

    st->f_bfree = sb->num_data_blocks - used_blocks;
    st->f_bavail = st->f_bfree;
    st->f_ffree = sb->num_inodes - used_inodes;
    st->f_namemax = MAX_NAME;

    return 0;
}

static struct fuse_operations wfs_oper = {
  .getattr = wfs_getattr,
  .mknod = wfs_mknod,
  .mkdir = wfs_mkdir,
  .unlink = wfs_unlink,
  .rmdir = wfs_rmdir,
  .read = wfs_read,
  .write = wfs_write,
  .readdir = wfs_readdir,
  .statfs = wfs_statfs,
    .setxattr = wfs_setxattr,
    .getxattr = wfs_getxattr,
    .listxattr = wfs_listxattr,
    .removexattr = wfs_removexattr,
};

void free_bitmap(uint32_t position, uint32_t* bitmap) {
    int b = position / 32;
    int p = position % 32;
    bitmap[b] = bitmap[b] ^ (0x1 << p);
}

// we choose to zero blocks and inodes as they are freed because some
// functions depend on fields being zero-initialized (e.g. finding an
// empty directory entry slot.) If blocks are freed and reused, garbage
// data could affect correctness.

void free_block(off_t blk) {
    struct wfs_sb* sb = (struct wfs_sb*)mregion;
    memset(MMAP_PTR(blk), 0, BLOCK_SIZE); // zero

    free_bitmap(/*position*/ (blk - sb->d_blocks_ptr) / BLOCK_SIZE,
                /*bitmap*/ (uint32_t*)MMAP_PTR(sb->d_bitmap_ptr));
}

void free_inode(struct wfs_inode* inode) {
    struct wfs_sb* sb = (struct wfs_sb*)mregion;
    memset((char*)inode, 0, BLOCK_SIZE); // zero

    free_bitmap(/*position*/ ((char*)inode - MMAP_PTR(sb->i_blocks_ptr)) / BLOCK_SIZE,
                /*bitmap*/ (uint32_t*)MMAP_PTR(sb->i_bitmap_ptr));
}

struct wfs_inode* retrieve_inode(int num) {
    struct wfs_sb* sb = (struct wfs_sb*)mregion;
    uint32_t* bitmap = (uint32_t*)MMAP_PTR(sb->i_bitmap_ptr);

    int b = num / 32;
    int p = num % 32;
    // check if it is allocated first
    if (bitmap[b] & (0x1 << p)) {
        return (struct wfs_inode*)(MMAP_PTR(sb->i_blocks_ptr) + num*BLOCK_SIZE);
    }
    return NULL;
}

// careful - block allocations are always stored by their offsets 
// to use a block, we add the block address (i.e. offset) to mregion
// we don't store pointers in the inode as they are not persistent across fs reboot.
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
//                return block_region + (BLOCK_SIZE * (32*i + k));
            }
        }
    }
    return -1; // no free blocks found
}

off_t allocate_data_block(void) {
    struct wfs_sb* sb = (struct wfs_sb*)mregion;
    off_t blknum = allocate_block((uint32_t*)MMAP_PTR(sb->d_bitmap_ptr),
                                  sb->num_data_blocks / 32);
    if (blknum < 0) {
        return 0;
    }
       
    return sb->d_blocks_ptr + BLOCK_SIZE * blknum;
}

struct wfs_inode* allocate_inode(void) {
    struct wfs_sb* sb = (struct wfs_sb*)mregion;
    off_t blknum = allocate_block((uint32_t*)MMAP_PTR(sb->i_bitmap_ptr),
                                  sb->num_inodes / 32);
    if (blknum < 0) { 
        wfs_error = -ENOSPC;
        return NULL;
    }
    struct wfs_inode* inode = (struct wfs_inode*)(MMAP_PTR(sb->i_blocks_ptr) + BLOCK_SIZE * blknum);
    inode->num = blknum;
    return inode;
}

int main(int argc, char* argv[]) {
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
    fuse_stat = fuse_main(argc, argv, &wfs_oper, NULL);

    munmap(mregion, sb.st_size);
    close(fd);
    return fuse_stat;
}
