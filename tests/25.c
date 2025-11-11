#include <stdio.h>
#include <string.h>
#include <sys/xattr.h>
#include "common/test.h"

/* Test 25: readdir from a normal process returns plain names regardless of color */
int main() {
  int ret;
  CHECK(create_file("mnt/t25_a")); int fd = ret; CHECK(close_file(fd));
  CHECK(create_file("mnt/t25_b")); fd = ret; CHECK(close_file(fd));

  // Color only t25_b
  const char *attr = "user.color"; const char *val_red = "red";
  if (setxattr("mnt/t25_b", attr, val_red, strlen(val_red), 0) != 0) { perror("setxattr red"); return FAIL; }

  char* expected[] = {"t25_a", "t25_b"};
  if (read_dir_check("mnt", expected, 2) != PASS) { return FAIL; }
  return PASS;
}
