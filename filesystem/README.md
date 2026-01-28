# Root Filesystem (`filesystem/`)

The root filesystem content that gets packaged into an EXT2 disk image.

## Structure

```text
filesystem/
├── bin/              ← Compiled user programs (symlink/copy)
│   ├── shell, cat, ls, mkdir, ...
│   └── tests/        ← Test executables
├── dev/              ← Device files (empty, created at runtime)
├── etc/              ← System configuration
│   ├── passwd        ← User accounts
│   ├── group         ← User groups
│   ├── shadow        ← Password hashes
│   ├── hostname      ← System hostname
│   ├── issue         ← Login message
│   └── motd          ← Message of the day
├── home/             ← User home directories
│   └── user/         ← Regular user home
│       ├── .shellrc  ← Shell configuration
│       └── welcome.md
├── proc/             ← Procfs mount point (empty, created at runtime)
├── root/             ← Root user home
│   └── .shellrc      ← Root shell config
└── usr/
    └── share/man/    ← Manual pages for commands
        └── *.man
```

## Building the Filesystem

```bash
make filesystem
```

This creates:

```
build/rootfs.img     ← EXT2 filesystem image (32MB by default)
```

The image is created using `mke2fs` with the contents from `filesystem/`.

## EXT2 Image

The filesystem image:

- **Size**: 32MB (configurable in root CMakeLists.txt)
- **Format**: EXT2 (Linux second extended filesystem)
- **Inode count**: Calculated automatically
- **Block size**: 4096 bytes

## Configuration Files

### `etc/passwd`

User accounts and home directories

```text
root:*:0:0::/root:/bin/shell
user:*:1000:1000::/home/user:/bin/shell
```

### `etc/group`

User groups

```text
root:*:0:
wheel:*:1:
user:*:1000:
```

### `etc/shadow`

Password hashes

```text
root:HASH:1:0:99999:7:0:0:
```

### `etc/hostname`

System hostname

### Manual Pages

Located in `usr/share/man/`:

Access via `man` command in shell.

## Boot Sequence

1. **init** process mounts filesystem
2. Load configuration from /etc
3. Start login prompt
4. User logs in

## Adding Files

1. Place files in `filesystem/` directory tree
2. Run `make filesystem`
3. Files become part of `build/rootfs.img`

## Rebuilding

After modifying files:

```bash
make filesystem      # Rebuilds rootfs.img
make qemu            # Run new image in QEMU
```

## Related

- [userspace/README.md](../userspace/README.md) - User programs
- [boot/README.md](../boot/README.md) - Bootloader
- [kernel/README.md](../kernel/README.md) - Kernel
