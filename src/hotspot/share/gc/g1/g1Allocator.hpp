/*
 * Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_GC_G1_G1ALLOCATOR_HPP
#define SHARE_VM_GC_G1_G1ALLOCATOR_HPP

#include "gc/g1/g1AllocRegion.hpp"
#include "gc/g1/g1AllocationContext.hpp"
#include "gc/g1/g1InCSetState.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/plab.hpp"

class EvacuationInfo;

// Interface to keep track of which regions G1 is currently allocating into. Provides
// some accessors (e.g. allocating into them, or getting their occupancy).
// Also keeps track of retained regions across GCs.
class G1Allocator : public CHeapObj<mtGC> {
  friend class VMStructs;
protected:
  G1CollectedHeap* _g1h;

  virtual MutatorAllocRegion* mutator_alloc_region(AllocationContext_t context) = 0;

  virtual bool survivor_is_full(AllocationContext_t context) const = 0;
  virtual bool old_is_full(AllocationContext_t context) const = 0;

  virtual void set_survivor_full(AllocationContext_t context) = 0;
  virtual void set_old_full(AllocationContext_t context) = 0;

  // Accessors to the allocation regions.
  virtual SurvivorGCAllocRegion* survivor_gc_alloc_region(AllocationContext_t context) = 0;
  virtual OldGCAllocRegion* old_gc_alloc_region(AllocationContext_t context) = 0;

  // Allocation attempt during GC for a survivor object / PLAB.
  inline HeapWord* survivor_attempt_allocation(size_t min_word_size,
                                               size_t desired_word_size,
                                               size_t* actual_word_size,
                                               AllocationContext_t context);
  // Allocation attempt during GC for an old object / PLAB.
  inline HeapWord* old_attempt_allocation(size_t min_word_size,
                                          size_t desired_word_size,
                                          size_t* actual_word_size,
                                          AllocationContext_t context);
public:
  G1Allocator(G1CollectedHeap* heap) : _g1h(heap) { }
  virtual ~G1Allocator() { }

#ifdef ASSERT
  // Do we currently have an active mutator region to allocate into?
  bool has_mutator_alloc_region(AllocationContext_t context) { return mutator_alloc_region(context)->get() != NULL; }
#endif
  virtual void init_mutator_alloc_region() = 0;
  virtual void release_mutator_alloc_region() = 0;

  virtual void init_gc_alloc_regions(EvacuationInfo& evacuation_info) = 0;
  virtual void release_gc_alloc_regions(EvacuationInfo& evacuation_info) = 0;
  virtual void abandon_gc_alloc_regions() = 0;

  // Management of retained regions.

  virtual bool is_retained_old_region(HeapRegion* hr) = 0;
  void reuse_retained_old_region(EvacuationInfo& evacuation_info,
                                 OldGCAllocRegion* old,
                                 HeapRegion** retained);

  // Allocate blocks of memory during mutator time.

  inline HeapWord* attempt_allocation(size_t word_size, AllocationContext_t context);
  inline HeapWord* attempt_allocation_locked(size_t word_size, AllocationContext_t context);
  inline HeapWord* attempt_allocation_force(size_t word_size, AllocationContext_t context);

  size_t unsafe_max_tlab_alloc(AllocationContext_t context);

  // Allocate blocks of memory during garbage collection. Will ensure an
  // allocation region, either by picking one or expanding the
  // heap, and then allocate a block of the given size. The block
  // may not be a humongous - it must fit into a single heap region.
  HeapWord* par_allocate_during_gc(InCSetState dest,
                                   size_t word_size,
                                   AllocationContext_t context);

  HeapWord* par_allocate_during_gc(InCSetState dest,
                                   size_t min_word_size,
                                   size_t desired_word_size,
                                   size_t* actual_word_size,
                                   AllocationContext_t context);

  virtual size_t used_in_alloc_regions() = 0;
};

// The default allocation region manager for G1. Provides a single mutator, survivor
// and old generation allocation region.
// Can retain the (single) old generation allocation region across GCs.
class G1DefaultAllocator : public G1Allocator {
private:
  bool _survivor_is_full;
  bool _old_is_full;
protected:
  // Alloc region used to satisfy mutator allocation requests.
  MutatorAllocRegion _mutator_alloc_region;

  // Alloc region used to satisfy allocation requests by the GC for
  // survivor objects.
  SurvivorGCAllocRegion _survivor_gc_alloc_region;

  // Alloc region used to satisfy allocation requests by the GC for
  // old objects.
  OldGCAllocRegion _old_gc_alloc_region;

  HeapRegion* _retained_old_gc_alloc_region;
public:
  G1DefaultAllocator(G1CollectedHeap* heap);

  virtual bool survivor_is_full(AllocationContext_t context) const;
  virtual bool old_is_full(AllocationContext_t context) const ;

  virtual void set_survivor_full(AllocationContext_t context);
  virtual void set_old_full(AllocationContext_t context);

  virtual void init_mutator_alloc_region();
  virtual void release_mutator_alloc_region();

  virtual void init_gc_alloc_regions(EvacuationInfo& evacuation_info);
  virtual void release_gc_alloc_regions(EvacuationInfo& evacuation_info);
  virtual void abandon_gc_alloc_regions();

  virtual bool is_retained_old_region(HeapRegion* hr) {
    return _retained_old_gc_alloc_region == hr;
  }

  virtual MutatorAllocRegion* mutator_alloc_region(AllocationContext_t context) {
    return &_mutator_alloc_region;
  }

  virtual SurvivorGCAllocRegion* survivor_gc_alloc_region(AllocationContext_t context) {
    return &_survivor_gc_alloc_region;
  }

  virtual OldGCAllocRegion* old_gc_alloc_region(AllocationContext_t context) {
    return &_old_gc_alloc_region;
  }

  virtual size_t used_in_alloc_regions() {
    assert(Heap_lock->owner() != NULL,
           "Should be owned on this thread's behalf.");
    size_t result = 0;

    // Read only once in case it is set to NULL concurrently
    HeapRegion* hr = mutator_alloc_region(AllocationContext::current())->get();
    if (hr != NULL) {
      result += hr->used();
    }
    return result;
  }
};

// Manages the PLABs used during garbage collection. Interface for allocation from PLABs.
// Needs to handle multiple contexts, extra alignment in any "survivor" area and some
// statistics.
class G1PLABAllocator : public CHeapObj<mtGC> {
  friend class G1ParScanThreadState;
protected:
  G1CollectedHeap* _g1h;
  G1Allocator* _allocator;

  // The survivor alignment in effect in bytes.
  // == 0 : don't align survivors
  // != 0 : align survivors to that alignment
  // These values were chosen to favor the non-alignment case since some
  // architectures have a special compare against zero instructions.
  const uint _survivor_alignment_bytes;

  // Number of words allocated directly (not counting PLAB allocation).
  size_t _direct_allocated[InCSetState::Num];

  virtual void flush_and_retire_stats() = 0;
  virtual PLAB* alloc_buffer(InCSetState dest, AllocationContext_t context) = 0;

  // Calculate the survivor space object alignment in bytes. Returns that or 0 if
  // there are no restrictions on survivor alignment.
  static uint calc_survivor_alignment_bytes() {
    assert(SurvivorAlignmentInBytes >= ObjectAlignmentInBytes, "sanity");
    if (SurvivorAlignmentInBytes == ObjectAlignmentInBytes) {
      // No need to align objects in the survivors differently, return 0
      // which means "survivor alignment is not used".
      return 0;
    } else {
      assert(SurvivorAlignmentInBytes > 0, "sanity");
      return SurvivorAlignmentInBytes;
    }
  }

  HeapWord* allocate_new_plab(InCSetState dest,
                              size_t word_sz,
                              AllocationContext_t context);

  bool may_throw_away_buffer(size_t const allocation_word_sz, size_t const buffer_size) const;
public:
  G1PLABAllocator(G1Allocator* allocator);
  virtual ~G1PLABAllocator() { }

  virtual void waste(size_t& wasted, size_t& undo_wasted) = 0;

  // Allocate word_sz words in dest, either directly into the regions or by
  // allocating a new PLAB. Returns the address of the allocated memory, NULL if
  // not successful. Plab_refill_failed indicates whether an attempt to refill the
  // PLAB failed or not.
  HeapWord* allocate_direct_or_new_plab(InCSetState dest,
                                        size_t word_sz,
                                        AllocationContext_t context,
                                        bool* plab_refill_failed);

  // Allocate word_sz words in the PLAB of dest.  Returns the address of the
  // allocated memory, NULL if not successful.
  inline HeapWord* plab_allocate(InCSetState dest,
                                 size_t word_sz,
                                 AllocationContext_t context);

  HeapWord* allocate(InCSetState dest,
                     size_t word_sz,
                     AllocationContext_t context,
                     bool* refill_failed) {
    HeapWord* const obj = plab_allocate(dest, word_sz, context);
    if (obj != NULL) {
      return obj;
    }
    return allocate_direct_or_new_plab(dest, word_sz, context, refill_failed);
  }

  void undo_allocation(InCSetState dest, HeapWord* obj, size_t word_sz, AllocationContext_t context);
};

// The default PLAB allocator for G1. Keeps the current (single) PLAB for survivor
// and old generation allocation.
class G1DefaultPLABAllocator : public G1PLABAllocator {
  PLAB  _surviving_alloc_buffer;
  PLAB  _tenured_alloc_buffer;
  PLAB* _alloc_buffers[InCSetState::Num];

public:
  G1DefaultPLABAllocator(G1Allocator* _allocator);

  virtual PLAB* alloc_buffer(InCSetState dest, AllocationContext_t context) {
    assert(dest.is_valid(),
           "Allocation buffer index out-of-bounds: " CSETSTATE_FORMAT, dest.value());
    assert(_alloc_buffers[dest.value()] != NULL,
           "Allocation buffer is NULL: " CSETSTATE_FORMAT, dest.value());
    return _alloc_buffers[dest.value()];
  }

  virtual void flush_and_retire_stats();

  virtual void waste(size_t& wasted, size_t& undo_wasted);
};

// G1ArchiveRegionMap is a boolean array used to mark G1 regions as
// archive regions.  This allows a quick check for whether an object
// should not be marked because it is in an archive region.
class G1ArchiveRegionMap : public G1BiasedMappedArray<bool> {
protected:
  bool default_value() const { return false; }
};

// G1ArchiveAllocator is used to allocate memory in archive
// regions. Such regions are not scavenged nor compacted by GC.
// There are two types of archive regions, which are
// differ in the kind of references allowed for the contained objects:
//
// - 'Closed' archive region contain no references outside of other
//   closed archive regions. The region is immutable by GC. GC does
//   not mark object header in 'closed' archive region.
// - An 'open' archive region allow references to any other regions,
//   including closed archive, open archive and other java heap regions.
//   GC can adjust pointers and mark object header in 'open' archive region.
class G1ArchiveAllocator : public CHeapObj<mtGC> {
protected:
  bool _open; // Indicate if the region is 'open' archive.
  G1CollectedHeap* _g1h;

  // The current allocation region
  HeapRegion* _allocation_region;

  // Regions allocated for the current archive range.
  GrowableArray<HeapRegion*> _allocated_regions;

  // The number of bytes used in the current range.
  size_t _summary_bytes_used;

  // Current allocation window within the current region.
  HeapWord* _bottom;
  HeapWord* _top;
  HeapWord* _max;

  // Allocate a new region for this archive allocator.
  // Allocation is from the top of the reserved heap downward.
  bool alloc_new_region();

public:
  G1ArchiveAllocator(G1CollectedHeap* g1h, bool open) :
    _g1h(g1h),
    _allocation_region(NULL),
    _allocated_regions((ResourceObj::set_allocation_type((address) &_allocated_regions,
                                                         ResourceObj::C_HEAP),
                        2), true /* C_Heap */),
    _summary_bytes_used(0),
    _bottom(NULL),
    _top(NULL),
    _max(NULL),
    _open(open) { }

  virtual ~G1ArchiveAllocator() {
    assert(_allocation_region == NULL, "_allocation_region not NULL");
  }

  static G1ArchiveAllocator* create_allocator(G1CollectedHeap* g1h, bool open);

  // Allocate memory for an individual object.
  HeapWord* archive_mem_allocate(size_t word_size);

  // Return the memory ranges used in the current archive, after
  // aligning to the requested alignment.
  void complete_archive(GrowableArray<MemRegion>* ranges,
                        size_t end_alignment_in_bytes);

  // The number of bytes allocated by this allocator.
  size_t used() {
    return _summary_bytes_used;
  }

  // Clear the count of bytes allocated in prior G1 regions. This
  // must be done when recalculate_use is used to reset the counter
  // for the generic allocator, since it counts bytes in all G1
  // regions, including those still associated with this allocator.
  void clear_used() {
    _summary_bytes_used = 0;
  }

  // Create the _archive_region_map which is used to identify archive objects.
  static inline void enable_archive_object_check();

  // Set the regions containing the specified address range as archive/non-archive.
  static inline void set_range_archive(MemRegion range, bool open);

  // Check if the object is in closed archive
  static inline bool is_closed_archive_object(oop object);
  // Check if the object is in open archive
  static inline bool is_open_archive_object(oop object);
  // Check if the object is either in closed archive or open archive
  static inline bool is_archive_object(oop object);

private:
  static bool _archive_check_enabled;
  static G1ArchiveRegionMap  _closed_archive_region_map;
  static G1ArchiveRegionMap  _open_archive_region_map;

  // Check if an object is in a closed archive region using the _closed_archive_region_map.
  static inline bool in_closed_archive_range(oop object);
  // Check if an object is in open archive region using the _open_archive_region_map.
  static inline bool in_open_archive_range(oop object);

  // Check if archive object checking is enabled, to avoid calling in_open/closed_archive_range
  // unnecessarily.
  static inline bool archive_check_enabled();
};

#endif // SHARE_VM_GC_G1_G1ALLOCATOR_HPP
