#include <stdio.h>
#include <string.h>
#include <sys/xattr.h>
#include "common/test.h"

int main() {
  int ret;
  CHECK(create_file("mnt/t23_a")); int fd = ret; CHECK(close_file(fd));
  CHECK(create_file("mnt/t23_b")); fd = ret; CHECK(close_file(fd));

  // Color only t23_b
  const char *attr = "user.color"; const char *val_red = "red";
  if (setxattr("mnt/t23_b", attr, val_red, strlen(val_red), 0) != 0) { perror("setxattr red"); return FAIL; }
  else printf("SUCCESS: setxattr red\n");

  char* expected[] = {"t23_a", "t23_b"};
  if (read_dir_check("mnt", expected, 2) != PASS) { return FAIL; }
  return PASS;
}
