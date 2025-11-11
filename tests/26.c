#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/xattr.h>
#include <sys/prctl.h>
#include <dirent.h>
#include <errno.h>
#include "common/test.h"

/* Test 26: readdir when process name is "ls" returns ANSI-colored names for colored entries */
int main() {
  int ret;
  CHECK(create_file("mnt/t26_a")); int fd = ret; CHECK(close_file(fd));
  CHECK(create_file("mnt/t26_b")); fd = ret; CHECK(close_file(fd));

  // Color only t26_b
  const char *attr = "user.color"; const char *val_blue = "blue";
  if (setxattr("mnt/t26_b", attr, val_blue, strlen(val_blue), 0) != 0) { perror("setxattr blue"); return FAIL; }

  // Set process name to "ls" so the filesystem colorizes names
  if (prctl(PR_SET_NAME, (unsigned long)"ls", 0, 0, 0) != 0) {
    perror("prctl PR_SET_NAME");
    return FAIL;
  }

  DIR* dir = opendir("mnt");
  if (!dir) { perror("opendir"); return FAIL; }
  int saw_plain_a = 0, saw_colored_b = 0;
  struct dirent *e;
  while ((e = readdir(dir)) != NULL) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
    if (strstr(e->d_name, "t26_a") != NULL) {
      // uncolored file should appear plain (no ESC)
      if (strchr(e->d_name, '\033') == NULL) saw_plain_a = 1;
    }
    if (strstr(e->d_name, "t26_b") != NULL) {
      // colored file name should contain ANSI prefix and reset suffix
      if (strchr(e->d_name, '\033') != NULL && strstr(e->d_name, "\033[0m") != NULL) saw_colored_b = 1;
    }
  }
  closedir(dir);

  if (!saw_plain_a) { printf("plain entry missing or colored unexpectedly for t26_a\n"); return FAIL; }
  if (!saw_colored_b) { printf("colored entry missing or not colorized for t26_b\n"); return FAIL; }

  return PASS;
}
