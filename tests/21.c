#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "common/test.h"

/* Test 21: read updates atime but not mtime/ctime */
int main() {
  int ret;
  CHECK(create_file("mnt/t21_file"));
  int fd = ret;
  CHECK(write_file_check(fd, "ABC", 3, "mnt/t21_file", 0));
  CHECK(close_file(fd));

  struct stat before, after;
  if (stat("mnt/t21_file", &before) != 0) { perror("stat before"); return FAIL; }
  sleep(1); // ensure timestamp granularity difference
  CHECK(open_file_read("mnt/t21_file")); fd = ret;
  char buf[3]; if (read(fd, buf, 3) != 3) { perror("read"); return FAIL; }
  CHECK(close_file(fd));
  if (stat("mnt/t21_file", &after) != 0) { perror("stat after"); return FAIL; }

  if (!(after.st_atime >= before.st_atime)) { printf("atime did not increase on read\n"); return FAIL; }
  if (after.st_mtime != before.st_mtime) { printf("mtime changed on read (should not)\n"); return FAIL; }
  if (after.st_ctime != before.st_ctime) { printf("ctime changed on read (should not)\n"); return FAIL; }
  return PASS;
}
