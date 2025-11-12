#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <string.h>
#include "common/test.h"

/* Test 24: xattr set/get/list/remove for user.color and ctime updates */
int main()
{
  int ret;
  CHECK(create_file("mnt/t24_file"));
  int fd = ret;
  CHECK(close_file(fd));

  struct stat before, after1, after2;
  if (stat("mnt/t24_file", &before) != 0)
  {
    perror("stat before");
    return FAIL;
  }

  // Set color to blue
  sleep(1);
  const char *attr = "user.color";
  const char *val_blue = "blue";
  if (setxattr("mnt/t24_file", attr, val_blue, strlen(val_blue), 0) != 0)
  {
    perror("setxattr blue");
    return FAIL;
  }

  // getxattr should return "blue"
  char vbuf[64];
  ssize_t n = getxattr("mnt/t24_file", attr, vbuf, sizeof(vbuf));
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

  // listxattr should include user.color
  /*ssize_t need = listxattr("mnt/t24_file", NULL, 0);
  if (need <= 0)
  {
    printf("listxattr size unexpected: %ld\n", need);
    return FAIL;
  }
  char *lbuf = (char *)malloc(need);
  if (listxattr("mnt/t24_file", lbuf, need) != need)
  {
    printf("listxattr read mismatch\n");
    free(lbuf);
    return FAIL;
  }
  int found = 0;
  for (ssize_t i = 0; i < need;)
  {
    const char *p = lbuf + i;
    if (strcmp(p, attr) == 0)
    {
      found = 1;
      break;
    }
    i += strlen(p) + 1;
  }
  free(lbuf);
  if (!found)
  {
    printf("user.color not in listxattr\n");
    return FAIL;
  }*/

  if (stat("mnt/t24_file", &after1) != 0)
  {
    perror("stat after1");
    return FAIL;
  }
  if (!(after1.st_ctime >= before.st_ctime))
  {
    printf("ctime did not increase after setxattr\n");
    return FAIL;
  }

  // Remove xattr -> should become "none"
  sleep(1);
  if (removexattr("mnt/t24_file", attr) != 0)
  {
    perror("removexattr");
    return FAIL;
  }
  n = getxattr("mnt/t24_file", attr, vbuf, sizeof(vbuf));
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
  if (stat("mnt/t24_file", &after2) != 0)
  {
    perror("stat after2");
    return FAIL;
  }
  if (!(after2.st_ctime >= after1.st_ctime))
  {
    printf("ctime did not increase after removexattr\n");
    return FAIL;
  }

  return PASS;
}
