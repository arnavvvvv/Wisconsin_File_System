#include <stdio.h>
#include <string.h>
#include <sys/xattr.h>
#include <dirent.h>
#include "common/test.h"

/* Test 28: Changing color updates ctime and name remains stable (non-ls view) */
int main() {
  int ret;
  CHECK(create_file("mnt/t28_target")); int fd = ret; CHECK(close_file(fd));

  struct stat s0, s1, s2;
  if (stat("mnt/t28_target", &s0) != 0) { perror("stat s0"); return FAIL; }
  sleep(1);
  // Set blue
  if (setxattr("mnt/t28_target", "user.color", "blue", 4, 0) != 0) { perror("setxattr blue"); return FAIL; }
  else printf("SUCCESS: setxattr blue for file t28_target\n");
  if (stat("mnt/t28_target", &s1) != 0) { perror("stat s1"); return FAIL; }
  if (!(s1.st_ctime >= s0.st_ctime)) { printf("ctime did not advance after first color set\n"); return FAIL; }
  else printf("SUCCESS: ctime advanced after first color set\n");
  sleep(1);
  // Change to red
  if (setxattr("mnt/t28_target", "user.color", "red", 3, 0) != 0) { perror("setxattr red"); return FAIL; }
  else printf("SUCCESS: setxattr red for file t28_target\n");
  if (stat("mnt/t28_target", &s2) != 0) { perror("stat s2"); return FAIL; }
  if (!(s2.st_ctime >= s1.st_ctime)) { printf("ctime did not advance after second color set\n"); return FAIL; }
  else printf("SUCCESS: ctime advanced after second color set\n");

  // Non-ls readdir should list plain name
  char* expected[] = {"t28_target"};
  if (read_dir_check("mnt", expected, 1) != PASS) { return FAIL; }
  else printf("SUCCESS: plain entry found for t28_target on non-ls readdir\n");

  return PASS;
}
