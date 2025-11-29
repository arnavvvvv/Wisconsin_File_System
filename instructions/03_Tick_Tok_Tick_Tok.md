# Part 3 – Tick Tok Tick Tok (File Times)
---
Extend your inode with three timestamps: `atim` (last access), `mtim` (last content modification), and `ctim` (last metadata/status change). Keep them consistent with familiar POSIX behavior so `stat` and friends look reasonable.

## Objective
---
- Creation: initialize all three to “now” (current time) when a file or directory is first made.
- Reporting: `getattr` returns these values; it never changes them.
- Reading: when file contents are actually read, advance the file’s access time.
- Listing directories: after producing a directory listing, advance that directory’s access time.
- Writing: when file contents change, advance modification time; metadata-affecting operations should advance change time.
- Directory entry changes: when you add or remove a child, the parent directory’s modification and change times should advance.
- Removal: parent directory times should reflect the change; it’s reasonable to also advance the removed object’s change time during teardown.

## Implementation Guidance
---
- Place time updates where you’re certain the operation succeeded to avoid spurious changes.
- Keep behavior uniform across similar code paths (e.g., all writes update the same set of times).

## Sanity checks
---
- Immediately after creation, the three times match.
- A read followed by a write shows access time advancing on the read, and modification/change times advancing on the write.
- Listing a directory advances that directory’s access time; creating a child advances its modification/change times.