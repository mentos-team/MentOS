/// @file fault_injection.h
/// @brief Deliberate failure of block-device transfers, for testing.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// @details The storage error paths cannot be tested without this. A sector
/// read fails on real hardware about once in a few hundred boots (#291), which
/// is often enough to matter and far too rare to write a test against, so every
/// fix to those paths has had to ship verified only by inspection and by the
/// success path continuing to work. That is the gap this closes: with the
/// failure arriving on demand, the code above the block layer can be exercised
/// exactly as a failing device would exercise it (#338).
///
/// Compiled in only under -DENABLE_ATA_FAULT_INJECTION=ON. When the option is
/// off the calls are macros that evaluate to 0, so the default build carries
/// neither the branch nor the state.
///
/// Armed through /proc/faultinj:
///
///     echo "read 1"          > /proc/faultinj  # fail the next sector read
///     echo "read 3 sector 40" > /proc/faultinj  # fail reads of sector 40 only
///     echo "read 3 sector 40 skip 1" > /proc/faultinj  # ... after letting one through
///     echo "write 2"         > /proc/faultinj  # fail the next two writes
///     echo "off"             > /proc/faultinj  # disarm
///     cat /proc/faultinj                       # armed, fired, last sector read

#pragma once

#include "stdint.h"

#ifdef ENABLE_ATA_FAULT_INJECTION

/// @brief Reports whether a read of the given sector should be failed.
/// @param lba_sector the sector about to be read.
/// @return the negative errno to fail with, or 0 to let the read proceed.
/// @details Consumes one of the armed failures when it returns non-zero, and
///          records the sector so a test can discover which one carries the
///          metadata it wants to fail. Arming a specific sector is the only
///          way to reach a read-modify-write: a count alone is spent by
///          whatever reads first, which is path resolution.
int ata_fault_inject_read(uint32_t lba_sector);

/// @brief Reports whether the next sector write should be failed.
/// @return the negative errno to fail with, or 0 to let the write proceed.
/// @details Consumes one of the armed failures when it returns non-zero.
int ata_fault_inject_write(void);

/// @brief Arms or disarms the injection.
/// @param reads how many subsequent reads to fail.
/// @param writes how many subsequent writes to fail.
/// @param sector the sector to restrict read failures to, or 0 for any.
/// @param skip how many matching reads to let through before failing any.
/// @details `skip` is what makes a read-modify-write reachable. Every path
///          that writes a metadata block reads something from the same block
///          first, through a function that already checks its read, so an
///          unskipped failure stops the operation before it gets to the
///          write-back. Skipping the first match lets that earlier read
///          succeed and fails the one that matters.
void ata_fault_inject_arm(unsigned reads, unsigned writes, uint32_t sector, unsigned skip);

/// @brief Registers `/proc/faultinj`.
/// @return 0 on success, 1 on failure.
int procfaultinj_module_init(void);

#else

/// The option is off: no state, no branch, no cost.
#define ata_fault_inject_read(sector) 0
#define ata_fault_inject_write()      0

#endif
