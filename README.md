# MiniVFS

MiniVFS is a lightweight C-based virtual filesystem image generator and file adder for a custom disk image format. The project includes two utilities:

- `mkfs_builder.c` – creates a new filesystem image
- `mkfs_adder.c` – inserts a file into an existing image

The implementation is designed as an educational filesystem project and demonstrates on-disk metadata layout, inode management, directory entries, bitmaps, and simple CRC-based integrity checks.

---

## Project Overview

This repository models a very small filesystem that stores:

- a superblock
- inode bitmap
- data bitmap
- inode table
- root directory entries
- file data blocks

The filesystem uses a fixed block size of 4096 bytes and stores metadata using compact packed C structs.

---

## Files in This Repository

### 1) mkfs_builder.c
This program initializes a disk image with a valid filesystem layout.

It creates a blank image with:

- valid superblock values
- inode bitmap initialized for root inode
- data bitmap initialized
- inode table for the root directory
- root directory entries for `.` and `..`
- zero-filled data region for the remaining blocks

This tool is the equivalent of a minimal `mkfs` implementation for the custom filesystem.

### 2) mkfs_adder.c
This program loads an existing image, verifies the filesystem header and metadata, then adds a regular file to the root directory.

It:

- validates the image magic number and block size
- finds a free inode
- finds free data blocks
- writes file data into the data region
- populates a directory entry in the root directory
- updates metadata and CRC fields
- writes the modified image back to disk

---

## Filesystem Design

### Block Size

- `BS = 4096`

### Inode Size

- `INODE_SIZE = 128`

### Maximum Direct Pointers

- `MAX_DIRECT_PTR = 12`

### Maximum File Name Length

- `MAX_FILE_NAME_SZ = 58`

This means filenames are limited to 58 characters, excluding the terminating null byte.

---

## On-Disk Structures

The project uses packed structs to keep the on-disk layout compact and predictable.

### Superblock

The filesystem stores a `superblock_t` with fields such as:

- magic value
- version
- block size
- total block count
- inode count
- inode bit start / block count
- data bit start / block count
- inode table start / block count
- data region start / block count
- root inode number
- timestamp
- flags
- checksum

The checksum is computed using a CRC32 routine over the superblock contents.

### Inode

Each inode is represented by `inode_t` and includes:

- file mode
- link count
- UID/GID
- file size
- timestamps
- 12 direct block pointers
- reserved fields
- CRC metadata

### Directory Entry

Each entry is represented by `dirent64_t` and includes:

- inode number
- entry type
- file name
- checksum

The root directory is initialized with entries for:

- `.`
- `..`

Additional files are inserted as root-level entries.

---

## Filesystem Layout

The image layout created by `mkfs_builder.c` follows this structure:

1. Block 0: Superblock
2. Block 1: Inode bitmap
3. Block 2: Data bitmap
4. Block 3 onwards: Inode table
5. Data region start: Root directory data block and other file blocks

The code calculates the positions as follows:

- inode bitmap start = 1
- data bitmap start = 2
- inode table start = 3
- data region start = inode table end + 1

---

## Usage

### Build the tools

On a Linux/macOS environment with GCC installed:

```bash
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
```

On Windows, the same build can be run using MinGW, MSYS2, or a compatible GCC toolchain.

---

### Create a filesystem image

```bash
./mkfs_builder --image mini.img --size-kib 180 --inodes 128
```

Arguments:

- `--image`: output image file path
- `--size-kib`: image size in KiB
- `--inodes`: number of inodes to allocate

Accepted ranges in the implementation:

- size: `180` to `4096` KiB
- inodes: `128` to `512`

This creates a valid empty filesystem image.

---

### Add a file to the image

```bash
./mkfs_adder --input mini.img --output mini_with_file.img --file sample.txt
```

Arguments:

- `--input`: existing image file
- `--output`: output image file
- `--file`: filename to add from the host filesystem

The file is copied into the data region, its inode is created, and a root directory entry is appended.

---

## Example Workflow

```bash
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder

./mkfs_builder --image myfs.img --size-kib 512 --inodes 256
./mkfs_adder --input myfs.img --output myfs_updated.img --file hello.txt
```

This produces a filesystem image containing a root directory with the file `hello.txt`.

---

## Important Behavior and Constraints

### Root directory

The root directory is created automatically and contains:

- `.`
- `..`

### File size constraints

The implementation supports only direct blocks. A file may require at most 12 blocks because of the direct pointer array, so:

```text
maximum file size ≈ 12 * 4096 bytes
```

If the file requires more than 12 blocks, the program rejects it.

### File name constraints

Filenames are stored in a `char[58]` field. Long names are truncated to fit.

### Directory capacity

The root directory uses a fixed block size and stores entries as `dirent64_t` structs. Each directory entry is 64 bytes, so the root directory can hold:

```text
4096 / 64 = 64 entries
```

### Single-level directory model

This project supports files in the root directory only. There is no full subdirectory hierarchy or recursive path handling.

---

## Data Integrity and Validation

The project includes helper functions for integrity checking:

- CRC32 table generation
- superblock checksum generation
- inode CRC calculation
- directory entry checksum generation

These checks are used to ensure metadata consistency when the filesystem is constructed and updated.

---

## Limitations

This project is intentionally small and educational. It does not include:

- subdirectories beyond the root
- nested file paths
- journaling
- extents or indirect blocks
- real permissions management beyond basic inode fields
- a full shell or mountable filesystem driver
- advanced allocation strategies

It is best understood as a simplified custom filesystem prototype.

---

## Build Notes

The source code uses standard C features and depends on:

- `stdio.h`
- `stdlib.h`
- `stdint.h`
- `string.h`
- `time.h`
- `assert.h`

The code is written with `-std=c17` and includes warnings enabled with `-Wall -Wextra`.

---

## Example of the Internal Flow

### mkfs_builder.c flow

1. Parse CLI arguments
2. Validate image size and inode count
3. Compute block boundaries
4. Initialize a superblock
5. Generate inode and data bitmaps
6. Set up the root inode
7. Create the root directory entries
8. Write the final image to disk

### mkfs_adder.c flow

1. Open input image
2. Validate superblock and metadata
3. Find a free inode and data blocks
4. Read the host file into memory
5. Write file bytes into the chosen blocks
6. Create a directory entry for the file in the root directory
7. Update inode metadata and filesystem timestamps
8. Write the updated image to the output file

---

## Why This Project Matters

This project is useful for learning:

- filesystem layout and block allocation
- inode metadata design
- directory entry structure
- bitmap-based allocation
- disk-image simulation
- simple low-level C programming

It is a great example of how filesystems organize metadata and payload data on a raw disk image.

---

## License

This project does not currently include a formal license file. If you are publishing or distributing it publicly, it is recommended to add an appropriate open-source license such as MIT or Apache 2.0.

---

## Summary

MiniVFS is a compact, educational disk-image filesystem project that introduces the fundamentals of filesystem construction and file insertion in C. It is ideal for understanding how a filesystem stores metadata, allocates blocks, and organizes files in a simple root directory.

---

## Quick Reference

```bash
# Build
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder

# Create image
./mkfs_builder --image mini.img --size-kib 180 --inodes 128

# Add file
./mkfs_adder --input mini.img --output mini_with_file.img --file sample.txt
```

If you want, this repository can also be expanded with:

- a `LICENSE` file
- a screenshot or diagram of the disk layout
- a sample `test.sh` script
- a more advanced README with architecture diagrams
- support for subdirectories and nested paths
