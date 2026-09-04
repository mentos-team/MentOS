# ext2

Verified against `MAIN` = `62c638a`.

The driver used to be one 4412-line `kernel/src/fs/ext2.c`; it now lives in
`kernel/src/fs/ext2/`, one unit per concern, so the references below name a
file rather than a line:

| file | what it holds |
| --- | --- |
| `ext2.c` | mount, the VFS operation tables, initialize and finalize |
| `ext2_io.c` | transfers between the block device and memory: superblock, block, BGDT, inode |
| `ext2_alloc.c` | finding, allocating and releasing inodes and blocks |
| `ext2_blockmap.c` | file-relative block to device block, and inode data I/O |
| `ext2_dir.c` | directory entries: iteration, creation, removal |
| `ext2_namei.c` | resolving a path, and creating or removing a name |
| `ext2_file.c` | operations on an open file |
| `ext2_attr.c` | stat, statfs and setattr |
| `ext2_debug.c` | dumps of the on-disk structures |
| `ext2_internal.h` | the definitions the units share |

Serial output quoted further down predates the split and still names
`ext2.c`: it is kept verbatim, because it is evidence of a past run.

## Image build (root cause context for #192)

- `make filesystem` (CMakeLists.txt:177-182) runs:
  `mke2fs -L 'rootfs' -N 0 -d filesystem -b 4096 -m 5 -r 1 -t ext2 -v -F
  ${BUILD}/rootfs.img 32M`.
- Block size 4096, 8192-block image, inode size 256 (dumpe2fs-verified).
- **VERIFIED FACT + EMPIRICALLY REPRODUCED**: `mke2fs -d` stores all-zero
  blocks as sparse holes. Isolated host experiment: 27×4096 bytes of 0xAB
  plus a 4-zero-byte tail → inode with only 27 data blocks; block index 27
  is a hole.
- QEMU attaches rootfs.img as a **rw** IDE disk — the guest writes to the
  image during runs. For pristine-image forensics, delete and rebuild
  (`make filesystem`) before inspecting.

## Read path

`sys_read` → `vfs_read` → `ext2_read(file, buffer, f_pos, n)` →
`ext2_read_inode_data` (ext2_blockmap.c @`MAIN`):

- `end_offset = min(inode->size, offset + nbyte)`;
  `start_block/end_block = offset|end_offset / block_size`;
  loop `start_block..end_block` inclusive, per block
  `ext2_read_inode_block(fs, inode, block_index, cache)` → resolves direct /
  indirect (`ext2_get_real_block_index`) → `ext2_read_block`.

`ext2_read_block` (ext2_io.c @`MAIN`) **rejects block_index == 0**
with `pr_err("You are trying to read an invalid block index (%d)")`.

### The #192 bug (VERIFIED FACT; EMPIRICALLY REPRODUCED)

A hole (block pointer 0) inside `i_size` is legal ext2 and MUST read as
zeros. MentOS treats it as an error: `ext2_read_inode_data`'s loop includes
the hole block, resolution yields 0, `ext2_read_block` fails, the whole read
returns -1. Any file whose size is N full blocks + an all-zero tail gets a
hole at the last block index; reading the file tail fails.

Observed instance: `/bin/tests/t_gid` — 110596 bytes = 27×4096 + 4;
inode 70 in the pristine image: `(0-11):2060-2071, (IND):2072,
(12-26):2073-2087` — 27 data blocks, hole at 27. Consequence chain:
`elf_load_file`'s exact-size `vfs_read` fails → `sys_execve` error after
`mm_destroy` → kernel panic (see execve.md). Serial signature:

```
[ER | ext2.c:1047] You are trying to read an invalid block index (0).
[ER | ext2.c:1919] Failed to read the inode block   27 of inode   70
[ER | elf.c:309  ] Failed to read 110592 bytes from the file `t_gid`.
... page fault → PANIC
```

Host `debugfs -R "dump <70> ..."` reads the file correctly (holes → zeros),
proving the image is valid and the defect is in the kernel read path.

### HISTORICAL CONTEXT

Issue #56 (2024): identical failure loading `t_alarm` (86020 bytes = 21
blocks − 4 … trailing-4-bytes pattern), closed after PR #59 ("Fix ext2 read
and write") without the sparse root cause being identified; maintainer asked
for a new issue on recurrence → #192.

## Open path and the per-inode file cache

`ext2_open` (ext2_file.c @`MAIN`):
- `get_ext2_filesystem(path)` → `ext2_resolve_path(fs->root, path, &search)`.
- O_CREAT → `ext2_creat`; O_DIRECTORY/O_EXCL validation; inode read;
  `vfs_valid_open_permissions` (root/pid-0 bypass; owner/group/other bits);
  O_TRUNC on regular files → `ext2_clean_inode_content`.
- **`ext2_find_vfs_file_with_inode`**: reuse cached `vfs_file_t` for the same
  inode (shared `count`, `f_pos`); only allocate + `ext2_init_vfs_file` +
  insert into `fs->opened_files` when not cached.
- Security relevance: see vfs-and-fd-lifetime.md — shared structs make
  refcount bugs exploitable (PR #189's pre-fix PoC).

## Close path

`ext2_close` (ext2_file.c @`MAIN`): `--count`; at 0: **refuses to free
`fs->root`** (pr_warning + `-EPERM`, count stays 0), otherwise unlinks from
`fs->opened_files` and `vfs_dealloc_file`. The root guard is what keeps the
mount root alive despite reference quirks.

## Untested hypotheses (do not treat as fact)

- **Write-side mirror of #192 (HYPOTHESIS)**: writing into a sparse hole
  must allocate a block first; `ext2_write_block` rejects block 0 the same
  way `ext2_read_block` does, so growing/appending into holes likely fails
  or corrupts. Not exercised during this investigation.
- ext2 audit tests exist (`t_ext2_audit_*`) but were never reached in our
  runs (panic at test 10); their coverage quality is unknown to us.

## Forensic quick reference (see debugging-playbook.md for full workflows)

```
debugfs -R "stat <INO>" rootfs.img        # size + BLOCKS list + TOTAL
debugfs -R "blocks <INO>" rootfs.img      # flat block list
debugfs -R "dump <INO> /tmp/out" rootfs.img
debugfs -R "ls -l /bin/tests" rootfs.img
dumpe2fs -h rootfs.img                    # block size, counts, inode size
```
Rule of thumb: `TOTAL` blocks (debugfs counts the indirect block) vs
`ceil(size / 4096)` data blocks — a shortfall means holes.
