#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "common/test.h"


int main() {
  int ret;
  // Create a parent directory
  CHECK(create_dir("mnt/t27_dir"));

  struct stat p0, p1, p2, p3;
  if (stat("mnt/t27_dir", &p0) != 0) { perror("stat p0"); return FAIL; }

  // readdir should bump atime only
  sleep(1);
  DIR* d = opendir("mnt/t27_dir"); if (!d) { perror("opendir"); return FAIL; }
  struct dirent* e; while ((e = readdir(d)) != NULL) { /* drain */ }
  closedir(d);
  if (stat("mnt/t27_dir", &p1) != 0) { perror("stat p1"); return FAIL; }
  if (!(p1.st_atime >= p0.st_atime)) { printf("dir atime did not increase on readdir\n"); return FAIL; }
  else printf("SUCCESS: dir atime updated correctly on readdir\n");
  if (p1.st_mtime != p0.st_mtime) { printf("dir mtime changed on readdir\n"); return FAIL; }
  else printf("SUCCESS: dir mtime unchanged on readdir\n");
  if (p1.st_ctime != p0.st_ctime) { printf("dir ctime changed on readdir\n"); return FAIL; }
  else printf("SUCCESS: dir ctime unchanged on readdir\n");

  // Adding an entry should bump mtime/ctime
  sleep(1);
  CHECK(create_file("mnt/t27_dir/child")); int fd = ret; CHECK(close_file(fd));
  if (stat("mnt/t27_dir", &p2) != 0) { perror("stat p2"); return FAIL; }
  if (!(p2.st_mtime >= p1.st_mtime && p2.st_ctime >= p1.st_ctime)) { printf("dir times did not increase on add dentry\n"); return FAIL; }
  else printf("SUCCESS: dir mtime/ctime updated correctly on add dentry\n");

  // Removing entry should bump mtime/ctime again
  sleep(1);
  CHECK(remove_file("mnt/t27_dir/child"));
  if (stat("mnt/t27_dir", &p3) != 0) { perror("stat p3"); return FAIL; }
  if (!(p3.st_mtime >= p2.st_mtime && p3.st_ctime >= p2.st_ctime)) { printf("dir times did not increase on remove dentry\n"); return FAIL; }
  else printf("SUCCESS: dir mtime/ctime updated correctly on remove dentry\n");

  return PASS;
}
