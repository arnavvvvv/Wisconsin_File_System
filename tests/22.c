#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "common/test.h"

/* Test 22: write updates mtime and ctime; atime unaffected unless read */
int main() {
  int ret;
  CHECK(create_file("mnt/t22_file"));
  int fd = ret;
  CHECK(write_file_check(fd, "AB", 2, "mnt/t22_file", 0));
  CHECK(close_file(fd));

  struct stat before, after;
  if (stat("mnt/t22_file", &before) != 0) { perror("stat before"); return FAIL; }
  sleep(1);
  CHECK(open_file_write("mnt/t22_file")); fd = ret;
  CHECK(write_file_check(fd, "CD", 2, "mnt/t22_file", 2));
  CHECK(close_file(fd));
  if (stat("mnt/t22_file", &after) != 0) { perror("stat after"); return FAIL; }

  if (!(after.st_mtime >= before.st_mtime)) { printf("mtime did not increase on write\n"); return FAIL; }
  if (!(after.st_ctime >= before.st_ctime)) { printf("ctime did not increase on write\n"); return FAIL; }
  // We didn't read, so atime may stay same (strictly could change due to implementation granularity but should not decrease)
  if (after.st_atime != before.st_atime) { printf("atime changed on pure write (should not)\n"); return FAIL; }

  return PASS;
}
