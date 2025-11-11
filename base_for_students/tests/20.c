#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common/test.h"

/* Test 20: creation times (mknod/mkdir) set atime==mtime==ctime and parent directory mtime/ctime bump */
int main() {
  int ret;
  // Create a file and directory and then stat them
  CHECK(create_file("mnt/t20_file"));
  int fd = ret; CHECK(close_file(fd));
  CHECK(create_dir("mnt/t20_dir"));

  struct stat st_file, st_dir, st_root_before, st_root_after;
  // Stat root before further modification (after creations root already changed once)
  CHECK(open_file_read("mnt")); // open to ensure mount exists
  int dirfd = ret; CHECK(close_file(dirfd));
  if (stat("mnt", &st_root_before) != 0) { perror("stat root before"); return FAIL; }
  if (stat("mnt/t20_file", &st_file) != 0) { perror("stat file"); return FAIL; }
  if (stat("mnt/t20_dir", &st_dir) != 0) { perror("stat dir"); return FAIL; }

  // Basic invariants on creation
  if (!(st_file.st_atime == st_file.st_mtime && st_file.st_mtime == st_file.st_ctime)) {
    printf("File initial times not identical: at=%ld mt=%ld ct=%ld\n", st_file.st_atime, st_file.st_mtime, st_file.st_ctime); return FAIL; }
  if (!(st_dir.st_atime == st_dir.st_mtime && st_dir.st_mtime == st_dir.st_ctime)) {
    printf("Dir initial times not identical: at=%ld mt=%ld ct=%ld\n", st_dir.st_atime, st_dir.st_mtime, st_dir.st_ctime); return FAIL; }

  // Touch parent by another create to see mtime/ctime bump
  CHECK(create_file("mnt/t20_file2")); fd = ret; CHECK(close_file(fd));
  if (stat("mnt", &st_root_after) != 0) { perror("stat root after"); return FAIL; }
  if (!(st_root_after.st_mtime >= st_root_before.st_mtime && st_root_after.st_ctime >= st_root_before.st_ctime)) {
    printf("Root directory times did not advance on new dentry add\n"); return FAIL; }

  return PASS;
}
