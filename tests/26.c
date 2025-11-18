#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include "common/test.h"

/* Test 26: statvfs around file times semantics
 * - Create + write: free inode/block counts drop
 * - Read (updates atime): free counts unchanged
 * - Append write (updates mtim/ctim): blocks drop again, inodes unchanged
 * - readdir on parent (updates atime): free counts unchanged
 * - Unlink: free counts increase (at least inode + one data block)
 */
int main() {
  int ret;

  struct statvfs s0, s1, s2, s3, s4, s5;
  if (statvfs("mnt", &s0) != 0) { perror("statvfs before"); return FAIL; }
  else printf("SUCCESS: initial statvfs succeeded\n");

  // Create one file and write exactly one block (512 bytes) to allocate 1 data block
  const char *path = "mnt/t26";
  CHECK(create_file(path)); int fd = ret;
  char block[512]; memset(block, 'A', sizeof(block));
  CHECK(write_file_check(fd, block, sizeof(block), path, 0));
  CHECK(close_file(fd));

  if (statvfs("mnt", &s1) != 0) { perror("statvfs after create/write"); return FAIL; }
  else printf("SUCCESS: statvfs after create+write\n");

  // Invariants
  if (s0.f_bsize == 0 || s0.f_frsize == 0) { printf("f_bsize/frsize are zero\n"); return FAIL; }
  else printf("SUCCESS: block size fields set (f_bsize=%lu, f_frsize=%lu)\n", s0.f_bsize, s0.f_frsize);
  if (s1.f_blocks != s0.f_blocks) { printf("f_blocks changed across ops (%lu vs %lu)\n", s1.f_blocks, s0.f_blocks); return FAIL; }
  else printf("SUCCESS: f_blocks stable across ops (%lu)\n", s1.f_blocks);
  if (s1.f_files  != s0.f_files)  { printf("f_files changed across ops (%lu vs %lu)\n", s1.f_files, s0.f_files); return FAIL; }
  else printf("SUCCESS: f_files stable across ops (%lu)\n", s1.f_files);

  // After create+write: expect inode consumed and blocks consumed (file data + maybe 1 dir block)
  unsigned long inodes_used_1 = s0.f_ffree - s1.f_ffree;
  unsigned long blocks_used_1 = s0.f_bfree - s1.f_bfree;
  if (inodes_used_1 < 1) { printf("Expected to consume at least 1 inode, consumed %lu\n", inodes_used_1); return FAIL; }
  else printf("SUCCESS: consumed %lu inode(s) after create\n", inodes_used_1);
  if (blocks_used_1 < 1) { printf("Expected to consume at least 1 data block after create, consumed 0\n"); return FAIL; }
  else printf("SUCCESS: consumed %lu data block(s) after create\n", blocks_used_1);

  // Read the file to update atime; free counts should not change
  CHECK(open_file_read(path)); fd = ret;
  char expected_first_bytes[8]; memset(expected_first_bytes, 'A', sizeof(expected_first_bytes));
  CHECK(read_file_check(fd, expected_first_bytes, sizeof(expected_first_bytes), path, 0));
  CHECK(close_file(fd));

  if (statvfs("mnt", &s2) != 0) { perror("statvfs after read"); return FAIL; }
  else printf("SUCCESS: statvfs after read\n");
  if (s2.f_ffree != s1.f_ffree) { printf("f_ffree changed on atime-only read (%lu -> %lu)\n", s1.f_ffree, s2.f_ffree); return FAIL; }
  if (s2.f_bfree != s1.f_bfree) { printf("f_bfree changed on atime-only read (%lu -> %lu)\n", s1.f_bfree, s2.f_bfree); return FAIL; }
  printf("SUCCESS: read did not change free counts\n");

  // Append another full block to allocate one more block; inode free count should stay the same
  CHECK(open_file_write(path)); fd = ret;
  CHECK(write_file_check(fd, block, sizeof(block), path, 512));
  CHECK(close_file(fd));

  if (statvfs("mnt", &s3) != 0) { perror("statvfs after append"); return FAIL; }
  else printf("SUCCESS: statvfs after append\n");
  if (s3.f_ffree != s2.f_ffree) { printf("f_ffree changed unexpectedly after append (%lu -> %lu)\n", s2.f_ffree, s3.f_ffree); return FAIL; }
  if (!(s3.f_bfree < s2.f_bfree)) { printf("Expected f_bfree to drop after append (%lu -> %lu)\n", s2.f_bfree, s3.f_bfree); return FAIL; }
  printf("SUCCESS: append consumed additional data blocks\n");

  // List directory to update its atime; free counts should not change
  DIR *d = opendir("mnt"); if (!d) { perror("opendir mnt"); return FAIL; }
  struct dirent *e; while ((e = readdir(d)) != NULL) { (void)e; }
  closedir(d);

  if (statvfs("mnt", &s4) != 0) { perror("statvfs after readdir"); return FAIL; }
  else printf("SUCCESS: statvfs after readdir\n");
  if (s4.f_ffree != s3.f_ffree) { printf("f_ffree changed on directory atime update (%lu -> %lu)\n", s3.f_ffree, s4.f_ffree); return FAIL; }
  if (s4.f_bfree != s3.f_bfree) { printf("f_bfree changed on directory atime update (%lu -> %lu)\n", s3.f_bfree, s4.f_bfree); return FAIL; }
  printf("SUCCESS: readdir did not change free counts\n");

  // Unlink the file: should free at least its data blocks and one inode
  CHECK(remove_file(path));
  if (statvfs("mnt", &s5) != 0) { perror("statvfs after unlink"); return FAIL; }
  else printf("SUCCESS: statvfs after unlink\n");
  if (!(s5.f_ffree > s4.f_ffree)) { printf("Expected f_ffree to increase after unlink (%lu -> %lu)\n", s4.f_ffree, s5.f_ffree); return FAIL; }
  if (!(s5.f_bfree > s4.f_bfree)) { printf("Expected f_bfree to increase after unlink (%lu -> %lu)\n", s4.f_bfree, s5.f_bfree); return FAIL; }
  printf("SUCCESS: unlink freed inode(s) and data block(s)\n");

  return PASS;
}
