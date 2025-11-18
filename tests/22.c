#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <string.h>
#include "common/test.h"

int main()
{
  int ret;
  CHECK(create_file("mnt/t22_file"));
  int fd = ret;
  CHECK(close_file(fd));

  struct stat before, after1, after2;
  if (stat("mnt/t22_file", &before) != 0)
  {
    perror("stat before");
    return FAIL;
  }

  // Set color to blue
  sleep(1);
  const char *attr = "user.color";
  const char *val_blue = "blue";
  if (setxattr("mnt/t22_file", attr, val_blue, strlen(val_blue), 0) != 0)
  {
    perror("setxattr blue");
    return FAIL;
  }
  else printf("SUCCESS: setxattr to blue\n");

  // getxattr should return "blue"
  char vbuf[64];
  ssize_t n = getxattr("mnt/t22_file", attr, vbuf, sizeof(vbuf));
  if (n < 0)
  {
    perror("getxattr after set");
    return FAIL;
  }
  if (strcmp(vbuf, "blue") != 0)
  {
    printf("expected 'blue', got '%s'\n", vbuf);
    return FAIL;
  }
  else printf("SUCCESS: getxattr returned blue\n");
 
  if (stat("mnt/t22_file", &after1) != 0)
  {
    perror("stat after1");
    return FAIL;
  }
  if (!(after1.st_ctime >= before.st_ctime))
  {
    printf("ctime did not increase after setxattr\n");
    return FAIL;
  }
  else printf("SUCCESS: ctime increased after setxattr\n");

  // Remove xattr -> should become "none"
  sleep(1);
  if (removexattr("mnt/t22_file", attr) != 0)
  {
    perror("removexattr");
    return FAIL;
  }
  else printf("SUCCESS: removexattr\n");
  n = getxattr("mnt/t22_file", attr, vbuf, sizeof(vbuf));
  if (n < 0)
  {
    perror("getxattr after remove");
    return FAIL;
  }
  if (strcmp(vbuf, "none") != 0)
  {
    printf("expected 'none' after remove, got '%s'\n", vbuf);
    return FAIL;
  }
  else printf("SUCCESS: getxattr returned none after removexattr\n");
  if (stat("mnt/t22_file", &after2) != 0)
  {
    perror("stat after2");
    return FAIL;
  }
  if (!(after2.st_ctime >= after1.st_ctime))
  {
    printf("ctime did not increase after removexattr\n");
    return FAIL;
  }
  else printf("SUCCESS: ctime increased after removexattr\n");

  return PASS;
}
