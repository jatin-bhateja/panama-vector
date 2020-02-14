/*
* Copyright (c) 2019, 2020 Oracle and/or its affiliates. All rights reserved.
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

#ifndef QBA_hpp
#define QBA_hpp

#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------------//
//
// QBA is an experimental project and not currently intended to be used in
// a production environment.
//
// Contact: panama-dev@openjdk.java.net
//

//----------------------------------------------------------------------------//
//
// The QBA API provides a very fast alternative to the standard library
// malloc-free. All calls in the QBA API are thread/process safe.
//

//----------------------------------------------------------------------------//
//
// qba_version returns the version of the QBA encoded as an integer.
//
// bits 31-24: Unused.
// bits 23-16: Release number.
// bits 15-8:  Major number.
// bits 7-0:   Minor number.
//
int qba_version();

//
// qba_version_string returns version information as an informative C string.
//
const char *qba_version_string();

//
// Opaque reference to qba instance.
//
typedef struct qba_t qba_t;

//----------------------------------------------------------------------------//

//
// qba_create returns a reference to a new qba instance.
//
qba_t *qba_create(
    intptr_t address,         // Fixed base address for allocations.
    const char* linkName,     // Shared link name.
    bool secure,              // true if allocations are zeroed.
    int smallPartitionCount,  // Partition count for small sized allocations.
    int mediumPartitionCount, // Partition count for medium sized allocations.
    int largePartitionCount,  // Partition count for large sized allocations.
    int maxSlabCount,         // Maximum number of slabs.
    int sideDataSize          // Size of size data.
);

//
// qba_create_size returns the number of bytes required to create the qba
// instance.
//
size_t qba_create_size(
    bool secure,              // true if allocations are zeroed.
    int smallPartitionCount,  // Partition count for small sized allocations.
    int mediumPartitionCount, // Partition count for medium sized allocations.
    int largePartitionCount,  // Partition count for large sized allocations.
    int maxSlabCount,         // Maximum number of slabs.
    int sideDataSize          // Size of size data.
);

//
// qba_destroy returns space used by a qba instance back to the system.
//
void qba_destroy(
    qba_t *qba,  // Reference to qba instance.
    bool unlink  // Unlink shared linl.
);

//
// qba_get_reference returns the current user reference.
//
void *qba_get_reference(
    qba_t *qba  // Reference to qba instance.
);

//
// qba_set_reference conditionally sets the user reference. Returns true if
// successful.
//
bool qba_set_reference(
    qba_t *qba,     // Reference to qba instance.
    void *oldValue, // Expected old value.
    void *newValue  // Value to set.
);

//
// qba_allocate returns a memory block address of size equal to or greater than
// "size" bytes. qba_allocate will return nullptr if the size is zero or if it
// is unable to allocate a block of that size. The allocated block should be
// recycled by invoking qba_deallocate or qba_reallocate.
//
void *qba_allocate(
    qba_t *qba,   // Reference to qba instance.
    uint64_t size // Number of bytes required to satisfy the request.
);

//
// qba_deallocate recycles a memory block previously allocated by qba_allocate
// or qba_reallocate. If the supplied address is nullptr or not in memory
// managed by QBA then qba_deallocate does nothing.
//
void qba_deallocate(
    qba_t *qba,   // Reference to qba instance.
    void *address // Address of an allocated memory block or nullptr.
);

//
// qba_reallocate ensures that the memory block returned is sized equal to or
// greater than "size" bytes. If the original block fits then the old block
// "address" is returned. If original block is nullptr, smaller or significantly
// larger then a new memory block is allocated, the contents of the old block
// copied (if not nullptr) to the new block, the old block deallocated and the
// new block address returned. The original block size is based on the size
// returned by qba_size. May return zero if unable to allocate the new block
// (old block not deallocated.)
//
void *qba_reallocate(
    qba_t *qba,    // Reference to qba instance.
    void *address, // Address of an allocated memory block or nullptr.
    uint64_t size  // Number of bytes required to satisfy the request.
);

//
// qba_clear zeroes out the content of a memory block previously allocated by
// qba_allocate or qba_reallocate.
//
void qba_clear(
    qba_t *qba,    // Reference to qba instance.
    void *address  // Address of an allocated memory block or nullptr.
);

//
// qba_size returns the number of bytes allocated to a memory block. This value
// may exceed the size of the original request due to rounding. Zero is returned
// if the supplied address is nullptr or not in memory managed by QBA.
//
size_t qba_size(
    qba_t *qba,   // Reference to qba instance.
    void *address // Address of an allocated memory block.
);

//
// qba_base recovers the base allocation address from any arbitrary address in a
// memory block. nullptr is returned if the supplied address is nullptr or not
// in memory managed by QBA.
//
void *qba_base(
    qba_t *qba,   // Reference to qba instance.
    void *address // Arbitrary address in an allocated memory block.
);

//
// qba_side_data returns the address of side data corresponding to an allocated
// memory block. The size of the side data is configuration specific.
// qba_side_data may return nullptr if the supplied address is nullptr or not in
// memory managed by QBA.
//
void *qba_side_data(
    qba_t *qba,   // Reference to qba instance.
    void *address // Address of an allocated memory block.
);

//
// qba_next can be used to "walk" through all the allocations managed by QBA.
// The first call should have an "address" of nullptr with successive calls
// using the result of the previous call. The result itself can not be used for
// memory access since the result may have been deallocated after fetching
// (potential seg fault). The result can however be used to when invoking
// qba_size or qba_side_data. A result of zero indicates no further blocks.
//
void *qba_next(
    qba_t *qba,   // Reference to qba instance.
    void *address // Address of an allocated memory block.
);

//
// qba_stats can be used to sample the current allocation state of QBA. The
// arguments are two uint64_t arrays of length QB_STATS_SIZE. The counts array
// receives the allocation count in each category and sizes array receives the
// allocation size in each category. Categories are as follows;
//
//     Slot 0 - Sum of all other slots.
//     Slot 1 - Maximums of administrative data (not necessarily active.)
//     Slot 2 - Unused.
//     Slot 3-52 - Totals for blocks sized 2^slot.
//     Slot 53 and above - Unused.
//
// Slot 0 is likely the most interesting but if the count of 16 byte allocations
// is required, for example, then use counts[4] (2^4 == 16).
//
#define QB_STATS_SIZE 64
void qba_stats(
    qba_t *qba,       // Reference to qba instance.
    uint64_t *counts, // Array that will receive allocation counts.
    uint64_t *sizes   // Array that will receive allocation sizes.
);

//----------------------------------------------------------------------------//
//
// qba_allocate_bulk quickly allocates "count" memory blocks of "size" bytes.
// qba_allocate_bulk is useful for allocating memory block pools.
// qba_allocate_bulk returns the number of addresses actually allocated, which
// may be zero or less than count depending on conditions. The "contiguous" flag
// indicates that allocations should be contiguous. Contiguous is faster with
// the risk of creating external fragmentation or not completing the request
// satisfactorily (result of zero.)
//
int qba_allocate_bulk(
    qba_t *qba,   // Reference to qba instance.
    uint64_t size,    // Requested number of bytes required per block.
    int count,        // Requested number of blocks.
    void** addresses, // Address buffer to receive the block addresses.
    bool contiguous   // True if the blocks should be contiguous.
);

//
// qba_deallocate_bulk quickly deallocates multiple memory blocks.
// qba_deallocate_bulk is useful for discarding memory block pools, but can also
// be applied to other allocations.
//
void qba_deallocate_bulk(
    qba_t *qba,      // Reference to qba instance.
    int count,       // Number of addresses in the address buffer.
    void** addresses // Address buffer containing allocated memory block
                     // addresses.
);

//----------------------------------------------------------------------------//
//
// qba_allocate_count quickly allocates "count" consecutive memory blocks of
// "size" bytes. qba_allocate_count is useful for reducing internal
// fragmentation when the application can fit allocations into smaller
// allocation units and is willing to risk increasing external fragmentation or
// not completing the request satisfactorily (result of nullptr.) Only
// qba_deallocate_count and qba_side_data can be invoked with an allocation made
// using qba_allocate_count.
//
void *qba_allocate_count(
    qba_t *qba,    // Reference to qba instance.
    uint64_t size, // Power of two size in bytes of memory blocks.
    int count      // Number of consecutive memory blocks required.
);

//
// qba_deallocate_count recycles memory blocks allocated using
// qba_allocate_count.
//
void qba_deallocate_count(
    qba_t *qba,    // Reference to qba instance.
    void *address, // Allocation address returned by qba_allocate_count.
    uint64_t size, // Power of two size in bytes of memory blocks.
    int count      // Number of consecutive memory blocks allocated.
);

//----------------------------------------------------------------------------//
//
// qba_allocate_fit allocates enough memory blocks necessary to keep internal
// fragmentation to a specified degree.
//
// degree 1 = 25% average fragmentation (same as allocate.)
//        2 = 12.5%
//        3 = 6.25%
//        4 = 3.125%
//
// qba_allocate_fit is useful for reducing internal fragmentation when the
// application can fit allocations into smaller allocation units and is willing
// to risk increasing external fragmentation or not completing the request
// satisfactorily (result of nullptr.) Only qba_deallocate_fit and qba_side_data
// can be invoked with an allocation made using qba_allocate_fit.
//
void *qba_allocate_fit(
    qba_t *qba,    // Reference to qba instance.
    uint64_t size, // Number of bytes required to satisfy the request.
    int degree     // Degree of acceptable internal fragmentation.
);

//
// qba_deallocate_fit recycles memory blocks allocated using qba_allocate_fit.
//
void qba_deallocate_fit(
    qba_t *qba,    // Reference to qba instance.
    void *address, // Allocation address returned by qba_allocate_fit.
    uint64_t size, // Number of bytes required to satisfy the request.
    int degree     // Degree of acceptable internal fragmentation.
);

//----------------------------------------------------------------------------//
#ifdef __cplusplus
}
#endif

#endif /* QBA_hpp */
