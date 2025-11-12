### Part 2 – Show me the Big Picture (statfs)
Goal: Report aggregate filesystem statistics.
Implement `statfs()`:
- `f_blocks` = total data block capacity.
- `f_files` = total inode capacity.
- `f_bfree`/`f_bavail` = number of free data blocks.

Use bitmap popcount to calculate used/free. After allocations, free counts should drop.

- Creating files reduces `f_ffree`; writing allocating blocks reduces `f_bfree`.