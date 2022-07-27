/// @file gfp.h
/// @brief List of Get Free Pages (GFP) Flags.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Type used for GFP_FLAGS.
typedef unsigned int gfp_t;

/// @defgroup GFP The Get Free Pages (GFP) flags
/// @brief Sets of GFP defines.
/// @{

/// @defgroup gfp_bitmasks Bitmasks
/// @brief Plain integer GFP bitmasks. Do not use this directly.
/// @{

#define ___GFP_DMA            0x001U ///< DMA
#define ___GFP_HIGHMEM        0x002U ///< HIGHMEM
#define ___GFP_DMA32          0x004U ///< DMA32
#define ___GFP_RECLAIMABLE    0x010U ///< RECLAIMABLE
#define ___GFP_HIGH           0x020U ///< HIGH
#define ___GFP_IO             0x040U ///< IO
#define ___GFP_FS             0x080U ///< FS
#define ___GFP_ZERO           0x100U ///< ZERO
#define ___GFP_ATOMIC         0x200U ///< ATOMIC
#define ___GFP_DIRECT_RECLAIM 0x400U ///< DIRECT_RECLAIM
#define ___GFP_KSWAPD_RECLAIM 0x800U ///< KSWAPD_RECLAIM

/// @}

/// @defgroup zone_modifiers Zone Modifiers
/// @brief Physical address zone modifiers (see linux/mmzone.h - low four bits)
/// @details
/// Do not put any conditional on these. If necessary modify the definitions
/// without the underscores and use them consistently. The definitions here may
/// be used in bit comparisons.
/// @{

#define __GFP_DMA     ___GFP_DMA ///< DMA
#define __GFP_HIGHMEM ___GFP_HIGHMEM ///< HIGHMEM
#define __GFP_DMA32   ___GFP_DMA32 ///< DMA32
/// All of the above.
#define GFP_ZONEMASK (__GFP_DMA | __GFP_HIGHMEM | __GFP_DMA32)

/// @}

/// @defgroup WatermarkModifiers Watermark Modifiers
/// @brief Controls access to emergency reserves.
/// @{

/// @brief Indicates that the caller cannot reclaim or sleep and is
/// high priority. Users are typically interrupt handlers. This may be
/// used in conjunction with %__GFP_HIGH.
#define __GFP_ATOMIC ___GFP_ATOMIC

/// @brief Indicates that the caller is high-priority and that granting
/// the request is necessary before the system can make forward progress.
/// For example, creating an IO context to clean pages.
#define __GFP_HIGH ___GFP_HIGH

/// @}

/// @defgroup ReclaimModifiers Reclaim Modifiers
/// @brief Enable reclaim operations on a specific region of memory.
/// @{

/// @brief Can start physical IO.
#define __GFP_IO ___GFP_IO

/// @brief Can call down to the low-level FS. Clearing the flag avoids the
/// allocator recursing into the filesystem which might already be holding
/// locks.
#define __GFP_FS ___GFP_FS

/// @brief Indicates that the caller may enter direct reclaim.
/// This flag can be cleared to avoid unnecessary delays when a fallback
/// option is available.
#define __GFP_DIRECT_RECLAIM ___GFP_DIRECT_RECLAIM

/// @brief Indicates that the caller wants to wake kswapd when
/// the low watermark is reached and have it reclaim pages until the high
/// watermark is reached. A caller may wish to clear this flag when fallback
/// options are available and the reclaim is likely to disrupt the system. The
/// canonical example is THP allocation where a fallback is cheap but
/// reclaim/compaction may cause indirect stalls.
#define __GFP_KSWAPD_RECLAIM ___GFP_KSWAPD_RECLAIM

/// @brief Is shorthand to allow/forbid both direct and kswapd reclaim.
#define __GFP_RECLAIM (___GFP_DIRECT_RECLAIM | ___GFP_KSWAPD_RECLAIM)

/// @}

/// @defgroup gfp_flag_combinations Flag Combinations
/// @brief Useful GFP flag combinations.
/// @details
/// Useful GFP flag combinations that are commonly used. It is recommended
/// that subsystems start with one of these combinations and then set/clear
/// the flags as necessary.
/// @{

/// @brief Users can not sleep and need the allocation to succeed. A lower
/// watermark is applied to allow access to "atomic reserves"
#define GFP_ATOMIC (__GFP_HIGH | __GFP_ATOMIC | __GFP_KSWAPD_RECLAIM)

/// @brief is typical for kernel-internal allocations. The caller requires
/// %ZONE_NORMAL or a lower zone for direct access but can direct reclaim.
#define GFP_KERNEL (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

/// @brief is for kernel allocations that should not stall for direct
/// reclaim, start physical IO or use any filesystem callback.
#define GFP_NOWAIT (__GFP_KSWAPD_RECLAIM)

/// @brief will use direct reclaim to discard clean pages or slab pages
/// that do not require the starting of any physical IO.
/// Please try to avoid using this flag directly and instead use
/// memalloc_noio_{save,restore} to mark the whole scope which cannot
/// perform any IO with a short explanation why. All allocation requests
/// will inherit GFP_NOIO implicitly.
#define GFP_NOIO (__GFP_RECLAIM)

/// @brief will use direct reclaim but will not use any filesystem interfaces.
/// Please try to avoid using this flag directly and instead use
/// memalloc_nofs_{save,restore} to mark the whole scope which cannot/shouldn't
/// recurse into the FS layer with a short explanation why. All allocation
/// requests will inherit GFP_NOFS implicitly.
#define GFP_NOFS (__GFP_RECLAIM | __GFP_IO)

/// @brief is for userspace allocations that also need to be directly
/// accessibly by the kernel or hardware. It is typically used by hardware
/// for buffers that are mapped to userspace (e.g. graphics) that hardware
/// still must DMA to. cpuset limits are enforced for these allocations.
#define GFP_USER (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

/// @brief exists for historical reasons and should be avoided where possible.
/// The flags indicates that the caller requires that the lowest zone be
/// used (%ZONE_DMA or 16M on x86-64). Ideally, this would be removed but
/// it would require careful auditing as some users really require it and
/// others use the flag to avoid lowmem reserves in %ZONE_DMA and treat the
/// lowest zone as a type of emergency reserve.
#define GFP_DMA (__GFP_DMA)

/// @brief is for userspace allocations that may be mapped to userspace,
/// do not need to be directly accessible by the kernel but that cannot
/// move once in use. An example may be a hardware allocation that maps
/// data directly into userspace but has no addressing limitations.
#define GFP_HIGHUSER (GFP_USER | __GFP_HIGHMEM)

/// @}

/// @}
