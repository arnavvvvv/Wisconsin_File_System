#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include "common/test.h"


int main() {
  int ret;

  struct statvfs s0, s1;
  if (statvfs("mnt", &s0) != 0) { perror("statvfs before"); return FAIL; }
  else printf("SUCCESS: initial statvfs succeeded\n");

  // Create a bunch of small files to consume inodes and a few data blocks
  const int N = 17; // ensure directory spills into next block in many setups
  char name[64];
  for (int i = 0; i < N; i++) {
    snprintf(name, sizeof(name), "mnt/s18_%02d", i);
    CHECK(create_file(name)); int fd = ret;
    CHECK(write_file_check(fd, "x", 1, name, 0));
    CHECK(close_file(fd));
  }

  if (statvfs("mnt", &s1) != 0) { perror("statvfs after"); return FAIL; }
  else printf("SUCCESS: subsequent statvfs succeeded\n");

  // Basic invariants: block size fields set
  if (s0.f_bsize == 0 || s0.f_frsize == 0) { printf("f_bsize/frsize are zero\n"); return FAIL; }
  else printf("SUCCESS: block size fields set (f_bsize=%lu, f_frsize=%lu)\n", s0.f_bsize, s0.f_frsize);
  // Totals stable within a mounted FS
  if (s1.f_blocks != s0.f_blocks) { printf("f_blocks changed across ops (%lu vs %lu)\n", s1.f_blocks, s0.f_blocks); return FAIL; }
  else printf("SUCCESS: f_blocks stable across ops (%lu)\n", s1.f_blocks);
  if (s1.f_files  != s0.f_files)  { printf("f_files changed across ops (%lu vs %lu)\n", s1.f_files, s0.f_files); return FAIL; }
  else printf("SUCCESS: f_files stable across ops (%lu)\n", s1.f_files);

  // Free counts should decrease after creating files and writing data
  if (s1.f_ffree >= s0.f_ffree) { printf("f_ffree increased unexpectedly (%lu -> %lu)\n", s0.f_ffree, s1.f_ffree); return FAIL; }
  else printf("SUCCESS: f_ffree decreased\n");
  if (s1.f_bfree >= s0.f_bfree) { printf("f_bfree increased unexpectedly (%lu -> %lu)\n", s0.f_bfree, s1.f_bfree); return FAIL; }
  else printf("SUCCESS: f_bfree decreased\n");

  // Should have consumed at least N inodes (root dir link counts aside) and some data blocks
  unsigned long inodes_used = s0.f_ffree - s1.f_ffree;
  if (inodes_used != (unsigned long)N) {
    printf("Expected to consume >= %d inodes, consumed %lu\n", N, inodes_used);
    return FAIL;
  }
  else printf("SUCCESS: consumed %lu inodes\n", inodes_used);

  unsigned long blocks_used = s0.f_bfree - s1.f_bfree;
  if (blocks_used <= N) {
    printf("Expected to consume at least %d data blocks, consumed %lu\n", N, blocks_used);
    return FAIL;
  }
  else printf("SUCCESS: consumed %lu data blocks\n", blocks_used);

  return PASS;
}
