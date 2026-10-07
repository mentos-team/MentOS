Symlink regression fixtures
===========================

These links and their target files are part of the ext2 test image.  The
`t_symlink` userspace test uses them to exercise relative and absolute target
resolution, block-backed link targets, and trailing separators.

They intentionally live outside `/home/user`: they are test data, not files
that should appear in the example user's home directory.
