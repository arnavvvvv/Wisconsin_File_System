#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include "common/test.h"

/* Test 28: statfs reports reasonable totals and free counts, and deltas after allocations */
int main() {
  int ret;

  struct statvfs s0, s1;
  if (statvfs("mnt", &s0) != 0) { perror("statvfs before"); return FAIL; }

  // Create a bunch of small files to consume inodes and a few data blocks
  const int N = 17; // ensure directory spills into next block in many setups
  char name[64];
  for (int i = 0; i < N; i++) {
    snprintf(name, sizeof(name), "mnt/s28_%02d", i);
    CHECK(create_file(name)); int fd = ret;
    CHECK(write_file_check(fd, "x", 1, name, 0));
    CHECK(close_file(fd));
  }

  if (statvfs("mnt", &s1) != 0) { perror("statvfs after"); return FAIL; }

  // Basic invariants: block size fields set
  if (s0.f_bsize == 0 || s0.f_frsize == 0) { printf("f_bsize/frsize are zero\n"); return FAIL; }
  // Totals stable within a mounted FS
  if (s1.f_blocks != s0.f_blocks) { printf("f_blocks changed across ops (%lu vs %lu)\n", s1.f_blocks, s0.f_blocks); return FAIL; }
  if (s1.f_files  != s0.f_files)  { printf("f_files changed across ops (%lu vs %lu)\n", s1.f_files, s0.f_files); return FAIL; }

  // Free counts should decrease or stay same after creating files and writing data
  if (s1.f_ffree > s0.f_ffree) { printf("f_ffree increased unexpectedly (%lu -> %lu)\n", s0.f_ffree, s1.f_ffree); return FAIL; }
  if (s1.f_bfree > s0.f_bfree) { printf("f_bfree increased unexpectedly (%lu -> %lu)\n", s0.f_bfree, s1.f_bfree); return FAIL; }

  // Should have consumed at least N inodes (root dir link counts aside) and some data blocks
  unsigned long inodes_used = s0.f_ffree - s1.f_ffree;
  if (inodes_used < (unsigned long)N) {
    printf("Expected to consume >= %d inodes, consumed %lu\n", N, inodes_used);
    return FAIL;
  }

  unsigned long blocks_used = s0.f_bfree - s1.f_bfree;
  if (blocks_used == 0) {
    printf("Expected to consume at least 1 data block, consumed 0\n");
    return FAIL;
  }

  return PASS;
}
