### Part 4 – Colour Colour which Colour do you choose? (xattrs + Colored ls)
Add a simple per-file “color” tag and surface it via an extended attribute. Show colored names in human-facing listings without baking escape codes into the on-disk names.

Guidance
- Store a compact palette code in each inode (0 = none). Expose it through the `user.color` xattr: set/get/list/remove.
- When listing, render names with color only for `ls`; programmatic consumers should see plain names.
- Treat changing color as a metadata update (consider the timestamp it should affect).