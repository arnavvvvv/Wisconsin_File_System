#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
// xattr
#include <sys/xattr.h>

#include "common/test.h"

int main() {
  const char* paths[] = {"mnt/file0",       "mnt/file1",
                         "mnt/dir0/file00", "mnt/dir0/file01",
                         "mnt/dir1/file10", "mnt/dir1/file11"};

  for (size_t i = 0; i < 6; i++) {
    int ret;
    CHECK(open_file_read(paths[i]));
    int fd = ret;

    const char* content = "content";
    CHECK(read_file_check(fd, content, strlen(content), paths[i], 0));
    
    CHECK(close_file(fd));
  }

  /* Verify deterministic colors for each created file/dir. */
  const char* xattr_paths[] = {"mnt/file0", "mnt/file1", "mnt/dir0", "mnt/dir0/file00",
                               "mnt/dir0/file01", "mnt/dir1", "mnt/dir1/file10", "mnt/dir1/file11"};
  const char* expected_colors[] = {"green", "red", "yellow", "magenta", "cyan", "blue", "green", "red"};
  const size_t num_checks = sizeof(expected_colors) / sizeof(expected_colors[0]);

  for (size_t i = 0; i < num_checks; i++) {
    const char* p = xattr_paths[i];
    struct stat st;
    if (stat(p, &st) < 0) {
      printf("Path missing: %s\n", p);
      return FAIL;
    }

    char buf[128] = {0};
    ssize_t x = getxattr(p, "user.color", buf, sizeof(buf));
    if (x < 0) {
      printf("Missing xattr user.color for %s: %s\n", p, strerror(errno));
      return FAIL;
    }
    size_t got = (size_t)x;
    if (got > 0 && buf[got - 1] == '\0') {
      /* Some environments may store a trailing NUL; ignore it for comparison */
      got -= 1;
    }
    const char* expect = expected_colors[i];
    if (got != strlen(expect) || strncmp(buf, expect, got) != 0) {
      printf("Wrong xattr for %s: expected '%s', got '%.*s'\n", p, expect, (int)got, buf);
      return FAIL;
    } else {
      printf("SUCCESS: xattr user.color for %s = '%s'\n", p, expect);
    }
  }

  return PASS;
}
 
