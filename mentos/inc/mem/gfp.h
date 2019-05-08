///                MentOS, The Mentoring Operating system project
/// @file gfp.h
/// @brief List of GFP Flags.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// Type used for GFP_FLAGS.
typedef unsigned int gfp_t;

// Plain integer GFP bitmasks. Do not use this directly.
#define ___GFP_DMA 0x01u

#define ___GFP_HIGHMEM 0x02u

#define ___GFP_DMA32 0x04u

#define ___GFP_RECLAIMABLE 0x10u

#define ___GFP_HIGH 0x20u

#define ___GFP_IO 0x40u

#define ___GFP_FS 0x80u

#define ___GFP_ZERO 0x100u

#define ___GFP_ATOMIC 0x200u

#define ___GFP_DIRECT_RECLAIM 0x400u

#define ___GFP_KSWAPD_RECLAIM 0x800u

/*
 * Physical address zone modifiers (see linux/mmzone.h - low four bits)
 *
 * Do not put any conditional on these. If necessary modify the definitions
 * without the underscores and use them consistently. The definitions here may
 * be used in bit comparisons.
 */
#define __GFP_DMA ___GFP_DMA

#define __GFP_HIGHMEM ___GFP_HIGHMEM

#define __GFP_DMA32 ___GFP_DMA32

#define GFP_ZONEMASK (__GFP_DMA | __GFP_HIGHMEM | __GFP_DMA32)

/**
 * DOC: Watermark modifiers
 *
 * Watermark modifiers -- controls access to emergency reserves
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * %__GFP_HIGH indicates that the caller is high-priority and that granting
 * the request is necessary before the system can make forward progress.
 * For example, creating an IO context to clean pages.
 *
 * %__GFP_ATOMIC indicates that the caller cannot reclaim or sleep and is
 * high priority. Users are typically interrupt handlers. This may be
 * used in conjunction with %__GFP_HIGH
 */
#define __GFP_ATOMIC ___GFP_ATOMIC

#define __GFP_HIGH ___GFP_HIGH

/**
 * DOC: Reclaim modifiers
 *
 * Reclaim modifiers
 * ~~~~~~~~~~~~~~~~~
 *
 * %__GFP_IO can start physical IO.
 *
 * %__GFP_FS can call down to the low-level FS. Clearing the flag avoids the
 * allocator recursing into the filesystem which might already be holding
 * locks.
 *
 * %__GFP_DIRECT_RECLAIM indicates that the caller may enter direct reclaim.
 * This flag can be cleared to avoid unnecessary delays when a fallback
 * option is available.
 *
 * %__GFP_KSWAPD_RECLAIM indicates that the caller wants to wake kswapd when
 * the low watermark is reached and have it reclaim pages until the high
 * watermark is reached. A caller may wish to clear this flag when fallback
 * options are available and the reclaim is likely to disrupt the system. The
 * canonical example is THP allocation where a fallback is cheap but
 * reclaim/compaction may cause indirect stalls.
 *
 * %__GFP_RECLAIM is shorthand to allow/forbid both direct and kswapd reclaim.
 */
#define __GFP_IO ___GFP_IO

#define __GFP_FS ___GFP_FS
// Caller can reclaim.
#define __GFP_DIRECT_RECLAIM ___GFP_DIRECT_RECLAIM

// Kswapd can wake.
#define __GFP_KSWAPD_RECLAIM ___GFP_KSWAPD_RECLAIM

#define __GFP_RECLAIM (___GFP_DIRECT_RECLAIM | ___GFP_KSWAPD_RECLAIM)

/**
 * DOC: Useful GFP flag combinations
 *
 * Useful GFP flag combinations
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Useful GFP flag combinations that are commonly used. It is recommended
 * that subsystems start with one of these combinations and then set/clear
 * %__GFP_FOO flags as necessary.
 *
 * %GFP_ATOMIC users can not sleep and need the allocation to succeed. A lower
 * watermark is applied to allow access to "atomic reserves"
 *
 * %GFP_KERNEL is typical for kernel-internal allocations. The caller requires
 * %ZONE_NORMAL or a lower zone for direct access but can direct reclaim.
 *
 * %GFP_NOWAIT is for kernel allocations that should not stall for direct
 * reclaim, start physical IO or use any filesystem callback.
 *
 * %GFP_NOIO will use direct reclaim to discard clean pages or slab pages
 * that do not require the starting of any physical IO.
 * Please try to avoid using this flag directly and instead use
 * memalloc_noio_{save,restore} to mark the whole scope which cannot
 * perform any IO with a short explanation why. All allocation requests
 * will inherit GFP_NOIO implicitly.
 *
 * %GFP_NOFS will use direct reclaim but will not use any filesystem interfaces.
 * Please try to avoid using this flag directly and instead use
 * memalloc_nofs_{save,restore} to mark the whole scope which cannot/shouldn't
 * recurse into the FS layer with a short explanation why. All allocation
 * requests will inherit GFP_NOFS implicitly.
 *
 * %GFP_USER is for userspace allocations that also need to be directly
 * accessibly by the kernel or hardware. It is typically used by hardware
 * for buffers that are mapped to userspace (e.g. graphics) that hardware
 * still must DMA to. cpuset limits are enforced for these allocations.
 *
 * %GFP_DMA exists for historical reasons and should be avoided where possible.
 * The flags indicates that the caller requires that the lowest zone be
 * used (%ZONE_DMA or 16M on x86-64). Ideally, this would be removed but
 * it would require careful auditing as some users really require it and
 * others use the flag to avoid lowmem reserves in %ZONE_DMA and treat the
 * lowest zone as a type of emergency reserve.
 *
 * %GFP_HIGHUSER is for userspace allocations that may be mapped to userspace,
 * do not need to be directly accessible by the kernel but that cannot
 * move once in use. An example may be a hardware allocation that maps
 * data directly into userspace but has no addressing limitations.
 */
#define GFP_ATOMIC (__GFP_HIGH | __GFP_ATOMIC | __GFP_KSWAPD_RECLAIM)

#define GFP_KERNEL (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

#define GFP_NOWAIT (__GFP_KSWAPD_RECLAIM)

#define GFP_NOIO (__GFP_RECLAIM)

#define GFP_NOFS (__GFP_RECLAIM | __GFP_IO)

#define GFP_USER (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

#define GFP_DMA (__GFP_DMA)

#define GFP_HIGHUSER (GFP_USER | __GFP_HIGHMEM)
