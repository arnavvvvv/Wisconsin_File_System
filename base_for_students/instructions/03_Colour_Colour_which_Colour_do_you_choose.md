## Part 4 – Colour Colour which Colour do you choose? (xattrs + Colored ls)
In this final part, you'll add a per-file "color" tag. This is a metadata-only feature that won't change the file's data. You'll store this tag in the inode, expose it via the standard "extended attribute" (xattr) interface, and then - for the grand finale - you'll make ls display files in their designated color without breaking any other programs.

This is a two-part challenge: implementing the xattr API and then implementing the "magic" ls detection.

### 4.1. Storing and Exposing Color (xattrs)

First, you need a place to store the color.

Look at `wfs.h` and the new `wfs_color_t` enum. You'll need to add a new field to your `struct wfs_inode` to hold this `uint8_t` color code.

Where should you initialize this? A new file's color should be `WFS_COLOR_NONE`. Find the function that initializes new inodes (like `fillin_inode`) and set this new field.

Next, wire up the xattr operations in `wfs_ops`:

**`wfs_setxattr`:** This is your "write" function.

- It should check if the name is `"user.color"`. If not, return `-EOPNOTSUPP`.
- It should use the provided `parse_color_name` helper to turn the value string (e.g., `"red"`) into its `uint8_t` code.
- Store that code in your new inode field.
- This is a metadata change! What timestamp should you update?

**`wfs_getxattr`:** This is your "read" function.

- It should also check for the `"user.color"` name.
- Use the `wfs_color_from_code` helper to turn the inode's `uint8_t` code back into a string (use the `.name` field of the returned struct).
- Correctly handle the getxattr "size" dance: if size is 0, just return the `strlen` of the name string. If size is sufficient, `memcpy` the string into the value buffer.

**`wfs_removexattr`:** Set the inode's color field back to `WFS_COLOR_NONE`.

Once this is done, you should be able to use the `setfattr` and `getfattr` command-line tools to tag your files:
```bash
$ setfattr -n user.color -v red /your/mntpoint/somefile
$ getfattr -n user.color /your/mntpoint/somefile
```

### 4.2. The ls "Magic Trick"

This is the fun part. You want `ls` to show colors, but `find`, `cat`, and scripts to see the plain, clean filenames.

**The Goal:**

- If `ls` calls `readdir`, you will send it "dirty" filenames with ANSI codes (e.g., `\033[31mfile.txt\033[0m`).
- If any other program calls `readdir`, you will send it the normal, "clean" filename (e.g., `file.txt`).

**The "How":**

Modify `wfs_readdir`: How can you know who is calling `readdir`?

- Hint: `fuse_get_context()` returns a `struct fuse_context` that contains the caller's `pid`.
- With a `pid`, you can go "spying" in the `/proc` filesystem. Reading `/proc/<pid>/comm` will give you the command name (e.g., `"ls"`).

Inside your `readdir` loop:

- Check if the caller is `"ls"` and the file's inode has a color set.
- If YES: Use `snprintf` to build a "dirty" string using the `.ansi` code from `wfs_color_from_code` and the `dent->name`. Pass this new string to the filler function.
- If NO: Pass the regular `dent->name` to the filler function, just like you did before.