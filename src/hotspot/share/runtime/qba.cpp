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

//----------------------------------------------------------------------------//
//
// QBA is an experimental project and not currently intended to be used in
// a production environment.
//
// Contact: panama-dev@openjdk.java.net
//

//----------------------------------------------------------------------------//
//
// TODO - clean up comments.
//
// Quantum Based Allocation Features
// =================================
//
// - There are no locks or monitors. QBA uses atomic operations to
//   provide memory coherence between cores. This no monitor aspect means
//   that QBA can also be used to manage the allocation of shared memory
//   across processes.
//
// - There are no expensive sbrk calls. QBA uses platform virtual memory
//   reservation to manage memory.
//
// - There are no best-fit searches. QBA finds an allocation fit in
//   constant time.
//
// - There are no free lists. QBA never touches allocated memory.
//   Administrative bits, sizes and side data are all held on the sidelines.
//   In fact, QBA can also be used to manage memory on external devices such
//   as GPUs.
//
// - QBA has minimal external fragmentation. 100% of managed memory is
//   recoverable. This means long running processes will not suffer from
//   uncontrolled fragmentation growth.
//
// - QBA is scalable. Unlike malloc-free, allocation-deallocation rates
//   remain constant no matter the allocation size or allocation volume;
//   from 8 bytes to 4 petabytes.
//
// - QBA allocations require no alignment padding. QBA guarantees that every
//   allocation is size aligned (up to 64M). Ex. 4K allocations are 4K aligned.
//
// - QBA queries for allocation size are returned in constant time.
//
// - QBA can recover the base allocation address from any arbitrary address
//   in constant time, making QBA ideal for garbage collectors.
//
// - QBA provides constant time access to allocation side data. Side data
//   allows an application to color allocations as required.
//
// - Most importantly, QBA is orders of magnitude faster than malloc-free.
//   Especially, when dealing with allocations larger than 32K.
//
// Overview
// ========
//
// This is an implementation of memory allocation system which supplies
// several different allocator methodologies depending on the size of
// allocation. For small to medium size allocations, Quantum Based Allocation
// (QBA) allocators are used. For larger allocations, a slab allocator is used.
//
// All allocators defined in this implementation are a sub-class of the class
// Allocator. Each allocator provides functionality for allocating and
// deallocating memory, as well as providing queries for allocation attributes
// and statistics.
//
// Allocation begins by choosing which allocator and allocation methodology is
// to be used. The choice is based on the power of two "order" of the allocation
// size that will satisfy the allocation request.
//
//     order = trunc(log2(size - 1))
//
// The order of any allocation request will be a value between 0 and 52
// (hardware memory address space is limited to 2^52.)
//
// The order is then used as a index by an instance of AllocatorRoster to select
// an appropriate allocator.
//
//     Order     Size      Allocator
//     -----     ----      ---------
//
//     0-10      0-1K      small quantum allocator (or a specialized Partition)
//     11-18     2K-256K   medium quantum allocator (or a specialized Partition)
//     19-26     512K-64M  large quantum allocator (or a specialized Partition)
//     27-48     64M-256T  SlabAllocator
//     49-64     256T-     NullAllocator
//
// The allocator's "virtual allocate" function is then invoked, which in
// response returns the memory address of the allocation or nullptr if it is not
// capable of satisfying the request. Any further requests are mapped by the
// allocation memory address to the sourcing allocator.
//
// A Director object to coordinate all the allocators within a region of
// reserved memory.
//
// Quantum Based Allocation
// ========================
//
// The QBA API provides a healthy alternative to the standard library
// malloc-free by exploiting contemporary C++, system APIs and multi-core 64-bit
// hardware. The term quantum is used here to describe the minimum amount of
// memory used to satisfy a memory allocation. All of QBA allocation is
// quantum-centric.
//
// QBA is a 64-bit address space allocator, and as such, takes advantage of the
// vast address space available on 64-bit processors. Intel processors allow for
// memory addresses up to 2^52 bytes (4 petabytes.) This is significantly more
// memory than a typical application would use. Even a TensorFlow slab would not
// likely exceed 256 *terabytes* (2^40).
//
// So it's not unreasonable for QBA to reserve large ranges of memory in advance
// of allocation. This type of virtual memory reservation is an inexpensive
// bookkeeping system call that doesn't tie up resources other than restricting
// other system requests from using the requested address range.
//
// Once memory is reserved, the memory is then logically divided into equal size
// partitions. Ex. a 128M reserve could be divided into 128 x 1M partitions.
// Care is given such that the first partition's base address is aligned to the
// size of the partition. The result of this alignment guarantees that all
// partitions are aligned, the partition's contents are aligned and a partition
// index can be quickly determined by the simple shifting of an arbitrary
// address in the partition space by the partition size order, i.e., partitions
// are indexable.
//
// At some point, a partition will be selected by a quantum allocator to satisfy
// an allocation request. Once selected, the partition is designated an order,
// which describes the size of all the quanta accessible in the partition. Ex.
// 1M partition could contain 256 x 4K quantum. Since, all the quanta in the
// partition are the same size, they too are indexable.
//
// Additionally, all the quanta in the aligned partition are also size aligned.
//
// The indexability of both partitions and quanta is how QBA attains constant
// time performance.
//
// Registries
// ==========
//
// One of the minimum requirements of any application's memory allocator is
// thread-safety. Many allocators, such as malloc, rely on monitors to lock out
// competing threads. This is necessary because the complexity of updating
// structures such as linked-lists is more easily dealt with by using critical
// regions.
//
// QBA avoids monitors by using simple atomic operations.
//
// As described in previous section, the main elements, partitions and quanta,
// are indexable. This means that an indexed bit in a bitmap can be used to
// represent the element's state of availability (free or in-use.) Setting the
// bit to 1 indicates that element is in-use and clearing the bit to zero
// indicates the element is available.
//
// Implementing the bitmap using atomic operations provides thread-safety, but
// what about performance? Linear searching a large bitmap or free bits sounds
// expensive.
//
// A QBA Registry object manages an atomic bitmap using a few basic techniques
// to boost performance.
//
// 1. Free bits are searched using 64-bit chunks (words) and not one bit at a
//    time. This is done by doing some simple bit-twiddling involving the
//    count-leading-zeroes/count-trailing-zeroes instructions.
//
// 2. Keep an atomic index of where the lowest free bit resides.
//
// 3. Always fill the lowest bits first. This will fill in with long lived
//    allocations early on and keep the rare scan of multiple words near the
//    higher end of the bitmap.
//
// Combining these techniques means that, much of the time, finding a free bit
// can be done in constant time.
//
// Allocation Performance
// ======================
//
// QBA uses registries to manage both partition and quanta allocation.
//
// Allocating a partition involves flipping the allocation bit in a partition
// registry, initializing the partition admin structure and flipping a partition
// in-use bit in the order registry to indicate deployment (online.)
//
// Once deployed, a partition replaces the quantum allocator in the
// corresponding order slot of the AllocatorRoster. Further allocations go
// directly to the partition with no intervening supervision.
//
// Quantum allocation just involves finding and flipping the bit in the
// partition's quanta registry and the returning the computed address of the
// corresponding quantum.
//
// Deallocation Performance
// ========================
//
// Once the quantum allocator is determined (one to three tiered range checks),
// the partition index can be determined directly from the address (a
// subtraction and a shift).
//
// The quantum index can be determined by masking the address with the partition
// order bit mask. Deallocation is then indicated by clearing the bit in
// partition's quanta registry.
//
// Configurations
// ==============
//
// QBA uses multiple quantum allocators with several different partition sizes.
// This is done to keep the quantum per partition count low and thus keeping the
// size of the quanta registry bitmaps relatively small.
//
// Secure Mode
// ===========
//
// QBA supports a mode which clears memory when deallocated. This technique is
// faster than clearing on allocation and is more secure. Newly committed memory
// is already clear. Recycled blocks are not necessarily used right away and may
// get swapped out before use. Clearing would force a reload from backing store.
//
// QBA Creed
// =========
//
// - Don't use malloc for administrative memory. QBA should be self-reliant.
//
// - Administrative memory should never be pulled from allocation memory.
//   Doing so would interfere with monitoring and testing by end users.
//
// - Most functions are declared as inline to max out speed optimization under
//   -O3. Most of these functions are very small anyway. The result has proved
//   to produce a very tight result.
//
// - Avoid atomic operations for secondary adminstrative duties. For instance,
//   maintaining an in-use bit count in the Register class will affect overall
//   performance. Better to brute force count when required.
//
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
//
// Presence of asserts significantly reduces performance.
//
// The #define NDEBUG should be periodically disabled to ensure
// correctness during development.
//
#if 1
    #define NDEBUG
#endif

//----------------------------------------------------------------------------//
//
// Enable use of hotspot system calls.
//
#define HOTSPOT

#ifdef HOTSPOT
#include "precompiled.hpp"
#include "runtime/os.inline.hpp"
#endif

//----------------------------------------------------------------------------//
//
// Includes
//
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "qba.hpp"

namespace qba {

//----------------------------------------------------------------------------//
//
// Version Information
//

#undef STRINGIFY_EXPAND
#undef STRINGIFY
#define STRINGIFY_EXPAND(X) #X
#define STRINGIFY(X) STRINGIFY_EXPAND(X)

#define QBA_NAME    "QBA"
#define QBA_RELEASE 0
#define QBA_MAJOR   0
#define QBA_MINOR   37
#define QBA_PRODUCT "Experimental"

#define QBA_VERSION (QBA_RELEASE << 16) | (QBA_MAJOR << 8) | QBA_MINOR
#define QBA_VERSION_STRING \
    QBA_NAME " " \
    STRINGIFY(QBA_RELEASE) "." \
    STRINGIFY(QBA_MAJOR) "." \
    STRINGIFY(QBA_MINOR) " " \
    QBA_PRODUCT

//----------------------------------------------------------------------------//
//
// Override system page size, default to 4K.
//
// TODO - Rethink the handling of large and huge page size.
//
#undef PAGE_SIZE
#undef PAGE_SIZE_ORDER
#undef PAGE_MASK

//----------------------------------------------------------------------------//
//
// Bootstrap macroes used for compile-time constant expressions.
//

//
// TWO_TO_ORDER computes two to the order power.
//
// ORDER - an integer value between 0 and MAX_ORDER
//
// Ex. TWO_TO_ORDER(4) would yield 2 to the 4th power which is 16.
//
#define TWO_TO_ORDER(ORDER) (1UL << (ORDER))

//
// MASK constructs a low mask from a power of two.
//
// POWER2 - A 64-bit unsigned value with a single bit set (or all zero.)
//
// Ex. MASK(0x1000) would yield 0x0FFF.
//
#define MASK(POWER2) ((uint64_t)(POWER2) - 1UL)

//----------------------------------------------------------------------------//
//
// UNUSED suppresses unused warnings for functions that are typically only used
// in asserts.
//
// Ex. inline static bool UNUSED isValidAddress(void *address) ...
//
#ifdef _MSC_VER
    #define UNUSED __declspec(unused)
#else
    #define UNUSED __attribute__ ((unused))
#endif

//----------------------------------------------------------------------------//
//
// NOINLINE suppresses inlining of methods, defeating some optimizations
//
// Ex. inline static bool NOINLINE isValidAddress(void *address) ...
//
#undef NOINLINE
#ifdef _MSC_VER
    #define NOINLINE __declspec(noinline)
#else
    #define NOINLINE __attribute__((noinline))
#endif

//----------------------------------------------------------------------------//
//
// Global constants
//

//
// NOT_FOUND is the generic "index not found" result used throughout QBA.
//
const static int NOT_FOUND = ~0;

//
// ZERO is the constant for 64-bit all zeroes
// ALL_ONES is the constant for 64-bit all ones
//
const static uint64_t ZERO = 0UL;
const static uint64_t ALL_ONES = ~ZERO;

//
// Memory sizes as orders for kilo, mega, giga, tera and peta.
//
const static int K_ORDER = 10;
const static int M_ORDER = K_ORDER + K_ORDER;
const static int G_ORDER = K_ORDER + M_ORDER;
const static int T_ORDER = K_ORDER + G_ORDER;
const static int P_ORDER UNUSED = K_ORDER + T_ORDER;

//
// Memory sizes for kilo, mega, giga, tera and peta.
//
const static uint64_t K = 1024UL;
const static uint64_t M = K * K;
const static uint64_t G = K * M;
const static uint64_t T = K * G;
const static uint64_t P UNUSED = K * T;

//
// Word size constants. QBA uses 64-bit words.
//
// BYTES_PER_WORD_ORDER is the order of bytes per (8 byte) word.
//
// BITS_PER_WORD_ORDER is the order of bits per word.
//
// BITS_PER_WORD is the number of bits per word.
//
const static int BYTES_PER_WORD_ORDER = 3;
const static int BITS_PER_WORD_ORDER = 6;
const static int BYTES_PER_WORD = TWO_TO_ORDER(BYTES_PER_WORD_ORDER);
const static int BITS_PER_WORD = TWO_TO_ORDER(BITS_PER_WORD_ORDER);

//
// PAGE_SIZE_ORDER is the order of the page size. Currently, hard-wired to 4K
// pages.
//
// TODO - Handle huge pages.
//
const static int PAGE_SIZE_ORDER = 12;

//
// PAGE_SIZE is the default memory page size. Currently, hard-wired to 4K pages.
//
// PAGE_MASK is for masking to page size.
//
// TODO - Handle huge pages.
//
const static uint64_t PAGE_SIZE = TWO_TO_ORDER(PAGE_SIZE_ORDER);
const static uint64_t PAGE_MASK UNUSED = MASK(PAGE_SIZE);

//
// BITS_MASK is for masking to number of bits per word.
//
const static uint64_t BITS_MASK = MASK(BITS_PER_WORD);

//
// Upper allocation limits.
//
// MAX_ADDRESS_ORDER is the order of the maximum memory address.
//
// MAX_ALLOCATION_ORDER is the order of the maximum allocation QBA allows
// (256T.)
//
// MAX_ORDER is the maximum value for order (BITS_PER_WORD.)
//
// MAX_PARTITION_QUANTUM is the maximum quantum per partition.
//
const static int MAX_ADDRESS_ORDER = 52;
const static int MAX_ALLOCATION_ORDER = MAX_ADDRESS_ORDER - 4; // 256T
const static int MAX_ORDER = BITS_PER_WORD;

//
// MAX_ADDRESS_SIZE is the maximum memory address and the maximum
// size that could be allocated (but not by QBA.)
//
// MAX_ALLOCATION_SIZE is the maximum allocation size supported by QBA.
//
// MAX_ADDRESS_MASK is for masking addresses to check validity.
//
// VALID_ADDRESS_MASK is used for system call address validation.
//
const static uint64_t MAX_ADDRESS_SIZE = TWO_TO_ORDER(MAX_ADDRESS_ORDER);
const static uint64_t MAX_ALLOCATION_SIZE UNUSED =
    TWO_TO_ORDER(MAX_ALLOCATION_ORDER);
const static uint64_t MAX_ADDRESS_MASK = MASK(MAX_ADDRESS_SIZE);
const static uint64_t VALID_ADDRESS_MASK = MAX_ADDRESS_MASK & ~MASK(8);


//----------------------------------------------------------------------------//
//
// Allocator configuration.
//

//
// MAX_PARTITION_QUANTUM is the maximum quantum per partition.
//
const static int MAX_PARTITION_QUANTUM = 16 * K;

//
// MAX_QUANTUM_ALLOCATORS is the number of quantum allocators.
//
const static int MAX_QUANTUM_ALLOCATORS = 3;

//
// MAX_REGISTRY_BIT_COUNT is maximum number of elements that can be managed by a
// registry. This is an arbitrary fixed value to ease the dynamic allocation of
// internal structures.
//
// MAX_REGISTRY_WORD_COUNT is maximum number of words required to handled
// MAX_REGISTRY_BIT_COUNT elements.
//
const static int MAX_REGISTRY_BIT_COUNT = MAX_PARTITION_QUANTUM;
const static int MAX_REGISTRY_WORD_COUNT =
    MAX_REGISTRY_BIT_COUNT >> BITS_PER_WORD_ORDER;

//
// MAX_QUANTUM_ALLOCATOR_ORDERS is the maximum number of orders managed by a
// single quantum allocator. It was chosen to keep the range supported by
// MAX_REGISTRY_BIT_COUNT reasonable.
//
const static int MAX_QUANTUM_ALLOCATOR_ORDERS = 8;

//
// SMALLEST_SIZE_ORDER is the order of smallest quantum, 8 bytes (also
// the the minimum allocation size.)
//
const static int SMALLEST_SIZE_ORDER = 3;

//
// LARGEST_SIZE_ORDER is the order of largest quantum.
//
const static int LARGEST_SIZE_ORDER = SMALLEST_SIZE_ORDER +
                                      MAX_QUANTUM_ALLOCATORS *
                                      MAX_QUANTUM_ALLOCATOR_ORDERS - 1;

//
// MAX_FIT_DEGREE is the maximum number of bits used by the
// qba_allocate_fit/qba_deallocate_fit API to determine fragmentation level.
//
// 1 is the normal allocate with 25% average fragmentation, 2 is 12.5%, 3 is
// 6.25%, 4 is 3.125%.
//
const static int MAX_FIT_DEGREE UNUSED = 4;

//
// MAX_LINK_NAME is teh maximum number of characters in a shared link name.
//
const static int MAX_LINK_NAME = 256;

//----------------------------------------------------------------------------//
//
// Address validation functions, primarily used to valid addresses passed to
// System calls.
//
// address - Any memory address cast as a 64-bit unsigned value.
//
inline static bool isValidAddress(uint64_t address) {
    return address && !(address & ~VALID_ADDRESS_MASK);
}

//
// address - Any memory address.
//
inline static bool UNUSED isValidAddress(void *address) {
    return isValidAddress(reinterpret_cast<uint64_t>(address));
}

//----------------------------------------------------------------------------//
//
// Bit twiddling utility functions.
//

//
// clz is for counting leading zero bits. Handles the zero case which is
// undefined on some platforms.
//
// value - Any 64-bit unsigned value.
//
// Ex. clz(0xFFFF) would yield 48.
//
inline static int clz(uint64_t value) {
    return value ? __builtin_clzll(value) : BITS_PER_WORD;
}

//
// clz is for counting trailing zero bits. Handles the zero case which is
// undefined on some platforms.
//
// value - Any 64-bit unsigned value.
//
// Ex. ctz(0xFF00) would yield 8.
//
inline static int ctz(uint64_t value) {
    return value ? __builtin_ctzll(value) : BITS_PER_WORD;
}

//
// popcount counts the number of one bits in a word. Primarily for performing
// the census of a Registry.
//
// value - Any 64-bit unsigned value.
//
// Ex. popcount(0xFFF0) would yield 12.
//
inline static int popcount(uint64_t value) {
    return value ? __builtin_popcountll(value) : 0;
}

//
// twoToOrder computes two to the order power.
//
// order - An int value between 0 and BITS_PER_WORD.
//
// Ex. twoToOrder(4) would yield 2 to the 4th power which is 16.
//
inline static uint64_t twoToOrder(int order) {
    // NO ASSERT - affects optimization.
    return TWO_TO_ORDER(order);
}

//
// loMask produces a mask of n bits at the low end of a word.
//
// n - An int value between 0 and BITS_PER_WORD.
//
// Ex. loMask(5) yields 0x001F.
//
inline static uint64_t loMask(int n) {
    // NO ASSERT - affects optimization.
    return MASK(twoToOrder(n));
}

//
// hiMask produces a mask of n bits at the high end of a word.
//
// n - An int value between 0 and BITS_PER_WORD.
//
// Ex. loMask(5) yields 0xF100000000000000.
//
inline static uint64_t hiMask(int n) {
    // NO ASSERT - affects optimization.
    return ~loMask(BITS_PER_WORD - n);
}

//
// isPowerOf2 tests if the value is a power of two. Treats zero as a power of
// two.
//
// value - Any 64-bit unsigned value.
//
// Ex. isPowerOf2(0x100) yields true.
//
inline static bool UNUSED isPowerOf2(uint64_t value) {
    return (value & MASK(value)) == ZERO;
}

//
// roundUp rounds the value up to the specified power of two. Primarily used
// to size up align to the next quantum.
//
// value - Any 64-bit unsigned value.
// powerOf2 - A 64-bit unsigned with a single bit set.
//
// Ex. roundUp(0x50034, 0x1000) yields 0x60000.
//
inline static uint64_t roundUp(uint64_t value, uint64_t powerOf2) {
    uint64_t mask = MASK(powerOf2);

    return (value + mask) & ~mask;
}

//
// roundUpPowerOf2 rounds the value up to the next power of two. Primarily used
// to size up align to the next quantum.
//
// value - Any 64-bit unsigned value.
//
// Ex. roundUp(0x50000) yields 0x80000.
//
inline static uint64_t roundUpPowerOf2(uint64_t value) {
    return value ? 1UL << (BITS_PER_WORD - clz(value - 1)) : ZERO;
}

//
// sizeToOrder translates an allocation size to a power of two order. I.E., the
// power of two bytes that is required to satisfy the allocation. Values less
// than 8 are special cased to always yield 3.
//
// size - Size of allocation.
//
// Ex. sizeToOrder(17) yields 5. 2^5 == 32 bytes the smallest quantum that can
// satisfy an allocation of 17 bytes.
//
inline static int sizeToOrder(uint64_t size) {
    return 8 < size ? BITS_PER_WORD - clz(size - 1) : 3;
}

//
// orderToSize translates an allocation order to it's corresponding size. This
// is simply 2^order.
//
// order - An int value between 0 and BITS_PER_WORD.
//
// Ex. orderToSize(5) yields 2^5 or 32.
//
inline static uint64_t orderToSize(int order) {
    // NO ASSERT - affects optimization.
    return TWO_TO_ORDER(order);
}

//
// orderMul is primarily to make the multiplication by an order distinct from
// the underlying shift operation.
//
// value - Any 64-bit unsigned value.
// order - An int value between 0 and BITS_PER_WORD.
//
// Ex. order = sizeToOrder(size)
//     offset = orderMul(index, order)
//     offset == index * size
//
inline static uint64_t orderMul(uint64_t value, int order) {
    // NO ASSERT - affects optimization.
    return value << order;
}

//
// orderDiv is primarily to make the division by an order distinct from the
// underlying shift operation.
//
// value - Any 64-bit unsigned value.
// order - An int value between 0 and BITS_PER_WORD.
//
// Ex. partitionIndex = orderDiv(address, order)
//
inline static int orderDiv(uint64_t value, int order) {
    // NO ASSERT - affects optimization.
    return (int)(value >> order);
}

//
// lowestZeroBit returns an isolated one bit where the lowest zero (free) bit
// resides. Used to find a free bit in a bit set.
//
// value - Any 64-bit unsigned value.
//
// Ex. lowestZeroBit(0x0F7F) yields 0x0080.
//
//     0b0000_1111_0111_1111 value
//     0b1111_0000_1000_0000 inverse
//     0b0000_1111_1000_0000 -inverse
//     0b0000_0000_1000_0000 inverse & -inverse
//
inline static uint64_t lowestZeroBit(uint64_t value) {
    // NO ASSERT - affects optimization.
    uint64_t inverse = ~value;

    return inverse & -inverse;
}

//
// lowestZeroBitsPosition returns the bit (LSB) index where a sequence of n
// lowest zero (free) bits reside. Used to find consecutive free bits in a
// bit set. Returns NOT_FOUND if no such sequence exists.
//
// lowestZeroBitsPosition is guaranteed to return a "maybe" result if the upper
// bit of the value is zero. This is to allow for sequences that wrap into the
// next word.
//
// value - Any 64-bit unsigned value.
// n - An int value between 1 and BITS_PER_WORD.
//
// Ex. lowestZeroBitsPosition(0x0F7F, 2) yields 11.
//
//      0b0000_1111_0111_1111 value
//      0b0000_0000_1000_0000 lowestBit
//      0b0000_0010_0000_0000 lowestBit << n
//      0b0000_0001_1000_0000 rangeMask
//      0b0000_0001_0000_0000 value & rangeMask
//                            not equal zero
//      0b0000_1111_1111_1111 value |= value - lowestBit
//      0b0001_0000_0000_0000 lowestBit
//      0b0100_0000_0000_0000 lowestBit << n
//      0b0011_0000_1000_0000 rangeMask
//      0b0000_0000_0000_0000 value & rangeMask
//                            equal zero
//      11                    log2(lowestBit) - 1
//
inline static int lowestZeroBitsPosition(uint64_t value, int n) {
    // NO ASSERT - affects optimization.
    while (value != ALL_ONES) {
        uint64_t lowestBit = lowestZeroBit(value);
        uint64_t rangeMask = (lowestBit << n) - lowestBit;

        if (!(value & rangeMask)) {
            return BITS_PER_WORD - 1UL - clz(lowestBit) ;
        }

        value |= value - lowestBit;
    }

    return NOT_FOUND;
}

//----------------------------------------------------------------------------//
//
// The NoAllocate class is an abstract superclass used to prevent the
// allocation of the annotated subclass.
//
class NoAllocate {
public:
    static void* operator new(size_t size, void *ptr) { return ptr; }
    static void* operator new(size_t size) = delete;
    static void* operator new[](size_t size) = delete;
    static void operator delete(void *ptr) = delete;
    static void operator delete[](void *ptr) = delete;
};

//----------------------------------------------------------------------------//
//
// The Address class exists to facilitate byte arithmetic on void * addresses.
//
class Address : public NoAllocate {
private:
    //
    // void *address converted to unsigned 64-bit integer.
    //
    uint64_t _address;

public:
    //
    // Empty constructor.
    //
    inline Address() : _address(ZERO) {
    }

    //
    // Constructor used when the address is a fixed predefined memory address.
    // Used to locate reserved memory at specific addresses.
    //
    inline Address(uint64_t address) :
        _address(address)
    {
    }

    //
    // Constructor used when the address is a pointer (void * derivative).
    // An optional byte offset can be supplied.
    //
    inline Address(void *address, uint64_t offset = ZERO) :
        _address(reinterpret_cast<uint64_t>(address) + offset)
    {
    }

    //
    // Constructor used when the address is a const pointer (const void *
    // derivative). An optional byte offset can be supplied.
    //
    inline Address(const void *address, uint64_t offset = ZERO) :
        _address(reinterpret_cast<uint64_t>(address) + offset)
    {
    }

    //
    // Constructor used when the address is a char * An optional byte offset
    // can be supplied.
    //
    inline Address(char *address, uint64_t offset = ZERO) :
        _address(reinterpret_cast<uint64_t>(address) + offset)
    {
    }

    //
    // Constructor used when the address is a const char * An optional byte
    // offset can be supplied.
    //
    inline Address(const char *address, uint64_t offset = ZERO) :
        _address(reinterpret_cast<uint64_t>(address) + offset)
    {
    }

    //
    // Implicit cast for unsigned 64-bit integer.
    //
    inline operator uint64_t() const {
        return _address;
    }

    //
    // Implicit cast for void *.
    //
    inline operator void *() const {
        return reinterpret_cast<void *>(_address);
    }

    //
    // Implicit cast for char *.
    //
    inline operator char *() const {
        return reinterpret_cast<char *>(_address);
    }

    //
    // Implicit cast for bool.
    //
    inline operator bool() const {
        return _address != ZERO;
    }

    //
    // isNull is a predicate to test for a null pointer.
    //
    inline bool isNull() const {
        return _address == ZERO;
    }

    //
    // isNull is a predicate to test for a not null pointer.
    //
    inline bool isNotNull() const {
        return _address != ZERO;
    }

    //
    // align aligns the address to the specified power of two alignment.
    //
    // alignment - A 64-bit unsigned value with a single bit set.
    //
    // Ex. void *reserved = ...
    //     void *megabyteAlignedReserved = Address(reserved).align(M)
    //
    inline Address align(uint64_t alignment) const {
        assert(isPowerOf2(alignment) &&
               "alignment should be power of two");

        return Address(roundUp(_address, alignment));
    }

    //
    // getIndex returns the index of the address relative to a base address.
    // The function would be used to convert an allocation address to a
    // partition index or a quantum index.
    //
    // base - Base address of the space being mapped.
    // order - int value representing the size order of the quantum.
    //
    // Ex. quantumIndex = Address(allocation).getIndex(partitionBase,
    //                                                 quantumSizeOrder)
    //
    inline int getIndex(void *base, int order) const {
        assert(base != nullptr &&
               "base address is null");
        assert(0 < order && order <= MAX_ORDER &&
               "order is out of range");

        return orderDiv(_address - (uint64_t)Address(base), order);
    }

    //
    // Operator to add a byte offset to an address.
    //
    // offset - A 64-bit unsigned value added to the address.
    //
    inline Address operator +(uint64_t offset) const {
        assert(offset < MAX_ADDRESS_SIZE &&
               "offset is too large");

        return Address(_address + offset);
    }

    //
    // Operator to subtract a byte offset from an address.
    //
    // offset - A 64-bit unsigned value subtracted the address.
    //
    inline Address operator -(uint64_t offset) const {
        assert(offset < MAX_ADDRESS_SIZE &&
               "offset is too large");

        return Address(_address - offset);
    }

    //
    // Operator to mask bits in an address.
    //
    // mask - Mask of one bits to be kept in the address (typically round down.)
    //
    inline Address operator &(uint64_t mask) const {
        return Address(_address & mask);
    }
};

//----------------------------------------------------------------------------//
//
// The System class encapsulates all system calls used by QBA.
//
class System : public NoAllocate {
public:

    //
    // Reserve an address range for future use by an allocator. Returns the
    // reserve address or nullptr if the request can not be satisfied.
    //
    // No TLBs or backing store are reserved by this call.
    //
    // size - Size of memory (in bytes) to reserve. Should be multiple of
    //        PAGE_SIZE.
    // location - Fixed memory location or zero for floating.
    // alignment - Alignment (doesn't always work on HOTSPOT).
    // fd - File descriptor for shared link.
    //
    inline static void *reserve(
        uint64_t size,
        uint64_t location = ZERO,
        uint64_t alignment = ZERO,
        int fd = -1
     ) {
        assert((size & PAGE_MASK) == 0 &&
               "size must be aligned to page size");
#ifdef _MSC_VER
        return os::reserve_memory(size, (char *)location, alignment, fd);
#else
        int protection = 0;
        int flags = 0;

        if (location != ZERO) {
            flags |= MAP_FIXED;
        }

        if (fd != -1) {
            protection = PROT_READ | PROT_WRITE;
            flags |= MAP_SHARED;
        } else {
            protection = PROT_NONE;
            flags |= MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
        }

        void *address = mmap(
            (char *)location, size,
            protection,
            flags,
            fd, 0
        );

        return address == MAP_FAILED ? nullptr : address;
#endif
    }

    //
    // Over reserve an address range for future use by an allocator. The excess
    // is necessary to guarantee the required alignment. Any excess is returned
    // to the system after the aligned range is excised from the middle of the
    // over reserve. Returns the reserve address or nullptr if the request can
    // not be satisfied.
    //
    // No TLBs or backing store are reserved by this call.
    //
    // size - Size of memory (in bytes) to reserve. Should be multiple of
    //        PAGE_SIZE.
    // alignment - A 64-bit unsigned value with a single bit set (power of two.)
    //
    inline static void *reserveAligned(uint64_t size, uint64_t alignment) {
        assert((size & PAGE_MASK) == 0 &&
               "size must be aligned to page size");
        assert(alignment != ZERO && (alignment & PAGE_MASK) == 0 &&
               "alignment must be aligned to page size");

        //
        // Over allocate by the alignment size. This will allow an aligned
        // portion to be excised from the middle of the reserve.
        //
        uint64_t reserveSize = size + alignment - PAGE_SIZE;
        void *address = reserve(reserveSize);

        if (address == MAP_FAILED) {
            return nullptr;
        }

        //
        // Compute the base of aligned reserve.
        //
        Address allocation(address);
        Address base = Address(address).align(alignment);

        //
        // Compute the size of the excesses before and after the aligned
        // reserve.
        //
        uint64_t prefixSize = base - allocation;
        uint64_t postfixSize = reserveSize - size - prefixSize;

        //
        // Return the prefix excess back to the system.
        //
        if (prefixSize) {
            release(allocation, prefixSize);
        }

        //
        // Return the postfix excess back to the system.
        //
       if (postfixSize) {
            release(base + size, postfixSize);
        }

        return base;
    }

    //
    // Map an address range for use by an allocator. Returns the
    // location address or nullptr if the request can not be satisfied.
    //
    // size - Size of memory (in bytes) to reserve. Should be multiple of
    //        PAGE_SIZE.
    // location - Fixed memory location or zero for floating.
    // fd - File descriptor for shared link.
    //
    inline static void *mapShared(uint64_t size, uint64_t location, int fd) {
#ifdef _MSC_VER
        return nullptr;
#else
        void *address = mmap(reinterpret_cast<void *>(location), size,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_FIXED,
                             fd, 0);

        return address == MAP_FAILED ? nullptr : address;
#endif
    }

    //
    // Release reserved memory back to the system.
    //
    // address - Base address previously reserved.
    // size - Size of memory (in bytes) previously reserved.
    //
    inline static void release(void *address, uint64_t size) {
        assert(isValidAddress(address) &&
               "address is invalid");
        assert((size & PAGE_MASK) == 0 &&
               "size must be aligned to page size");
#ifdef _MSC_VER
        os::release_memory((char *)address, size);
#else
        munmap(address, size);
#endif
    }

    //
    // Commit reserved memory. Allocate TLBs and backing store.
    //
    // address - Base address previously reserved.
    // size - Size of memory (in bytes) previously reserved. Should be a
    //        multiple of PAGE_SIZE.
    //
    inline static void commit(void *address, uint64_t size) {
        assert(isValidAddress(address) &&
               "address is invalid");
        assert((size & PAGE_MASK) == 0 &&
               "size must be aligned to page size");
#ifdef _MSC_VER
        os::commit_memory((char *)address, size, ZERO, true);
#else
        mmap(address, size,
             PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
             -1, 0);
#endif
    }

    //
    // Return memory back to "just" reserved state. Releasing TLBs and backing
    // store.
    //
    // address - Base address previously committed.
    // size - Size of memory (in bytes) previously committed. Should be a
    //        multiple of PAGE_SIZE.
    //
    inline static void uncommit(void *address, uint64_t size) {
        assert(isValidAddress(address) &&
               "address is invalid");
        assert((size & PAGE_MASK) == 0 &&
               "size must be aligned to page size");
#ifdef _MSC_VER
        os::uncommit_memory((char *)address, size);
#else
        mmap(address, size,
             PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
             -1, 0);
#endif
    }

    //
    // Optimal clear memory.
    //
    // address - Base address of memory to clear.
    // size - Size of memory (in bytes) to clear.
    // sharing - true if sharing.
    //
    static void clear(void *address, uint64_t size, bool sharing) {
        assert(isValidAddress(address) &&
               "address is invalid");
        assert((size & sizeof (uint64_t) - 1) == 0 &&
               "size must align on 8 bytes");

        //
        // Special case small values.
        //
        if (size == 8) {
            *static_cast<uint64_t *>(address) = ZERO;
        } else if (size == 16) {
            __builtin_memset(address, 0, 16);
        } else if (size == 32) {
            __builtin_memset(address, 0, 32);
        } else if (size == 64) {
            __builtin_memset(address, 0, 64);
        } else if (size <= 32 * K){
            //
            // Do system optimized clearing.
            //
            __builtin_memset(address, 0, size);
        } else if (!sharing) {
            //
            // For larger allocations, recommit memory (reset to zero page
            // and COW.)
            //
            mmap(address, size,
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS,
                -1, 0);
        } else {
            //
            // Do system optimized clearing.
            //
            __builtin_memset(address, 0, size);
        }
    }

    //
    // Optimal copy memory. Only used for reallocate.
    //
    // src - Address of memory to copy from.
    // dst - Address of memory to copy to.
    // size - Size of memory (in bytes) to copy.
    //
    inline static void copy(void *src, void *dst, uint64_t size) {
        assert(src != nullptr &&
               "source must not be null");
        assert(dst != nullptr &&
               "destination must not be null");
        assert((size & sizeof (uint64_t) - 1) == 0 &&
               "size must align on 8 bytes");

        if (size < PAGE_SIZE) {
            //
            // If small allocation, loop copy.
            //
            for (uint64_t *srcData = static_cast<uint64_t *>(src),
                          *dstData = static_cast<uint64_t *>(dst);
                 0 < size; size -= sizeof (uint64_t)) {
                 *dstData++ = *srcData++;
            }
        } else {
            //
            // For larger allocations, do system optimized copy.
            memcpy(dst, src, size);
        }
    }
};

//----------------------------------------------------------------------------//
//
// The Registry class implements an optimized atomic bitmap.
//
class Registry : public NoAllocate {
protected:
    //
    // Maximum bit index.
    //
    const int _maximumIndex;

    //
    // Maximum word index. _maximumWordIndex * BITS_PER_WORD may be greater than
    // _maximumIndex due to rounding up to full words.
    //
    const int _maximumWordIndex;

    //
    // Index of lowest bitmap word containing free bits.
    //
    std::atomic_int _lowestIndex;

    //
    // Bits used for bitmap.
    //
    std::atomic_uint64_t _bits[MAX_REGISTRY_WORD_COUNT];

    //
    // conditionallySetMaskRange is used to set multiword contiguous bits
    // conditionally. It does so by attempting to set bits one word at a
    // time. If it fails to do so, then it backs out the sets it did prior.
    // Returns true if fully successful.
    //
    // firstWordIndex - int index of first word to set to ones.
    // count - Number of "middle" words to set to ALL_ONES.
    // firstMask - Upper portion of first word to set to ones.
    // lastMask - Lower portion of last word to set to ones.
    //
    bool conditionallySetMaskRange(
        int firstWordIndex,
        int count,
        uint64_t firstMask,
        uint64_t lastMask
    )
    {
        assert(isValidWordIndex(firstWordIndex) &&
               "firstWordIndex out of range");
        assert(isValidWordIndex(count) &&
               "count is out of range");

        //
        // Attempt to set the first word mask.
        //
        if (!conditionallySetMask(firstWordIndex, firstMask)) {
            //
            // Cannot set the first word (may another thread beat it.)
            //
            return false;
        }

        //
        // Attempt to set the the middle words.
        //
        for (int i = 0; i < count; i++) {
            //
            // Attempt to set the next word.
            //
            if (!conditionallySetMask(firstWordIndex + i + 1, ALL_ONES)) {
                //
                // Unset everything set thus far.
                //
                clearMaskRange(firstWordIndex, i, firstMask, ZERO);

                return false;
            }
        }

        //
        // Attempt to set the last word mask.
        //
        if (!conditionallySetMask(firstWordIndex + count + 1, lastMask)) {
            //
            // Unset everything set thus far.
            //
            clearMaskRange(firstWordIndex, count, firstMask, ZERO);

            return false;
        }

        return true;
    }

    //
    // clearMaskRange unconditionally clears multi-word contiguous bits set by
    // conditionallySetMaskRange.
    //
    // firstWordIndex - int index of first word to clear to zeroes.
    // count - Number of "middle" words to set to ZERO.
    // firstMask - Upper portion of first word to clear to zeroes.
    // lastMask - Lower portion of last word to clear to zeroes.
    //
    void clearMaskRange(
        int firstWordIndex,
        int count,
        uint64_t firstMask,
        uint64_t lastMask
    )
    {
        assert(isValidWordIndex(firstWordIndex) &&
               "firstWordIndex out of range");
        assert(isValidWordIndex(count) &&
               "count is out of range");
        //
        // Clear first word mask.
        //
        clearMask(firstWordIndex, firstMask);

        //
        // Clear middle words.
        //
        for (int i = 0; i < count; i++) {
            clearMask(firstWordIndex + i + 1, ALL_ONES);
        }

        //
        // Clear last word mask.
        //
        clearMask(firstWordIndex + count + 1, lastMask);
    }

    //
    // incrementLowestFreeWordIndex attempts to increment the
    // getLowestIndex. If it fails to do so then it returns the value
    // set by other thread, presumably lower.
    //
    // wordIndex - The word index that is assumed (loaded elsewhere.)
    //
    inline int incrementLowestFreeWordIndex(int wordIndex) {
        assert(isValidWordIndex(wordIndex) &&
               "wordIndex out of range");

        //
        // Bump the word index.
        //
        int next = wordIndex + 1;

        if (swapLowestIndex(wordIndex, next)) {
            //
            // Succeeded so return next.
            //
            return next;
        } else {
            //
            // Failed so use the value set by another thread.
            //
            return wordIndex;
        }
    }

public:
    //
    // Constructor used for initializing global data.
    //
    Registry() :
        _maximumIndex(0),
        _maximumWordIndex(0),
        _lowestIndex(0),
        _bits()
    {
    }

    //
    // Constructor used when activating a specific registry.
    //
    // maximumIndex - Maximum number of bits managed by this registry.
    //
    Registry(int maximumIndex) :
        _maximumIndex(maximumIndex),
        _maximumWordIndex(wordsNeeded(maximumIndex)),
        _lowestIndex(0),
        _bits()
    {
        assert(0 <= _maximumIndex && _maximumIndex <= MAX_REGISTRY_BIT_COUNT &&
               "maximumCount out of range");
    }

    //
    // maximumIndex returns the maximum bit index.
    //
    inline int maximumIndex() const {
        return _maximumIndex;
    }

    //
    // maximumWordIndex returns the maximum word index.
    //
    inline int maximumWordIndex() const {
        return _maximumWordIndex;
    }

    //
    // getBits returns the atomic bits for the specified word index.
    //
    // wordIndex - int value between 0 and _maximumWordCount.
    //
    inline uint64_t getBits(int wordIndex) const {
        return _bits[wordIndex].load();
    }

    //
    // swapBits does an compare and exchange of atomic bits for the specified
    // word index.
    //
    inline bool swapBits(int wordIndex, uint64_t& existing, uint64_t value) {
        return _bits[wordIndex].compare_exchange_weak(existing, value);
    }

    //
    // andBits does a fetch "and" of the atomic bits for the specified word
    // index.
    //
    inline uint64_t andBits(int wordIndex, uint64_t value) {
        return _bits[wordIndex].fetch_and(value);
    }

    //
    // orBits does a fetch "or" of the atomic bits for the specified word
    // index.
    //
    inline uint64_t orBits(int wordIndex, uint64_t value) {
        return _bits[wordIndex].fetch_or(value);
    }

    //
    // getLowestIndex returns the index of the lowest word that has free
    // (zero) bits.
    //
    inline int getLowestIndex() const {
        return _lowestIndex.load();
    }

    //
    // swapLowestIndex does an compare and exchange of the lowest word index.
    //
    inline bool swapLowestIndex(int& existing, int value) {
        return _lowestIndex.compare_exchange_weak(existing, value);
    }

    //
    // getWordIndex returns the word index of word containing the 'index'ed
    // bit.
    //
    // index - int value between 0 and _maximumIndex.
    //
    inline static int getWordIndex(int index) {
        assert(0 <= index &&
               "index should be positive");

        return index >> BITS_PER_WORD_ORDER;
    }

    //
    // getBitIndex returns the bit index (from lowest bit) in word containing
    // the 'index'ed bit.
    //
    // index - int value between 0 and _maximumIndex.
    //
    inline static int getBitIndex(int index) {
        assert(0 <= index &&
               "index should be positive");

        return index & static_cast<int>(BITS_MASK);
    }

    //
    // getIndex combines a wordIndex and bitIndex into a single bit index
    // reference.
    //
    // wordIndex - int value between 0 and _maximumWordIndex.
    // bitIndex - int value between 0 and BITS_PER_WORD.
    //
    inline static int getIndex(int wordIndex, int bitIndex) {
        assert(0 <= wordIndex &&
               "wordIndex should be positive");
        assert(0 <= bitIndex &&
               "wordIndex should be positive");

        return (wordIndex << BITS_PER_WORD_ORDER) + bitIndex;
    }

    //
    // wordsNeeded returns the number of words needed to represent 'count' bits.
    //
    // count - Number of bits required in the registry.
    //
    inline static int wordsNeeded(int count) {
        assert(0 <= count && count <= MAX_PARTITION_QUANTUM &&
               "count out of range");

        return orderDiv(count + BITS_PER_WORD - 1UL, BITS_PER_WORD_ORDER);
    }

    //
    // bytesNeeded returns the number of bytes needed to represent 'count' bits.
    //
    // count - Number of bits required in the registry.
    //
    inline static int bytesNeeded(int count) {
        assert(0 <= count && count <= MAX_PARTITION_QUANTUM &&
               "count out of range");

        return orderDiv(count + BITS_PER_WORD - 1UL, BYTES_PER_WORD_ORDER);
    }

    //
    // isValidIndex validates an index.
    //
    // index - int value that should be be between 0 and _maximumIndex.
    //
    inline bool isValidIndex(int index) const {
        return 0 <= index && index < _maximumIndex;
    }

    //
    // isValidCount validates a count.
    //
    // count - int value that should be between 0 and _maximumIndex.
    //
    inline bool isValidCount(int count) const {
        return 0 <= count && count <= _maximumIndex;
    }

    //
    // isValidWordIndex validates a word index.
    //
    // wordIndex - int value that should be between 0 and _maximumWordIndex.
    //
    inline bool isValidWordIndex(int wordIndex) const {
        return 0 <= wordIndex && wordIndex <= _maximumWordIndex;
    }

    //
    // setMask unconditionally sets the mask bits in the word indexed by
    // wordIndex. Returns true if bits were all previously zeroes.
    //
    // wordIndex - int value that is between 0 and _maximumWordIndex.
    // mask - Bits that should be set to one.
    //
    inline bool setMask(int wordIndex, uint64_t mask) {
        assert(isValidWordIndex(wordIndex) &&
               "wordIndex out of range");

        return !mask || (orBits(wordIndex, mask) & mask) == ZERO;
    }

    //
    // clearMask unconditionally clears the mask bits in the word indexed by
    // wordIndex. Returns true if bits were previously not zeroes.
    //
    // wordIndex - int value that is between 0 and _maximumWordIndex.
    // mask - Bits that should be set to zero.
    //
    inline bool clearMask(int wordIndex, uint64_t mask) {
        assert(isValidWordIndex(wordIndex) &&
               "wordIndex out of range");

        return !mask || (andBits(wordIndex, ~mask) & mask) != ZERO;
    }

    //
    // clearAllBits unconditionally clears all bits. Should only be used when
    // registry is offline.
    //
    inline void clearAllBits() {
        memset(_bits, 0, bytesNeeded(_maximumIndex));
    }

    //
    // isSet weakly tests if bit is set. State may change after reading unless
    // reader "owns" (has allocated) bit.
    //
    // index - int index between 0 and _maximumIndex of bit to test.
    //
    inline bool isSet(int index) const {
         assert(isValidIndex(index) &&
               "index out of range");
        int wordIndex = getWordIndex(index);
        int bitIndex = getBitIndex(index);
        uint64_t bit = twoToOrder(bitIndex);
        uint64_t value = getBits(wordIndex);

        return (value & bit) != 0uL;
    }

    //
    // set conditionally sets a bit. Returns true if successful.
    //
    // index - int index between 0 and _maximumIndex of bit to set.
    //
    inline bool set(int index) {
        assert(isValidIndex(index) &&
              "index out of range");
        int wordIndex = getWordIndex(index);
        int bitIndex = getBitIndex(index);

        return setMask(wordIndex, twoToOrder(bitIndex));
    }

    //
    // clear conditionally clears a bit. Returns true if successful.
    //
    // index - int index between 0 and _maximumIndex of bit to clear.
    //
    inline bool clear(int index) {
        assert(isValidIndex(index) &&
              "index out of range");
        int wordIndex = getWordIndex(index);
        int bitIndex = getBitIndex(index);

        return clearMask(wordIndex, twoToOrder(bitIndex));
    }

    //
    // conditionallySetMask conditionally sets mask of one bits. Uses previous
    // know value. Returns true if bits were set.
    //
    // wordIndex - int word index between 0 and _maximumWordIndex of word to set.
    // value - Reference to previously know value of word.
    // mask - unsigned 64-bit mask of bits to set.
    //
    inline bool conditionallySetMask(
        int wordIndex,
        uint64_t& value,
        uint64_t mask
    )
    {
       assert(isValidWordIndex(wordIndex) &&
              "wordIndex out of range");
       if (mask) {
           return swapBits(wordIndex, value, value | mask);
       }

       return true;
    }

    //
    // conditionallySetMask conditionally sets mask of one bits. Returns true
    // if bits were set.
    //
    // wordIndex - int word index between 0 and _maximumWordIndex of word to set.
    // mask - unsigned 64-bit mask of bits to set.
    //
    inline bool conditionallySetMask(int wordIndex, uint64_t mask) {
       assert(isValidWordIndex(wordIndex) &&
              "wordIndex out of range");
       uint64_t value = getBits(wordIndex);

       return conditionallySetMask(wordIndex, value, mask);
    }

    //
    // findFree finds the lowest free bit in the registry. Returns the index
    // or NOT_FOUND if no free bits.
    //
    inline int findFree() {
        //
        // TODO - Use summary bit maps (on cache line size) to skip over
        //        expanses of in use bits.
        //

        //
        // Start at lowest word known to have free bits.
        //
        int wordIndex = getLowestIndex();

        //
        // Loop until a boundary condition is met.
        //
        while (true) {
            //
            // Exit if not more words.
            //
            if (wordIndex == _maximumWordIndex) {
                return NOT_FOUND;
            }

            //
            // Sample the current word.
            //
            uint64_t value = getBits(wordIndex);

            //
            // If no free bits.
            //
            if (value == ALL_ONES) {
                //
                // Try bumping up word index, but may get index pointed to by
                // another thread.
                //
                wordIndex = incrementLowestFreeWordIndex(wordIndex);

                //
                // Try again.
                //
                continue;
            }

            //
            // Get bit index of lowest zero bit.
            //
            int bitIndex = ctz(~value);

            //
            // Combine wordIndex and bitIndex to create a registry bit index.
            //
            int index = getIndex(wordIndex, bitIndex);

            //
            // May exceed the count of the registry (free bits in unused portion
            // of word.
            //
            if (_maximumIndex <= index) {
                return NOT_FOUND;
            }

            //
            // Attempt to update word with bit set.
            //
            if (swapBits(wordIndex, value, value | (1UL << bitIndex))) {
                //
                // Successful set bit.
                //
                return index;
            }

            //
            // Try again.
            //
        }
    }

    //
    // free clears the bit at index and then updates the lowest index.
    //
    // index - int index between 0 and _maximumIndex of bit to clear.
    //
    inline void free(int index) {
        clear(index);
        updateLowestIndex(index);
    }

    //
    // updateLowestFreeWordIndex updates the lowest free word index if the
    // specified word is lower.
    //
    // wordIndex - int between 0 and _maximumWordIndex.
    //
    inline void updateLowestFreeWordIndex(int wordIndex) {
        assert(isValidWordIndex(wordIndex) &&
               "wordIndex out of range");
        //
        // Sample current lowest free word index.
        //
        int lowestFreeIndex = getLowestIndex();

        //
        // Loop until boundary condition is met.
        //
        do {
            //
            // If the current value is lower then don't bother.
            //
            if (lowestFreeIndex <= wordIndex) {
                break;
            }

            //
            // Attempt to update. Exit if successful. Otherwise have a new value
            // for lowest free word index.
            //
        } while (!swapLowestIndex(lowestFreeIndex, wordIndex));
    }

    //
    // updateLowestIndex updates the lowest free index if the
    // specified index word is lower.
    //
    // index - int between 0 and _maximumIndex.
    //
    inline void updateLowestIndex(int index) {
        assert(isValidIndex(index) &&
               "index out of range");

        //
        // Use word index version.
        //
        updateLowestFreeWordIndex(getWordIndex(index));
    }

    //
    // isEmpty makes a best guess attempt to see if registry is empty. Can be
    // accurate if registry is offline. Best used to sample if possibly empty
    // then offline registry and the try again for accurate result.
    //
    inline bool isEmpty() const {
        if (getLowestIndex() == 0) {
            for (int i = 0; i < _maximumWordIndex; i++) {
                if (getBits(i) != ZERO) {
                    return false;
                }
            }

            return true;
        }

        return false;
    }

    //
    // findFreeRange finds "count" consecutive free bits.
    //
    // count - int value between 0 and maximumIndex number of free bits
    // required.
    //
    int findFreeRange(int count) {
        assert(isValidCount(count) &&
               "count is out of range");

        //
        // Shortcut for zero count.
        //
        if (count == 0) {
            return NOT_FOUND;
        }

        //
        // Shortcut for count of one.
        //
        if (count == 1) {
            return findFree();
        }

        //
        // Search all the words between lowest free word index and maximumIndex.
        // Ignore updates to lowest free word index since they will typically
        // be minor single bit updates.
        //
        for (int wordIndex = getLowestIndex();
             wordIndex < _maximumIndex;
             wordIndex++
        )
        {
            //
            // Start looking a first word
            //
            uint64_t value = getBits(wordIndex);

            //
            // If bits would value to be in at most two words.
            //
            if (count <= BITS_PER_WORD) {
                //
                // Find the first few free bits.
                //
                int bitIndex = lowestZeroBitsPosition(value, count);
                int index = getIndex(wordIndex, bitIndex);

                //
                // If the bits exceed the registry count (overflow safe test.)
                //
                if (_maximumIndex - count <= index) {
                    return NOT_FOUND;
                }

                //
                // If the bits are all in one word.
                //
                if (bitIndex + count <= BITS_PER_WORD) {
                    //
                    // Attempt to set bits.
                    //
                    if (conditionallySetMask(wordIndex,
                                             loMask(count) << bitIndex)) {
                        return index;
                    }
                } else {
                    //
                    // Attempt to set range of bits.
                    //
                    if (conditionallySetMaskRange(wordIndex, 0,
                                                  loMask(count) << bitIndex,
                                                  loMask(count - bitIndex))) {
                        return index;
                    }
                }
            } else {
                //
                // Spanning multiple words starting with upper bits of first
                // word.
                //
                int leading = clz(value);
                int index = getIndex(wordIndex, BITS_PER_WORD - leading);

                //
                // If the bits exceed the registry count (overflow safe test.)
                //
                if (_maximumIndex - count <= index) {
                    return NOT_FOUND;
                }

                //
                // Attempt to update range of bits.
                //
                int remaining = count - leading;
                int remainingWords = getWordIndex(remaining);
                int remainingBits = getBitIndex(remaining);
                uint64_t leadingMask = hiMask(leading);
                uint64_t trailingMask = loMask(remainingBits);

                if (conditionallySetMaskRange(wordIndex, remainingWords,
                                              leadingMask, trailingMask)) {
                    return index;
                }
            }
        }

        return NOT_FOUND;
    }

    //
    // freeRange frees (zeroes) a range of "count" concecutive bits starting
    // at "index".
    //
    // index - int between 0 and maximumIndex. First bit of range.
    // count - int number of bits to clear.
    //
    void freeRange(int index, int count) {
        assert(isValidIndex(index) &&
               "index is out of range");
        assert(isValidCount(count) &&
               "count is out of range");
        assert(isValidCount(index + count) &&
               "index + count is out of range");
        int wordIndex = getWordIndex(index);
        int bitIndex = getBitIndex(index);

        //
        // If range only spans two words.
        //

        if (bitIndex + count <= BITS_PER_WORD) {
            //
            // Bits in a single word.
            //
            clearMask(wordIndex, loMask(count) << bitIndex);
        } else if (count <= BITS_PER_WORD) {
            //
            // Bits span two words.
            //
            clearMaskRange(wordIndex, 0, loMask(count) << bitIndex,
                                         loMask(count - bitIndex));
        } else {
            //
            // Bits span multiple words.
            //
            int leading = BITS_PER_WORD - bitIndex;
            int remaining = count - leading;
            int remainingWords = getWordIndex(remaining);
            int remainingBits = getBitIndex(remaining);
            uint64_t leadingMask = hiMask(leading);
            uint64_t trailingMask = loMask(remainingBits);
            clearMaskRange(wordIndex, remainingWords,
                           leadingMask, trailingMask);
        }

        //
        // Update the lowest free word index.
        //
        updateLowestFreeWordIndex(wordIndex);
    }

    //
    // count does a sample enumeration of an active registry's set bits.
    //
    inline int count() const {
        int c = 0;

        for (int i = 0; i < _maximumWordIndex; i++) {
            c += popcount(getBits(i));
        }

        return c;
    }
};

//----------------------------------------------------------------------------//
//
// The RegistryIsSetIterator class iterates through set bits in a registry.
//
class RegistryIsSetIterator : public NoAllocate {
private:
    //
    // Registry being scanned.
    //
    Registry *_registry;

    //
    // Last bit viewed.
    //
    int _index;

    //
    // If more bits are possible.
    //
    inline bool hasMore() {
        return _index < _registry->maximumIndex();
    }

public:
    //
    // Constructor.
    //
    // registry - Registry to scan.
    // index - int between 0 and registry's maximumIndex indicating starting
    //         point.
    //
    RegistryIsSetIterator(Registry *registry, int index = 0) :
        _registry(registry),
        _index(index)
    {
        assert(registry &&
               "registry is null");
        assert(registry->isValidIndex(index) &&
               "index is out of range");
    }

    //
    // nextSet returns next bit index in the bitmap.
    //
    inline int nextSet() {
        const int maximumIndex = _registry->maximumIndex();
        const int maximumWordIndex = _registry->maximumWordIndex();
        int bitIndex = Registry::getBitIndex(_index);

        //
        // Iterate through bitmap words.
        //
        for (int wordIndex = Registry::getWordIndex(_index);
             wordIndex < maximumWordIndex;
             wordIndex++)
        {
            //
            // Fetch word value and mask out "seen" bits.
            //
            uint64_t value = _registry->getBits(wordIndex);
            value &= ~MASK(twoToOrder(bitIndex));

            //
            // If any bits remaining
            //
            if (value) {
                //
                // Locate lowest bit.
                //
                bitIndex = ctz(value);

                //
                // Combine word and bit index for result.
                //
                int index = Registry::getIndex(wordIndex, bitIndex);

                if (maximumIndex <= index) {
                    //
                    // Invalid result.
                    //
                    break;
                }

                //
                // Have valid index, then update saved index and return result.
                //
                _index = index + 1;

                return index;
            }

            //
            // Reset bit index for next word.
            //
            bitIndex = 0;
        }

         _index = maximumIndex;

        return NOT_FOUND;
    }
};

//----------------------------------------------------------------------------//
//
// The AllocateBulkIterator class is used by a partition to accumulate "count"
// free addresses in an address buffer.
//
class AllocateBulkIterator {
private:
    //
    // Registry being scanned.
    //
    Registry *_registry;

    //
    // Partition base address.
    //
    const void *_base;

    //
    // Size order of quantum in partition.
    //
    const int _order;

    //
    // Size of buffer and number of addresses to accumulate.
    //
    const int _count;

    //
    // Address buffer.
    //
    void **_addresses;

    //
    // Number of addresses actually accumulated.
    //
    int _allocated;

public:
    //
    // Constructor.
    //
    // registry - Registry to scan.
    // base - Partition base address.
    // order -  Size order of quantum in partition.
    // count - Size of buffer and number of addresses to accumulate.
    // addresses - Address buffer.
    //
    AllocateBulkIterator(
        Registry *registry,
        void *base,
        int order,
        int count,
        void **addresses
    ) :
        _registry(registry),
        _base(base),
        _order(order),
        _count(count),
        _addresses(addresses),
        _allocated(0)
    {
        assert(registry != nullptr &&
               "registry is null");
        assert(0 <= order && order <= MAX_ORDER &&
               "order is out of range");
        assert(0 < count &&
               "count is out of range");
        assert(addresses != nullptr &&
               "addresses should not be null");
    }

    //
    // iterate triggers iteration. For each free bit found calls the overridden
    // foundFree method. Continues until foundFree returns false.
    //
    inline void iterate() {
        const int maximumIndex = _registry->maximumIndex();
        const int maximumWordIndex = _registry->maximumWordIndex();

        //
        // Scan each word in registry bitmap.
        //
        for (int wordIndex = _registry->getLowestIndex();
             wordIndex < maximumWordIndex;
             wordIndex++
        )
        {
            //
            // Fetch word value.
            //
            uint64_t value = _registry->getBits(wordIndex);

            //
            // Pre-set all bits in word. Keep trying until get a valid snapshot.
            //
            while (value != ALL_ONES &&
                 !_registry->conditionallySetMask(wordIndex, value, ALL_ONES)) {
            }

            //
            // While there are free bits in the snapshot.
            //
            while (value != ALL_ONES) {
                //
                // Get lowest zero bit mask and compute index.
                //
                uint64_t lowestBit = lowestZeroBit(value);
                int bitIndex = BITS_PER_WORD - 1UL - clz(lowestBit);
                int index = Registry::getIndex(wordIndex, bitIndex);

                if (maximumIndex <= index) {
                    //
                    // If exceeded registry count.
                    //
                    break;
                }

                //
                // Invoke supplied foundFree with index.
                //
                if (!foundFree(index)) {
                    //
                    // If all done then clear unused bits.
                    //
                    if (value != ALL_ONES) {
                        _registry->clearMask(wordIndex, ~value);
                    }

                    return;
                }

                //
                // Mark bit in local snapshot as set.
                //
                value |= lowestBit;
            }

            //
            // If last word then clear unused bits.
            //
            if (value != ALL_ONES) {
                _registry->clearMask(wordIndex, ~value);
            }
        }
    }

    //
    // allocated returns the number of addresses actually accumulated.
    //
    inline int allocated() const {
        return _allocated;
    }

    //
    // foundFree is the overridden function that gets called when a free bit is
    // found. Returns false when address buffer is full.
    //
    // index - int bit index where free bit was found.
    //
    bool foundFree(int index) {
        assert(_registry->isValidIndex(index) &&
               "index is out of range");

        //
        // As long as the buffer is not full.
        //
        if (_allocated < _count) {
            Address address(_base, orderMul(index, _order));
            _addresses[_allocated++] = address;

            return true;
        }

        return false;
    }
};

//----------------------------------------------------------------------------//
//
// The DeallocateBulk class collects registry bits being freed and clears them
// on a word by word basis.
//
class DeallocateBulk : public NoAllocate {
private:
    //
    // Registry being updated.
    //
    Registry *_registry;

    //
    // Word index of the current word.
    //
    int _wordIndex;

    //
    // Current collection of free bits for the "wordIndex"ed word.
    //
    uint64_t _value;

public:
    //
    // Constructor.
    //
    // registry - Registry being updated.
    //
    DeallocateBulk(Registry *registry) :
        _registry(registry),
        _wordIndex(NOT_FOUND),
        _value(ZERO)
    {
        assert(registry != nullptr &&
               "registry is null");
    }

    //
    // Destructor
    //
    ~DeallocateBulk() {
        //
        // Make sure remaining bits are flushed out.
        //
        flush();
    }

    //
    // flush flushes out any pending collection of free bits.
    //
    inline void flush() {
        //
        // Only if bits are available.
        //
        if (_wordIndex != NOT_FOUND) {
            assert(_registry->isValidIndex(_wordIndex) &&
                   "index is out of range");

            //
            // Clear collection bits and restart.
            //
            _registry->clearMask(_wordIndex, _value);
            _wordIndex = NOT_FOUND;
            _value = ZERO;
        }
    }

    //
    // clear indicates that the bit at "index" should be cleared.
    //
    inline void clear(int index) {
        assert(_registry->isValidIndex(index) &&
               "index is out of range");
        int wordIndex = Registry::getWordIndex(index);
        int bitIndex = Registry::getBitIndex(index);

        //
        // Flush out any pending bits in another word.
        //
        if (wordIndex != _wordIndex) {
            flush();

            _wordIndex = wordIndex;
        }

        _value |= twoToOrder(bitIndex);
    }
};

//----------------------------------------------------------------------------//
//
// The Space class defines the bounds of a managed memory range.
//
class Space : public NoAllocate {
protected:
    //
    // The base or lower bounds (inclusive) of a memory range.
    //
    void *_base;

    //
    // The limit or upper bounds (exclusive) of a memory range.
    //
    void *_limit;

public:
    //
    // Constructor used for initializing global data.
    //
    Space() :
        _base(nullptr),
        _limit(nullptr)
    {
    }

    //
    // Constructor used when activating a specific range.
    //
    // base - base or lower bounds (inclusive) of a memory range.
    // size - the number of bytes in the range.
    //
    Space(void *base, uint64_t size) :
        _base(base),
        _limit(Address(base, size))
    {
        assert(_base <= _limit &&
               "base should be less equal than limit");
    }

    //
    // base returns the lower bounds (inclusive) of the range.
    //
    inline void *base() const {
        return _base;
    }

    //
    // limit returns the upper bounds (exclusive) of the range.
    //
    inline void *limit() const {
        return _limit;
    }

    //
    // size returns the number bytes in the range.
    //
    inline uint64_t size() const {
        return Address(_limit) - Address(_base);
    }

    //
    // in tests if the "address" is in the bounds of the range.
    //
    // address - memory address to test.
    //
    inline bool in(void *address) const {
        return _base <= address && address < _limit;
    }
};

//----------------------------------------------------------------------------//
//
// Arena for simple internal allocation.
//
class Arena : public Space {
protected:
    //
    // Next allocation address.
    //
    void *_next;

public:
    //
    // Sizing Constructor. Used to compute size of allocation sequence.
    //
    Arena() :
        Space(nullptr, ALL_ONES),
        _next(nullptr)
    {
    }

    //
    // Constructor.
    //
    Arena(
        void *base,
        uint64_t size
     ) :
        Space(base, size),
        _next(base)
    {
    }

    //
    // Return next allocation.
    //
    template<typename T>
    inline T *allocate(uint64_t size) {
        void *address = _next;
        uint64_t alignedSize = roundUp(size, BYTES_PER_WORD);
        Address next(_next, alignedSize);

        if (limit() < next) {
            assert(limit() < next &&
                   "arena out of space");
            return nullptr;
        }

        _next = next;

        return static_cast<T *>(address);
    }

    inline void *allocate(uint64_t size) {
        return allocate<void>(size);
    }

    //
    // Return number of bytes allocated.
    //
    inline uint64_t allocated() const {
        return reinterpret_cast<char *>(_next) -
               reinterpret_cast<char *>(base());
    }
};

//----------------------------------------------------------------------------//
//
// Forward quantum allocator.
//
class NullAllocator;
class Partition;
class QuantumAllocator;
class SlabAllocator;

//
// Persistent IDs for each of the allocators types.
//

const static int NullAllocatorID = 0;
const static int PartitionID = 1;
const static int QuantumAllocatorID = 2;
const static int SlabAllocatorID = 3;

//----------------------------------------------------------------------------//
//
// The Allocator class defines the minimum set of functions that all
// allocators should define.
//
class Allocator : public Space {
private:
    //
    // Persistent ID.
    //
    const int _ID;

    //
    // The size order of the smallest quantum handled by this allocator.
    //
    const int _smallestSizeOrder;

    //
    // The size order of the largest quantum handled by this allocator.
    //
    const int _largestSizeOrder;

public:
    //
    // Constructor used when activating a specific allocator.
    //
    // base - lower bounds of memory managed by allocator.
    // size - number of bytes managed by allocator.
    // ID - allocator ID.
    // smallestSizeOrder - size order of the smallest quantum handled by this
    //                     allocator.
    // largestSizeOrder - size order of the largest quantum handled by this
    //                     allocator.
    //
    Allocator(
        void *base,
        uint64_t size,
        int ID,
        int smallestSizeOrder,
        int largestSizeOrder
    ) :
        Space(base, size),
        _ID(ID),
        _smallestSizeOrder(smallestSizeOrder),
        _largestSizeOrder(largestSizeOrder)
    {
    }

    //
    // isNullAllocator tests if this is a Null allocator.
    //
    bool isNullAllocator() {
        return _ID == NullAllocatorID;
    }

    //
    // isPartition tests if this is a partition allocator.
    //
    bool isPartition() {
        return _ID == PartitionID;
    }

    //
    // isQuantumAllocatorUnsecure tests if this is a quantum allocator.
    //
    bool isQuantumAllocator() {
        return _ID == QuantumAllocatorID;
    }

    //
    // isSlabAllocator tests if this is a Slab allocator.
    //
    bool isSlabAllocator() {
        return _ID == SlabAllocatorID;
    }

    //
    // asNullAllocator casts this as NullAllocator.
    //
    NullAllocator *asNullAllocator() {
        assert(isNullAllocator() &&
               "is not Null allocator");
        return reinterpret_cast<NullAllocator *>(this);
    }

    //
    // asPartition casts this as Partition.
    //
    Partition *asPartition() {
        assert(isPartition() &&
               "is not partition allocator");
        return reinterpret_cast<Partition *>(this);
    }

    //
    // asQuantumAllocator casts this as QuantumAllocator.
    //
    QuantumAllocator *asQuantumAllocator() {
        assert(isQuantumAllocator() &&
               "is not quantum allocator");
        return reinterpret_cast<QuantumAllocator *>(this);
    }

    //
    // asSlabAllocator casts this as SlabAllocator.
    //
    SlabAllocator *asSlabAllocator() {
        assert(isSlabAllocator() &&
               "is not Slab allocator");
        return reinterpret_cast<SlabAllocator *>(this);
    }

    //
    // smallestSizeOrder returns the quantum allocators smallest quantum size
    // order.
    //
    inline int smallestSizeOrder() const {
        return _smallestSizeOrder;
    }

    //
    // largestSizeOrder returns the quantum allocators largest quantum size
    // order.
    //
    inline int largestSizeOrder() const {
        return _largestSizeOrder;
    }

     //
    // smallestSize returns the quantum allocators smallest quantum size.
    //
    inline uint64_t smallestSize() const {
        return orderToSize(_smallestSizeOrder);
    }

    //
    // largestSize returns the quantum allocators largest quantum size.
    //
    inline uint64_t largestSize() const {
        return orderToSize(_largestSizeOrder);
    }

};

//----------------------------------------------------------------------------//
//
// The NullAllocator class is used to respond to allocation requests for size
// orders not handled by any other allocator. That is, it always returns nullptr
// for allocations of orders higher than MAX_ALLOCATION_ORDER.
//
class NullAllocator : public Allocator {
public:
    //
    // Constructor used for initializing global data and specific null
    // allocators.
    //
    NullAllocator() :
        Allocator(nullptr, ZERO, NullAllocatorID, 0, 0)
    {
    }

    //
    // allocate always returns nullptr no matter the "size".
    //
    // size - number of bytes to allocate likely zero or value greater than
    //        MAX_ALLOCATION_SIZE.
    //
    inline void *allocate(uint64_t size) {
        return nullptr;
    }

    //
    // deallocate does nothing.
    //
    // address - nullptr.
    //
    inline void deallocate(void *address) {
        assert(address == nullptr &&
               "address should be null");
    }

    //
    // allocateCount always returns nullptr no matter the "size" or "count".
    //
    // size - number of bytes to allocate likely zero or value greater than
    //        MAX_ALLOCATION_SIZE.
    // count - number of blocks to allocate.
    //
    inline void *allocateCount(uint64_t size, int count) {
        assert(0 <= count &&
               "count should be positive");
        return nullptr;
    }

    //
    // deallocateCount does nothing.
    //
    // address - nullptr.
    // size - number of bytes to allocate likely zero or value greater than
    //        MAX_ALLOCATION_SIZE.
    // count - number of blocks to deallocate.
    //
    inline void deallocateCount(void *address, uint64_t size, int count) {
        assert(address == nullptr &&
               "address should be null");
        assert(0 <= count &&
               "count should be positive");
    }


    //
    // allocateBulk always returns zero.
    //
    // size - Size of blocks in bytes. Should be power of two.
    // count - length of addresses buffer.
    // addresses - address buffer.
    // contiguous - true if blocks should be contiguous (faster but wasteful.)
    //
    inline int allocateBulk(
        uint64_t size,
        int count,
        void **addresses,
        bool contiguous
    ) {
        return 0;
    }

    //
    // deallocateBulk always returns 0.
    //
    // count - length of addresses buffer.
    // addresses - address buffer.
    //
    inline int deallocateBulk(int count, void **addresses) {
        return 0;
    }

    //
    // clear zeroes out the content of a memory block.
    //
    // address - nullptr.
    //
    inline void clear(void *address) {
        assert(address == nullptr &&
               "address should be null");
    }

    //
    // allocationSize always returns zero.
    //
    // address - nullptr.
    //
    inline size_t allocationSize(void *address) {
        return ZERO;
    }

    //
    // allocationBase always returns nullptr.
    //
    // address - nullptr.
    //
    inline void *allocationBase(void *address) {
        return nullptr;
    }

    //
    // allocationSideData always returns nullptr.
    //
    // address - nullptr.
    //
    inline void *allocationSideData(void *address) {
        return nullptr;
    }

    //
    // nextAllocation always returns nullptr.
    //
    // address - nullptr.
    //
    inline void *nextAllocation(void *address) {
        assert(address == nullptr &&
               "address should be null");
        return nullptr;
    }

    //
    // stats adds nothing to the stats.
    //
    // counts - counts buffer.
    // sizes - sizes buffer.
    //
    inline void stats(uint64_t *counts, uint64_t *sizes) {
        assert(counts != nullptr &&
               "counts should not be null");
        assert(sizes != nullptr &&
               "sizes should not be null");
    }
};

//----------------------------------------------------------------------------//
//
// The AllocatorRoster class is used to assign allocators to specific size
// orders. A Director class instance is usually responsible for making the
// initial assignments. The entries in the roster are atomic because they can
// change over time. Ex. A partition allocator may take over for a quantum
// allocator for a specific order and thereby removing the middleman (overhead.)
//
class AllocatorRoster : public NoAllocate {
private:
    //
    // An table of allocators indexed by size order. The allocator at a given
    // index (order) can presumably allocate blocks of that size.
    //
    std::atomic<Allocator *> allocators[MAX_ORDER];

public:
    //
    // getAllocator returns the allocator assigned to "order".
    //
    // order - int between 0 and MAX_ORDER, size being 2^order.
    //
    inline Allocator *getAllocator(int order) {
        assert(SMALLEST_SIZE_ORDER <= order && order <= MAX_ORDER &&
               "order is out of range");

        return allocators[order].load();
    }

    //
    // setAllocator assigns an "allocator" to a specific "order".
    //
    // allocator - allocator instance used to allocate blocks of size order.
    // order - int between 0 and MAX_ORDER, size being 2^order.
    //
    inline void setAllocator(Allocator *allocator, int order) {
        assert(allocator != nullptr &&
               "allocator should not be null");
        assert(0 <= order && order <= MAX_ORDER &&
               "order is out of range");

        allocators[order].store(allocator);
    }

    //
    // setAllocators assigns an "allocator" to a range of orders.
    //
    // allocator - allocator instance used to allocate blocks of size order.
    // loOrder - lower bounds of order range (inclusive).
    // hiOrder - upper bounds of order range (exclusive).
    //
    inline void setAllocators(Allocator *allocator, int loOrder, int hiOrder) {
        assert(allocator != nullptr &&
               "allocator should not be null");
        assert(0 <= loOrder && loOrder <= MAX_ORDER &&
               "loOrder is out of range");
        assert(0 < hiOrder && hiOrder <= MAX_ORDER &&
               "hiOrder is out of range");

        for (int i = loOrder; i < hiOrder; i++) {
            allocators[i].store(allocator);
        }
    }
};

//----------------------------------------------------------------------------//
//
// The Partition class is a specialized allocator for a specific quantum in
// a single partition.
//
class Partition : public Allocator {
private:
    //
    // true if allocations are shared.
    //
    bool _sharing;

    //
    // Managing QuantumAllocator.
    //
    QuantumAllocator *_quantumAllocator;

    //
    // Order of the quantum size.
    //
    const int _quantumSizeOrder;

    //
    // Quantum allocation registry.
    //
    Registry _registry;

    //
    // Size of a quantum side data.
    //
    int _sideDataSize;

    //
    // Side data for each allocation.
    //
    char *_sidedata;

    //
    // quantumIndex returns the index of the quantum containing the "address".
    //
    // address - address in the quantum allocation.
    //
    inline int quantumIndex(Address address) {
        return address.getIndex(base(), _quantumSizeOrder);
    }

public:
    //
    // Constructor for specific partition and quantum size.
    //
    // sharing - true if allocations are shared.
    // quantumAllocator - the managing QuantumAllocator.
    // base - base address of the partition.
    // partitionSize - size of the partition in bytes.
    // quantumSize - size of the quantum in bytes.
    // sideDataSize - size of a quantum side data.
    // sideData - space for quantum side data.
    // ID - allocator ID.
    //
    Partition(
        bool sharing,
        QuantumAllocator *quantumAllocator,
        void *base,
        uint64_t partitionSize,
        uint64_t quantumSize,
        int sideDataSize,
        char *sideData,
        int ID = PartitionID
    ) :
        Allocator(
            base,
            partitionSize,
            ID,
            sizeToOrder(quantumSize),
            sizeToOrder(quantumSize)
        ),
        _sharing(sharing),
        _quantumAllocator(quantumAllocator),
        _quantumSizeOrder(sizeToOrder(quantumSize)),
        _registry(orderDiv(partitionSize, _quantumSizeOrder)),
        _sideDataSize(sideDataSize),
        _sidedata(sideData)
    {
        assert(quantumAllocator != nullptr &&
               "quantumAllocator is null");
        assert(isPowerOf2(partitionSize) &&
               "invalid partitionSize");
        assert(base != nullptr &&
               "base is null");
        assert(isPowerOf2(quantumSize) &&
               quantumSize <= orderToSize(LARGEST_SIZE_ORDER) &&
               "invalid quantumSize");
        assert(0 <= sideDataSize &&
               "side data size should be positive");
        assert(sideData != nullptr &&
               "side data should not be null");
    }

    //
    // getQuantumAllocator returns managing quantum allocator.
    //
    inline QuantumAllocator *getQuantumAllocator() const {
        return _quantumAllocator;
    }

    //
    // quantumOrder returns the size order for all quanta in this partition.
    //
    inline int quantumOrder() const {
        return _quantumSizeOrder;
    }

    //
    // quantumSize returns the size for all quanta in this partition.
    //
    inline uint64_t quantumSize() const {
        return orderToSize(_quantumSizeOrder);
    }

    //
    // isEmpty speculatively returns true is this partition is empty.
    //
    inline bool isEmpty() const {
        return _registry.isEmpty();
    }

    //
    // allocateBulk is helper function for allocating addresses in bulk from
    // this partition. Returns the number addresses actually allocated (may be
    // zero.)
    //
    // count - length of addresses buffer.
    // addresses - address buffer.
    //
    inline int allocateBulk(int count, void **addresses) {
        assert(0 <= count &&
               "count should be positive");
        assert(addresses != nullptr &&
               "addresses should not be null");
        AllocateBulkIterator allocateBulkIterator(&_registry,
                                                  _base, _quantumSizeOrder,
                                                  count, addresses);
        allocateBulkIterator.iterate();

        return allocateBulkIterator.allocated();
    }

    //
    // allocateBulk is helper function for allocating addresses in bulk from
    // this partition. Returns the number addresses actually allocated (may be
    // zero.) Unlike allocateBulk this function requires the addresses are
    // consecutive (faster.)
    //
    // count - length of addresses buffer.
    // addresses - address buffer.
    //
    inline int allocateBulkContiguous(int count, void **addresses) {
        //
        // Find consecutive bits in quantum registry.
        //
        int index = _registry.findFreeRange(count);

        if (index == NOT_FOUND) {
            return 0;
        }

        //
        // Synthesize addresses.
        //
        for (int i = 0; i < count; i++) {
            Address address(_base, orderMul(index + i, _quantumSizeOrder));
            addresses[i] = address;
        }

        return count;
    }

    //
    // allocate - attempts to allocate a block. If it can not then send the
    // request to the managing quantum allocator.
    //
    // size - Number of bytes to allocate. Ignored since all allocations in the
    //        partition are the same size.
    //
    inline void *allocate(uint64_t size) {
        assert(size <= quantumSize() &&
               "size is not valid for this partition");
        int index = _registry.findFree();

        if (index == NOT_FOUND) {
            return nullptr;
        }

        Address address(_base, orderMul(index, _quantumSizeOrder));

        return address;
    }

    //
    // deallocate frees the quantum that contains the "address".
    //
    // address - address of memory block to deallocate.
    //
    inline void deallocate(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");

        int index = quantumIndex(address);
        assert(_registry.isSet(index) &&
               "double deallocate");

        _registry.free(index);
    }

    //
    // allocateBulk allocates addresses in bulk and puts them in the "addresses"
    // buffer. Returns the number addresses actually allocated (may be zero.)
    //
    // size - Size of blocks in bytes. Should be power of two.
    // count - length of addresses buffer.
    // addresses - address buffer.
    // contiguous - true if blocks should be contiguous (faster but wasteful.)
    //
    inline int allocateBulk(
        uint64_t size,
        int count,
        void **addresses,
        bool contiguous
    )
    {
        if (contiguous) {
            return allocateBulk(count, addresses);
        } else {
            return allocateBulkContiguous(count, addresses);
        }
    }

    //
    // deallocateBulk is a helper function for deallocating addresses en masse.
    // This is faster than individual calls to deallocate since it reduces the
    // number of atomic writes to the quantum registry.
    //
    // count - length of addresses buffer.
    // addresses - address buffer.
    // secure - true if shoud be zeroed.
    //
    inline int deallocateBulk(int count, void **addresses, bool secure) {
        assert(0 <= count &&
               "count should be positive");
        assert(addresses != nullptr &&
               "addresses should not be null");
        DeallocateBulk deallocateBulk(&_registry);

        int deallocated = 0;

        while (deallocated < count) {
            void *address = addresses[deallocated];

            if (!in(address)) {
                break;
            }

            if (secure) {
                System::clear(address, quantumSize(), _sharing);
            }

            deallocateBulk.clear(quantumIndex(address));
            deallocated++;
        }

        return deallocated;
    }

    //
    // clear zeroes out the content of a memory block.
    //
    // address - address of memory block to clear.
    //
    inline void clear(void *address) {
        System::clear(allocationBase(address), quantumSize(), _sharing);
    }

    //
    // allocateCount allocates "count" consecutive blocks of "size" bytes. If
    // it can not then send the request to the managing quantum allocator.
    //
    // size - Number of bytes to allocate. Ignored since all allocations in the
    //        partition are the same size.
    // count - number of blocks to allocate.
    //
    inline void *allocateCount(uint64_t size, int count) {
        assert(size <= quantumSize() &&
               "size is not valid for this partition");
        assert(0 <= count &&
               "count should be positive");

        // Exit early if it is impossible to allocate "count" quanta in a
        // single partition.
        //
        if (orderDiv(this->size(), _quantumSizeOrder) < count) {
            return nullptr;
        }

        //
        // Find consecutive bits in quantum registry.
        //
        int index = _registry.findFreeRange(count);

        //
        // If not found.
        //
        if (index == NOT_FOUND) {
            return nullptr;
        }

        //
        // Produce address.
        //
        Address address(_base, orderMul(index, _quantumSizeOrder));

        return address;
    }

    //
    // deallocateCount frees "count" consecutive blocks of "size" bytes.
    //
    // address - address of memory blocks to deallocate.
    // secure - true if shoud be zeroed.
    // size - Number of bytes to allocate. Ignored since all allocations in the
    //        partition are the same size.
    // count - number of blocks to deallocate.
    //
    inline void deallocateCount(
        void *address,
        bool secure,
        uint64_t size,
        int count
    ) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        assert(size <= quantumSize() &&
               "size is not valid for this partition");
        assert(0 <= count &&
               "count should be positive");

        if (secure) {
            System::clear(address,
                          orderMul(count, _quantumSizeOrder),
                          _sharing);
        }

        int index = quantumIndex(address);
        assert(_registry.isSet(index) &&
               "double deallocate");
        _registry.freeRange(index, count);
    }

    //
    // allocationSize returns number of bytes allocated at the "address".
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline size_t allocationSize(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");

        return quantumSize();
    }

    //
    // allocationBase returns the base address of an allocated block containing
    // the "address".
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline void *allocationBase(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        Address base(address);

        return base & ~MASK(quantumSize());
    }

    //
    // allocationSideData returns the address of side data reserved for the
    // allocation at "address". The size of side data is a configuration
    // parameter. If the size of side data is zero then allocationSideData
    // returns nullptr.
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline void *allocationSideData(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");

        return static_cast<void *>(_sidedata + quantumIndex(address) *
                                               _sideDataSize);
    }

    //
    // nextAllocation can be used to "walk" through all the allocations
    // managed by QBA. The first call should have an "address" of nullptr with
    // successive calls using the result of the previous call. The result
    // itself can not be used for memory access since the result may have been
    // deallocated after fetching (potential seg fault). The result can however
    // be used to make calls to allocationSize or allocationSideData.
    //
    // address - nullptr or result of last call to nextAllocation.
    //
    inline void *nextAllocation(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        RegistryIsSetIterator iterator(&_registry,
                                       address ? quantumIndex(address) + 1 : 0);
        int index = iterator.nextSet();

        return index == NOT_FOUND ? nullptr :
                    (void *)Address(base(), orderMul(index, _quantumSizeOrder));
    }

    //
    // stats fills in "counts" and "sizes" buffers with information known to
    // this allocator. Specifically stats updates the quantum order slots with
    // the sample count of in-use bits in the registry.
    //
    // counts - counts buffer.
    // sizes - sizes buffer.
    //
    inline void stats(uint64_t *counts, uint64_t *sizes) {
        assert(counts != nullptr &&
               "counts should not be null");
        assert(sizes != nullptr &&
               "sizes should not be null");
        int count = _registry.count();
        counts[_quantumSizeOrder] += count;
        sizes[_quantumSizeOrder] += orderMul(count, _quantumSizeOrder);
    }
};

//----------------------------------------------------------------------------//
//
// The QuantumAllocator class manages a span of memory subdivided into
// partitions. There can be multiple QuantumAllocators managed by a Director.
// The reason for doing this is to keep the ratio of partition size and quantum
// size to the low end. This in turn keeps quantum registries small and fast.
//
// This is why QuantumAllocator is a template. The template provides the
// ability to get the best configuration for the size of quanta and partitions
// being managed.
//
class QuantumAllocator : public Allocator {
private:
    //
    // true if allocations are shared.
    //
    bool _sharing;

    //
    // Reference to the main roster managed by the Director. This is required
    // to swap in specialized partition allocators for specific size orders.
    //
    AllocatorRoster *_roster;

    //
    // Order of the partition size handled by this allocator.
    //
    const int _partitionSizeOrder;

    //
    // Number of partitions managed by this allocator.
    //
    const int _partitionCount;

    //
    // Partition size handled by this allocator.
    //
    const uint64_t _partitionSize;

    //
    // Smallest quantum size order.
    //
    const int _smallestSizeOrder;

    //
    // Largest quantum size order.
    //
    const int _largestSizeOrder;

    //
    // Smallest quantum size.
    //
    const uint64_t _smallestSize;

    //
    // Largest quantum size.
    //
    const uint64_t _largestSize;

    //
    // Initially unconfigured allocators for each managed partition. Updated
    // as partitions are brought online.
    //
    Partition *_partitions;

    //
    // Registry of partitions in-use.
    //
    Registry _partitionRegistry;

    //
    // Registries of partitions in-use, broken down into size orders. Used to
    // find partitions online for specific sizes.
    //
    Registry _orderRegistry[MAX_QUANTUM_ALLOCATOR_ORDERS];

    //
    // Size of a quantum side data.
    //
    int _sideDataSize;

    //
    // Side data space passed to partition allocator when constructed.
    //
    char *_sideData;

    //
    // getPartition returns the partition allocator at "partitionIndex".
    //
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    //
    inline Partition *getPartition(int partitionIndex) {
        assert(0 <= partitionIndex && partitionIndex < _partitionCount &&
               "partition out of range");
        return _partitions + partitionIndex;
    }

    //
    // newPartition initializes the partition at "partitionIndex" for quantum of
    // size "size" and then returns its partition allocator.
    //
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    // size - Size of quantum managed by the partition.
    //
    inline Partition *newPartition(
        int partitionIndex,
        uint64_t size
    )
    {
        assert(0 <= partitionIndex && partitionIndex < _partitionCount &&
               "partition out of range");
        assert(isPowerOf2(size) &&
               "size must be a power of 2");
        assert(_smallestSize <= size && size <= _largestSize &&
               "size must be appropriate for allocator");
        Address address(base(), orderMul(partitionIndex, _partitionSizeOrder));

        return new(_partitions + partitionIndex)
               Partition(
                 _sharing,
                 this,
                 address,
                 _partitionSize,
                 size,
                 _sideDataSize,
                 _sideData + _sideDataSize *
                             partitionIndex *
                             MAX_PARTITION_QUANTUM
                 );
    }

    //
    // getOrderIndex returns the local order index (_orderRegistry index.)
    //
    // size - size of allocation in bytes.
    //
    inline int getOrderIndex(uint64_t size) const {
        assert(sizeToOrder(size) <= _partitionSizeOrder &&
               "size must be less than partition size");
        return sizeToOrder(size) - smallestSizeOrder();
    }

    //
    // addToOrder adds a partition to an order registry and then makes its
    // allocator the primary allocator for allocations of that order.
    //
    // orderIndex - The size order of the partition relative to the
    //              _smallestSizeOrder.
    // partition - The partition allocator.
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    //
    inline void addToOrder(
        int orderIndex,
        Partition *partition,
        int partitionIndex
    )
    {
        assert(0 <= orderIndex &&
               orderIndex < MAX_QUANTUM_ALLOCATOR_ORDERS &&
               "order index out of range");
        //
        // TODO - Set in roster conditionally. If other thread won the race then
        // offline the new partition and use theirs.
        //
        onlinePartition(partitionIndex, orderIndex);
        _roster->setAllocator(partition, smallestSizeOrder() + orderIndex);
    }

    //
    // newOrderPartition creates a new partition allocator and puts it online.
    // May return nullptr if no partitions are available.
    //
    // orderIndex - The size order of the partition relative to the
    //              _smallestSizeOrder.
    //
    inline Partition *newOrderPartition(int orderIndex) {
        assert(0 <= orderIndex &&
               orderIndex < MAX_QUANTUM_ALLOCATOR_ORDERS &&
               "order index out of range");
        int partitionIndex = allocatePartition();

        if (partitionIndex == NOT_FOUND) {
            return nullptr;
        }

        uint64_t size = orderToSize(orderIndex + smallestSizeOrder());
        Partition *partition = newPartition(partitionIndex, size);
        addToOrder(orderIndex, partition, partitionIndex);

        return partition;
    }

    //
    // offlinePartition takes a partition out of rotation.
    //
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    // orderIndex - The size order of the partition relative to the
    //              _smallestSizeOrder.
    //
    inline bool offlinePartition(int partitionIndex, int orderIndex) {
        assert(0 <= partitionIndex && partitionIndex < _partitionCount &&
               "partition out of range");
        assert(0 <= orderIndex &&
               orderIndex < MAX_QUANTUM_ALLOCATOR_ORDERS &&
               "order index out of range");
        Registry *orderRegistry = _orderRegistry + orderIndex;
        bool cleared = orderRegistry->clear(partitionIndex);
        _roster->setAllocator(this, smallestSizeOrder() + orderIndex);

        return cleared;
    }

    //
    // onlinePartition puts a partition into rotation.
    //
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    // orderIndex - The size order of the partition relative to the
    //              _smallestSizeOrder.
    //
    inline void onlinePartition(int partitionIndex, int orderIndex) {
        assert(0 <= partitionIndex && partitionIndex < _partitionCount &&
               "partition out of range");
        assert(0 <= orderIndex &&
               orderIndex < MAX_QUANTUM_ALLOCATOR_ORDERS &&
               "order index out of range");
        Registry *orderRegistry = _orderRegistry + orderIndex;
        orderRegistry->set(partitionIndex);
    }

    //
    // freeUpPartition scans through partitions looking for an empty partition
    // then takes it offline and reestablishes with a new size order. Returns
    // the partition or nullptr is not found.
    //
    // orderIndex - The size order of the partition relative to the
    //              _smallestSizeOrder.
    //
    inline Partition *freeUpPartition(int orderIndex) {
        //
        // TODO - Use virtual partition instead (higher order partition
        // overlayed over sparse lower order partition.
        //
        assert(0 <= orderIndex &&
               orderIndex < MAX_QUANTUM_ALLOCATOR_ORDERS &&
               "order index out of range");
        for (int partitionIndex = _partitionCount - 1;
             0 <= partitionIndex;
             partitionIndex--
        )
        {
            Partition *partition = getPartition(partitionIndex);

            //
            // Is the partition provisionally empty.
            //
            if (!partition->isEmpty()) {
                continue;
            }

            //
            // Take partition offline and then test for absolute emptiness.
            //
            if (!offlinePartition(partitionIndex, orderIndex) ||
                !partition->isEmpty()) {
                //
                // If can't take offline or not empty put online again.
                // No-op if already online.
                //
                onlinePartition(partitionIndex, orderIndex);

                continue;
            }

            //
            // Put partition online with new size.
            //
            uint64_t size = orderToSize(orderIndex + smallestSizeOrder());
            partition = newPartition(partitionIndex, size);
            addToOrder(orderIndex, partition, partitionIndex);

            return partition;
        }

        return nullptr;
    }

    //
    // partitionBase returns the base address of the "partitionIndex"th
    // partition.
    //
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    //
    inline void *partitionBase(int partitionIndex) const {
        assert(0 <= partitionIndex && partitionIndex < _partitionCount &&
               "partition out of range");
        Address address(base(), orderMul(partitionIndex, _partitionSizeOrder));

        return address;
    }

    //
    // allocatePartition finds a free partition and commits its memory. Returns
    // the partition index or NOT_FOUND.
    //
    inline int allocatePartition() {
        int partitionIndex = (int)_partitionRegistry.findFree();

        if (partitionIndex != NOT_FOUND) {
            //
            // TODO - Switch to committing pages at allocate for allocations
            // larger than PAGE_SIZE.
            //
            if (!_sharing) {
                System::commit(partitionBase(partitionIndex), _partitionSize);
            }
        }

        return partitionIndex;
    }

    //
    // freePartition frees the partition in the partition registry.
    //
    // partitionIndex - int between 0 and _partitionCount corresponding to the
    //                  partition's position in memory and its matching
    //                  allocator.
    //
    inline void freePartition(int partitionIndex) {
        assert(0 <= partitionIndex && partitionIndex < _partitionCount &&
               "partition out of range");
        _partitionRegistry.clear(partitionIndex);
    }

    //
    // partitionFromAddress returns the partition allocator from an arbitrary
    // address in the partition.
    //
    // address - arbitrary address in this quantum allocator's space.
    //
    inline Partition *partitionFromAddress(Address address) {
        assert(address.isNotNull() &&
               "address should not be null");
        assert(in(address) &&
               "address not in range for allocator");
        int partitionIndex = getPartitionIndex(address);

        return getPartition(partitionIndex);
    }

    //
    // getFreePartition attempts to create a new partition allocator. If it can
    // not then it tries to free up an existing partition allocator. Returns
    // partition allocator if successful otherwise returns nullptr.
    //
    // orderIndex - The size order of the partition relative to the
    //              _smallestSizeOrder.
    //
    inline Partition *getFreePartition(int orderIndex) {
        assert(0 <= orderIndex &&
               orderIndex < MAX_QUANTUM_ALLOCATOR_ORDERS &&
               "order index out of range");
        Partition *partition = newOrderPartition(orderIndex);

        return partition ? partition : freeUpPartition(orderIndex);
    }

    //
    // The PartitionIterator class is used to iterate through partitions
    //
    class PartitionIterator : public NoAllocate {
    private:
        //
        // Managing quantum allocator.
        //
        QuantumAllocator *_quantumAllocator;

        //
        // Size order relative to _smallestSizeOrder.
        //
        int _orderIndex;

        //
        // Underlying registry iterator.
        //
        RegistryIsSetIterator _registryIterator;

        //
        // true if should allocate new partition allocator if exhausts registry.
        //
        bool _allocateNew;

        //
        // true if should continuously allocate new partition allocator if
        // exhausts registry.
        //
        bool _continuous;

    public:
        //
        // Constructor.
        //
        // quantumAllocator - Managing quantum allocator.
        // size - Size in bytes of allocation.
        // allocateNew - true if should allocate new partition allocator
        //               if exhausts registry.
        // continuous - true if should continuously allocate new partition
        //              allocator if exhausts registry.
        //
        PartitionIterator(
            QuantumAllocator *quantumAllocator,
            uint64_t size,
            bool allocateNew = false,
            bool continuous = false
        ) :
            _quantumAllocator(quantumAllocator),
            _orderIndex(quantumAllocator->getOrderIndex(size)),
            _registryIterator(quantumAllocator->_orderRegistry + _orderIndex),
            _allocateNew(allocateNew),
            _continuous(continuous)
        {
            assert(quantumAllocator != nullptr &&
                   "quantumAllocator should not be null");
            assert((!continuous || (continuous && allocateNew)) &&
                   "continuous only if allocateNew");
        }

        //
        // next returns next online partition allocator or nullptr if none
        // found.
        //
        inline Partition *next() {
            int partitionIndex = _registryIterator.nextSet();

            if (partitionIndex != NOT_FOUND) {
                return _quantumAllocator->getPartition(partitionIndex);
            }

            if (_allocateNew) {
                if (!_continuous) {
                    _allocateNew = false;
                }

                return _quantumAllocator->getFreePartition(_orderIndex);
            }

            return nullptr;
        }
    };

public:
    //
    // Base constructor.
    //
    // sharing - true if allocations are shared.
    // roster - allocation roster from managing Director.
    // smallestSizeOrder - size order of the smallest quantum handled by this
    //                     allocator.
    // largestSizeOrder - size order of the largest quantum handled by this
    //                     allocator.
    // partitionSizeOrder - order of the partition size handled by this
    //                     allocator.
    // partitionCount - number of partitions managed by this allocator.
    // base - Lower bounds of managed space.
    // partitions - space for allocating partition allocators.
    // sideDataSize - size of a quantum side data,
    // sideData - space for side data.
    //
    //
    QuantumAllocator(
        bool sharing,
        AllocatorRoster *roster,
        int smallestSizeOrder,
        int largestSizeOrder,
        int partitionSizeOrder,
        int partitionCount,
        void *base,
        Partition *partitions,
        int sideDataSize,
        char *sideData
    ) :
        Allocator(
            base,
            orderToSize(partitionSizeOrder) * partitionCount,
            QuantumAllocatorID,
            smallestSizeOrder,
            largestSizeOrder
        ),
        _sharing(sharing),
        _roster(roster),
        _partitionSizeOrder(partitionSizeOrder),
        _partitionCount(partitionCount),
        _partitionSize(orderToSize(partitionSizeOrder)),
        _smallestSizeOrder(smallestSizeOrder),
        _largestSizeOrder(largestSizeOrder),
        _smallestSize(orderToSize(smallestSizeOrder)),
        _largestSize(orderToSize(largestSizeOrder)),
        _partitions(partitions),
        _partitionRegistry(partitionCount),
        _sideDataSize(sideDataSize),
        _sideData(sideData)
    {
        //
        // Initialize order registries.
        //
        for (int i = 0; i < MAX_QUANTUM_ALLOCATOR_ORDERS; i++) {
            new(_orderRegistry + i) Registry(_partitionCount);
        }
    }

    //
    // getPartitionIndex returns the partition index from an arbitrary address
    // in the partition.
    //
    // address - arbitrary address in this quantum allocator's space.
    //
    inline int getPartitionIndex(Address address) const {
        assert(address.isNotNull() &&
               "address should not be null");
        assert(in(address) &&
               "address not in allocator");

        return address.getIndex(base(), _partitionSizeOrder);
    }

    //
    // allocate returns the address of a memory block at least "size" bytes
    // long. The block may be larger due to rounding up to power of two.
    // allocate may return nullptr if the required memory is not available.
    //
    // size - Number of bytes to allocate.
    //
    inline void *allocate(uint64_t size) {
        assert(((_smallestSize <= size && size <= _largestSize) ||
                (size <= _smallestSize &&
                 _smallestSizeOrder == SMALLEST_SIZE_ORDER)) &&
               "size must be appropriate for allocator");

        PartitionIterator iterator(this, size, true, true);
        for (Partition *partition = iterator.next();
             partition;
             partition = iterator.next()) {
            void *address =
                partition->allocate(size);

            if (address) {
                return address;
            }
        }

        return nullptr;
    }

    //
    // deallocate makes the memory block pointed to by "address" available
    // for further allocation. If the "address" is the nullptr or outside the
    // range of the allocator the deallocate does nothing.
    //
    // address - address of memory block to deallocate.
    //
    inline void deallocate(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        Partition *partition = partitionFromAddress(address);

        partition->deallocate(address);
    }

    //
    // This form of allocate allocates "count" blocks, each at least "size"
    // bytes long. Each block may be larger due to rounding up to power of two.
    // allocate may return nullptr if the required memory is not available.
    // Use of this form of allocate contractually requires you use the
    // corresponding form of deallocate(address, size, count).
    //
    // size - number of bytes to allocate per block.
    // count - number of blocks to allocate.
    //
    inline void *allocateCount(uint64_t size, int count) {
        assert(((_smallestSize <= size && size <= _largestSize) ||
                (size <= _smallestSize &&
                 _smallestSizeOrder == SMALLEST_SIZE_ORDER)) &&
               "size must be appropriate for allocator");
        assert(0 <= count &&
               "count should be positive");

        if (orderDiv(_partitionSize, sizeToOrder(size)) < count) {
            return nullptr;
        }

        PartitionIterator iterator(this, size, true, true);
        for (Partition *partition = iterator.next();
             partition;
             partition = iterator.next()) {
            void *address =
                partition->allocateCount(size, count);

            if (address) {
                return address;
            }
        }

        return nullptr;
    }

    //
    // This form of deallocate makes the blocks of memory pointed to by
    // "address" available to further allocation. The memory should have been
    // allocated using the allocate(size, count) function.  If the "address" is
    // the nullptr or outside the range of the allocator the deallocate does
    // nothing.
    //
    // address - address of memory blocks to deallocate.
    // secure - true if shoud be zeroed.
    // size - number of bytes to deallocate per block.
    // count - number of blocks to deallocate.
    //
    inline void deallocateCount(
        void *address,
        bool secure,
        uint64_t size,
        int count
    ) {
        assert(((_smallestSize <= size && size <= _largestSize) ||
                (size <= _smallestSize &&
                 _smallestSizeOrder == SMALLEST_SIZE_ORDER)) &&
               "size must be appropriate for allocator");
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        assert(0 <= count &&
               "count should be positive");
        Partition *partition = partitionFromAddress(address);

        partition->deallocateCount(address, secure, size, count);
    }

    //
    // allocateBulk allocates addresses in bulk and puts them in the "addresses"
    // buffer. Returns the number addresses actually allocated (may be zero.)
    //
    // size - Size of blocks in bytes. Should be power of two.
    // count - length of addresses buffer.
    // addresses - address buffer.
    // contiguous - true if blocks should be contiguous (faster but wasteful.)
    //
    inline int allocateBulk(
        uint64_t size,
        int count,
        void **addresses,
        bool contiguous
    )
    {
        assert(_smallestSize <= size && size <= _largestSize &&
               "size must be appropriate for allocator");
        assert(0 <= count &&
               "count should be positive");
        assert(addresses != nullptr &&
               "addresses should not be null");

        PartitionIterator iterator(this, size, true, false);
        int allocated = 0;

        if (contiguous) {
            if (count <= orderDiv(_partitionSize, sizeToOrder(size))) {
                while (allocated < count) {
                    Partition *partition = iterator.next();

                    if (!partition) {
                        break;
                    }

                    allocated +=
                        partition->allocateBulkContiguous(count, addresses);
                }
            }
        } else {
            while (allocated < count) {
                Partition *partition = iterator.next();

                if (!partition) {
                    break;
                }

                allocated +=
                    partition->allocateBulk(count - allocated,
                                            addresses + allocated);
            }
        }

        return allocated;
    }

    //
    // deallocateBulk is a more efficient way to deallocate addresses en masse.
    // This is faster than individual calls to deallocate since it reduces the
    // number of atomic writes to the quantum registry.
    //
    // count - length of addresses buffer.
    // addresses - address buffer.
    // secure - true if shoud be zeroed.
    //
    inline int deallocateBulk(int count, void **addresses, bool secure) {
        assert(0 <= count &&
               "count should be positive");
        assert(addresses != nullptr &&
               "addresses should not be null");
        int deallocated = 0;

        while (deallocated < count) {
            void *address = addresses[deallocated];

            if (!in(address)) {
                break;
            }

            Partition *partition = partitionFromAddress(address);
            deallocated +=
                partition->deallocateBulk(count - deallocated,
                                          addresses + deallocated,
                                          secure);
        }

        return deallocated;
    }

    //
    // clear zeroes out the content of a memory block.
    //
    // address - address of memory block to clear.
    //
    inline void clear(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        Partition *partition = partitionFromAddress(address);

        partition->clear(address);
    }

    //
    // allocationSize returns number of bytes allocated at the "address".
    //
    // address - arbitrary address in an allocated memory block.
    //
    size_t allocationSize(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        int partitionIndex = getPartitionIndex(address);
        Partition *partition = _partitions + partitionIndex;

        return partition->allocationSize(address);
    }

    //
    // allocationBase returns the base address of an allocated block containing
    // the "address".
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline void *allocationBase(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");
        int partitionIndex = getPartitionIndex(address);
        Partition *partition = _partitions + partitionIndex;

        return partition->allocationBase(address);
    }

    //
    // allocationSideData returns the address of side data reserved for the
    // allocation at "address". The size of side data is a configuration
    // parameter. If the size of side data is zero then allocationSideData
    // returns nullptr.
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline void *allocationSideData(void *address) {
        assert(address != nullptr &&
               "address should not be null");
        assert(in(address) &&
               "address should be in this partition");

        int partitionIndex = getPartitionIndex(address);
        Partition *partition = _partitions + partitionIndex;

        return partition->allocationSideData(address);
    }

    //
    // nextAllocation can be used to "walk" through all the allocations
    // managed by QBA. The first call should have an "address" of nullptr with
    // successive calls using the result of the previous call. The result
    // itself can not be used for memory access since the result may have been
    // deallocated after fetching (potential seg fault). The result can however
    // be used to make calls to allocationSize or allocationSideData.
    //
    // address - nullptr or result of last call to nextAllocation.
    //
    inline void *nextAllocation(void *address) {
        int index = address && in(address) ? getPartitionIndex(address) : 0;

        while (index < _partitionCount) {
            if (_partitionRegistry.isSet(index)) {
                Partition *partition = getPartition(index);
                void *next =
                    partition->nextAllocation(address);

                if (next) {
                    return next;
                }
            }

            index++;
            address = nullptr;
        }

        return nullptr;
    }

    //
    // stats fills in "counts" and "sizes" buffers with information known to
    // this allocator. Slots 1 to MAX_ALLOCATION_ORDER contain counts and
    // sizes of allocations of that size order.
    //
    // Slot 0 - Sum of all other slots.
    // Slot 1 - Maximums of administrative data (not necessarily active.)
    // Slot 2 - Unused.
    // Slot 3-52 - Totals for blocks sized 2^slot.
    // Slot 53 and above - Unused.
    //
    // counts - counts buffer.
    // sizes - sizes buffer.
    //
    inline void stats(uint64_t *counts, uint64_t *sizes) {
        assert(counts != nullptr &&
               "counts should not be null");
        assert(sizes != nullptr &&
               "sizes should not be null");

        sizes[1] += sizeof(QuantumAllocator) +
                    _partitionCount * sizeof(Partition);

        for (int i = 0; i < _partitionCount; i++) {
            if (_partitionRegistry.isSet(i)) {
                Partition *partition = getPartition(i);
                partition->stats(counts, sizes);
            }
        }
    }
};

//----------------------------------------------------------------------------//
//
// The Slab class represents allocations that are very large and unlikely to be
// recycled.
//
class Slab : public Space {
private:
    //
    // Side data specific to the slab allocation.
    //
   char *_sideData;

public:
    //
    // Constructor used when creating a specific slab.
    //
    Slab(void *base, uint64_t size) :
        Space(base, size)
    {
    }
};

//----------------------------------------------------------------------------//
//
// The SlabAllocator class is an allocator for allocating large one-up blocks
// that are unlikely to be recycled.
//
class SlabAllocator : public Allocator {
private:
    //
    // Slab alignment.
    //
    const uint64_t SLAB_ALIGNMENT = orderToSize(LARGEST_SIZE_ORDER);

    //
    // true if allocations are to be secure (zeroed.)
    //
    bool _secure;

    //
    // Maximum number of slabs in _slabs.
    //
    int _maxCount;

    //
    // Array of allocated slabs.
    //
    Slab *_slabs;

    //
    // Size of side data per slab.
    //
    int _sideDataSize;

    //
    // Slab side data.
    //
    char *_sideData;

    //
    // Registry for allocated slabs.
    //
    Registry _registry;

    //
    // Find slab containing the address. Return the index in _slabs or
    // NOT_FOUND if not found.
    //
    // address  - Address in a slab allocation.
    //
    inline int find(void *address) const {
        for (int i = 0; i < _maxCount; i++) {
            if (_slabs[i].in(address) && _registry.isSet(i)) {
                return i;
            }
        }

        return NOT_FOUND;
    }

    //
    // record registers an allocation with the allocator.
    //
    // base - Allocation adddress.
    // size - Size of allocation.
    //
    inline int record(void *base, uint64_t size) {
        assert(isValidAddress(base) &&
               "base address is invalid");
        assert(SLAB_ALIGNMENT < size &&
               size <= MAX_ALLOCATION_SIZE &&
               "size is not valid for this allocator");
        int index = _registry.findFree();

        if (index == NOT_FOUND) {
            return NOT_FOUND;
        }

        new(_slabs + index) Slab(base, size);

        return index;
    }

    //
    // erase unregisters an allocation from the allocator.
    //
    // index - Index in the allocation table.
    //
    inline void erase(int index) {
        assert(0 <= index && index < _maxCount &&
               "slab index out of range");
        _registry.free(index);
    }

    //
    // reserve attempts to recycle a previously freed allocation. If not
    // then allocate new space.
    //
    // size - number of bytes requires rounded to page size.
    //
    inline void *reserve(uint64_t size) {
        //
        // Look for a free slab.
        //
        int index = _registry.findFree();

        //
        // If no slabs available.
        //
        if (index == NOT_FOUND) {
            return nullptr;
        }

        //
        // Extract slab data.
        //
        Slab *slab = _slabs + index;
        void *base =  slab->base();
        uint64_t slabSize = slab->size();

        //
        // If old slab is large enough.
        //
        if (slabSize > size) {
            //
            // Discard extra.
            //
            uint64_t postfixSize = slabSize - size;
            Address postfix(base, size);
            System::release(postfix, postfixSize);
        }

        //
        // If old slab is large enough.
        //
        if (slabSize >= size) {
            //
            // Clear it and return base address.
            //
            if (_secure) {
                System::commit(base, size);
            }

            new(slab) Slab(base, size);

            return base;
        }

        //
        // If old slab is not empty.
        //
        if (slabSize) {
            //
            // Discard old slab.
            //
            System::release(base, slabSize);
        }

        //
        // Allocate new slab.
        //
        base = System::reserveAligned(size, SLAB_ALIGNMENT);

        //
        // If not allocated.
        //
        if (!base) {
            //
            // Clear registry entry.
            //
            _registry.free(index);

            return nullptr;
        }

        //
        // Commit to using the new slab and return result.
        //
        System::commit(base, size);
        new(slab) Slab(base, size);

        return base;
    }

public:
    //
    // Constructor.
    //
    SlabAllocator(
        bool secure,
        int maxCount,
        Slab *slabs,
        int sideDataSize,
        char *sideData
    ) :
        Allocator(
            nullptr,
            ALL_ONES,
            SlabAllocatorID,
            LARGEST_SIZE_ORDER + 1,
            MAX_ALLOCATION_ORDER
        ),
        _secure(secure),
        _maxCount(maxCount),
        _slabs(slabs),
        _sideDataSize(sideDataSize),
        _sideData(sideData),
        _registry(maxCount)
    {
    }

    //
    // release any outstanding slabs.
    //
    inline void release() {
        for (int i = 0; i < _maxCount; i++) {
            Slab *allocation = _slabs + i;

            if (allocation->size() != ZERO) {
                System::release(allocation->base(), allocation->size());
            }
        }
    }

    //
    // allocate returns the address of a memory block at least "size" bytes
    // long. The block may be larger due to rounding up to one megabyte.
    // allocate may return nullptr if the required memory is not available.
    //
    // size - Number of bytes to allocate.
    //
    inline void *allocate(uint64_t size) {
        assert(orderToSize(LARGEST_SIZE_ORDER) < size &&
               size <= MAX_ALLOCATION_SIZE &&
               "size is not valid for this allocator");

        return reserve(roundUp(size, M));
    }

    //
    // deallocate makes the memory block pointed to by "address" available
    // for further allocation. If the "address" is the nullptr or outside the
    // range of the allocator the deallocate does nothing.
    //
    // address - Address of memory block to deallocate.
    //
    inline void deallocate(void *address) {
        int index = find(address);

        if (index == NOT_FOUND) {
            return;
        }

        //
        // Clear from registry but allow for recycling.
        //
        erase(index);
    }

    //
    // clear zeroes out the content of a memory block.
    //
    // address - address of memory block to clear.
    //
    inline void clear(void *address) {
        int index = find(address);

        if (index == NOT_FOUND) {
            return;
        }

        Slab *allocation = _slabs + index;

        System::clear(allocation->base(), allocation->size(), false);
    }

    //
    // This form of allocate allocates "count" blocks, each at least "size"
    // bytes long. Each block may be larger due to rounding up to one magabyte.
    // allocate may return nullptr if the required memory is not available.
    // Use of this form of allocate contractually requires you use the
    // corresponding form of deallocate(address, size, count).
    //
    // size - Number of bytes to allocate per block.
    // count - Number of blocks to allocate.
    //
    inline void *allocateCount(uint64_t size, int count) {
        assert(SLAB_ALIGNMENT < size &&
               size <= MAX_ALLOCATION_SIZE &&
               "size is not valid for this allocator");

        return reserve(roundUp(size * count, SLAB_ALIGNMENT));
    }

    //
    // This form of deallocate makes the blocks of memory pointed to by
    // "address" available to further allocation. The memory should have been
    // allocated using the allocate(size, count) function.  If the "address" is
    // the nullptr or outside the range of the allocator the deallocate does
    // nothing.
    //
    // address - Address of memory blocks to deallocate.
    // size - Number of bytes to deallocate per block.
    // count - Number of blocks to deallocate.
    //
    inline void deallocateCount(void *address, uint64_t size, int count) {
        assert(SLAB_ALIGNMENT < size &&
               size <= MAX_ALLOCATION_SIZE &&
               "size is not valid for this allocator");
        assert(0 < count &&
               "count is out of range");
        int index = find(address);

        if (index == NOT_FOUND) {
            return;
        }

        Slab *allocation = _slabs + index;
        System::release(allocation->base(), allocation->size());
        erase(index);
    }

    //
    // allocateBulk allocates addresses in bulk and puts them in the "addresses"
    // buffer. Returns the number addresses actually allocated (may be zero.)
    //
    // size - Size of blocks in bytes. Should be power of two.
    // count - Length of addresses buffer.
    // addresses - Address buffer.
    // contiguous - true if blocks should be contiguous (faster but wasteful.)
    //
    inline int allocateBulk(
        uint64_t size,
        int count,
        void **addresses,
        bool contiguous
    )
    {
        assert(SLAB_ALIGNMENT < size &&
               size <= MAX_ALLOCATION_SIZE &&
               "size is not valid for this allocator");
        assert(0 < count &&
               "count is out of range");
        assert(addresses != nullptr &&
               "addresses should not be null");
        uint64_t roundedSize = roundUp(size, SLAB_ALIGNMENT);
        uint64_t total = roundedSize * count;
        void *base = System::reserveAligned(total, SLAB_ALIGNMENT);
        System::commit(base, total);

        if (!base) {
            return 0;
        }

        for (int i = 0; i < count; i++) {
            Address address(base, i * roundedSize);
            int index = record(address, roundedSize);
            assert(index != NOT_FOUND &&
                   "too many slab allocations");

            if (index == NOT_FOUND) {
                System::release(address, (count - i) * roundedSize);

                return i;
            }

            addresses[i] = address;
        }

        return count;
    }

    //
    // deallocateBulk is a more efficient way to deallocate addresses en masse.
    // This is faster than individual calls to deallocate since it reduces the
    // number of atomic writes to the quantum registry.
    //
    // count - Length of addresses buffer.
    // addresses - Address buffer.
    //
    inline int deallocateBulk(int count, void **addresses) {
        assert(0 < count &&
               "count is out of range");
        assert(addresses != nullptr &&
               "addresses should not be null");
        for (int i = 0; i < count; i++) {
            void *address = addresses[i];

            if (!address) {
                return i;
            }

            SlabAllocator::deallocate(address);
        }

        return count;
    }

    //
    // allocationSize returns number of bytes allocated at the "address".
    //
    // address - Arbitrary address in an allocated memory block.
    //
    inline size_t allocationSize(void *address) {
        int index = find(address);

        if (index == NOT_FOUND) {
            return ZERO;
        }

        Slab *allocation = _slabs + index;

        return allocation->size();
    }

    //
    // allocationBase returns the base address of an allocated block containing
    // the "address".
    //
    // address - Arbitrary address in an allocated memory block.
    //
    inline void *allocationBase(void *address) {
        int index = find(address);

        if (index == NOT_FOUND) {
            return nullptr;
        }

        Slab *allocation = _slabs + index;

        return allocation->base();
    }

    //
    // allocationSideData returns the address of side data reserved for the
    // allocation at "address". The size of side data is a configuration
    // parameter. If the size of side data is zero then allocationSideData
    // returns nullptr.
    //
    // address - Arbitrary address in an allocated memory block.
    //
    inline void *allocationSideData(void *address) {
        int index = find(address);

        if (index == NOT_FOUND) {
            return nullptr;
        }

        return _sideData + index * _sideDataSize;
    }

    //
    // nextAllocation can be used to "walk" through all the allocations
    // managed by QBA. The first call should have an "address" of nullptr with
    // successive calls using the result of the previous call. The result
    // itself can not be used for memory access since the result may have been
    // deallocated after fetching (potential seg fault). The result can however
    // be used to make calls to allocationSize or allocationSideData.
    //
    // address - nullptr or result of last call to nextAllocation.
    //
    inline void *nextAllocation(void *address) {
        int index = address ? find(address) : -1;

        if (address && index == NOT_FOUND) {
            return nullptr;
        }

        RegistryIsSetIterator iterator(&_registry, index + 1);

        index = iterator.nextSet();

        return index != NOT_FOUND ? _slabs[index].base() : nullptr;
    }

    //
    // stats fills in "counts" and "sizes" buffers with information known to
    // this allocator. Slots 1 to MAX_ALLOCATION_ORDER contain counts and
    // sizes of allocations of that size order.
    //
    // Slot 0 - Sum of all other slots.
    // Slot 1 - Maximums of administrative data (not necessarily active.)
    // Slot 2 - Unused.
    // Slot 3-52 - Totals for blocks sized 2^slot.
    // Slot 53 and above - Unused.
    //
    // counts - Counts buffer.
    // sizes - Sizes buffer.
    //
    inline void stats(uint64_t *counts, uint64_t *sizes) {
        assert(counts != nullptr &&
               "counts should not be null");
        assert(sizes != nullptr &&
               "sizes should not be null");

        sizes[1] += sizeof(SlabAllocator) +
                    _maxCount * sizeof(Slab);

        for (int i = 0; i < _maxCount; i++) {
            if (_registry.isSet(i)) {
                uint64_t size = _slabs[i].size();
                int order = sizeToOrder(size);
                counts[order]++;
                sizes[order] += size;
            }
        }
    }
};

//----------------------------------------------------------------------------//
//
// The FitSize class is used to calculate the number of downsized blocks to
// reduce the average interior fragmentation.
//
// Degree 1 = 25% average fragmentation
//        2 = 12.5%
//        3 = 6.25%
//        4 = 3.125%
//
// Ex. If the allocation size is 48 bytes then the default allocate will return
// one block of size 64 (rounded up to power of two) with fragmentation of
// ((64 - 48)/64 * 100)% = 25%.
//
// Allocating 3 blocks of size 16 would yield 0% fragmentation.
//
class FitSize : public NoAllocate {
private:
    //
    // Order of raw size.
    //
    int _order;

    //
    // Downsize to allocate.
    //
    uint64_t _size;

    //
    // Number of downsized blocks to allocate.
    //
    int _count;

public:
    //
    // Constructor.
    //
    FitSize(uint64_t size, int degree) :
        _order(sizeToOrder(size)),
        _size(orderToSize(_order)),
        _count(1)
    {
        //
        // Determine lowest order to use.
        //
        int lowOrder = _order - degree;

        //
        // Clip to smallest allocation order.
        //
        if (lowOrder < SMALLEST_SIZE_ORDER) {
            degree = _order - SMALLEST_SIZE_ORDER + 1;
            lowOrder = SMALLEST_SIZE_ORDER;
        }

        //
        // Round up to lowest order.
        //
        uint64_t roundedSize = size + orderToSize(lowOrder) - 1;

        //
        // Rescale size to lowest order.
        //
        uint64_t scaledSize = roundedSize >> lowOrder;

        //
        // Count trailong zeroes.
        //
        int zeroes = ctz(scaledSize);

        //
        // Number of quantum needed.
        //
        int count = orderDiv(scaledSize, zeroes);

        //
        // Do nothing if count is one.
        //
        if (1 < count) {
            //
            // Minimumal quantum size to use.
            //
            _size = orderToSize(lowOrder + zeroes);

            //
            // Number of quantum.
            //
            _count = count;
        }
    }

    //
    // size returns the size of blocks to allocate.
    //
    inline uint64_t size() const {
        return _size;
    }

    //
    // count returns the number of blocks to allocate.
    //
    inline int count() const {
        return _count;
    }
};

//----------------------------------------------------------------------------//
//
// The Director class is responsible for delegating requests to the appropriate
// allocator.
//
class Director : public Space {
private:
    //
    // true if sharing allocations.
    //
    const bool _sharing;

    //
    // If allocations should be may secure by zeroing on deallocation.
    //
    const bool _secure;

    //
    // Roster used to map size order to appropriate allocator.
    //
    AllocatorRoster *_roster;

    //
    // Quantum allocators used to allocate blocks less trhan 64M.
    //
    QuantumAllocator **_quantumAllocators;

    //
    // Slab allocator used to allocate blocks larger than 64M.
    //
    SlabAllocator *_slabAllocator;

    //
    // Null allocator used to no-op requests.
    //
    NullAllocator _nullAllocator;

    //
    // User reference.
    //
    std::atomic<void *> _reference;

    //
    // Shared link name.
    //
    char _linkName[MAX_LINK_NAME];

    //
    // Constructor.
    //
    Director(
        void *base,
        uint64_t size,
        bool sharing,
        bool secure,
        AllocatorRoster *roster,
        QuantumAllocator **quantumAllocators,
        SlabAllocator *slabAllocator,
        const char *linkName
    ) :
        Space(base, size),
        _sharing(sharing),
        _secure(secure),
        _roster(roster),
        _quantumAllocators(quantumAllocators),
        _slabAllocator(slabAllocator),
        _nullAllocator(),
        _reference(nullptr),
        _linkName()
    {

        //
        // Fill out the roster.
        //
        _roster->setAllocators(&_nullAllocator,
                               0,
                               SMALLEST_SIZE_ORDER);
        _roster->setAllocators(_quantumAllocators[0],
                               1,
                               SMALLEST_SIZE_ORDER + 1);

        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            Allocator *allocator = _quantumAllocators[i];
            _roster->setAllocators(allocator,
                                   allocator->smallestSizeOrder(),
                                   allocator->largestSizeOrder() + 1);
        }

        _roster->setAllocators(_slabAllocator,
                               LARGEST_SIZE_ORDER + 1,
                               MAX_ALLOCATION_ORDER);
        _roster->setAllocators(&_nullAllocator,
                               MAX_ALLOCATION_ORDER,
                               MAX_ORDER);

        std::strncpy(_linkName, linkName ? linkName : "", MAX_LINK_NAME);
        _linkName[MAX_LINK_NAME - 1] = '\0';
    }

    //
    // createDirector sizes or creates an instance of Director.
    //
    // arena - is used to calculate size or allocate space for internal
    //         structures.
    // creating - true if actually creating internal structures.
    // sharing - true if sharing allocations.
    // secure - true if allocations are to be secure (zeroed.)
    // smallPartitionCount - partition count for small sized allocations.
    // mediumPartitionCount - partition count for small sized allocations.
    // largePartitionCount - partition count for small sized allocations.
    // maxSlabCount - maximum number of slabs.
    // sideDataSize - number of bytes reserved for side data.
    // linkName - shared link name.
    //
    inline static Director *createDirector(
        Arena &arena,
        bool creating,
        bool sharing,
        bool secure,
        int *partitionCounts,
        int maxSlabCount,
        int sideDataSize,
        const char *linkName
    ) {
        //
        // Base addresses, sizes and allocators for quantum regions.
        //
        void *bases[MAX_QUANTUM_ALLOCATORS];
        uint64_t sizes[MAX_QUANTUM_ALLOCATORS];

        //
        // Smallest size order for the last quantum allocator.
        //
        int smallestSizeOrder = LARGEST_SIZE_ORDER -
                                MAX_QUANTUM_ALLOCATOR_ORDERS + 1;

        //
        // Allocate quantum allocator regions, largest to smallest (to maintain
        // alignment.)
        //
        for (int i = MAX_QUANTUM_ALLOCATORS - 1; 0 <= i; i--) {
            //
            // Partition size and size order for current quantum allocator.
            //
            uint64_t partitionSize =
                orderMul(MAX_PARTITION_QUANTUM, smallestSizeOrder);
            int partitionSizeOrder = sizeToOrder(partitionSize);

            //
            // Save size of current quantum allocator region.
            //
            sizes[i] = orderMul(partitionCounts[i], partitionSizeOrder);

            //
            // Calculate base address of region.
            //
            bases[i] = arena.allocate(sizes[i]);

            //
            // Move on to previous quantum allocator.
            //
            smallestSizeOrder -= MAX_QUANTUM_ALLOCATOR_ORDERS;
        }

        //
        // Immediately commit rest of allocation for use for internal
        // structures.
        //

        if (creating && !sharing) {
            System::commit(arena.allocate(0), arena.size() - arena.allocated());
#if 0
            //
            // Useful for tweaking configurations.
            //
            printf("%lld\n", arena.size());
            printf("%lld\n", arena.allocated());
            printf("%lld\n", arena.size() - arena.allocated());
#endif
        }

        //
        // Allocate roster.
        //
        AllocatorRoster *roster =
            arena.allocate<AllocatorRoster>(sizeof(AllocatorRoster));

        if (creating) {
            new(roster) AllocatorRoster();
        }

        //
        // Track quantum allocators (only used if isCreate.)
        //
        QuantumAllocator **quantumAllocators =
            arena.allocate<QuantumAllocator *>
                    (MAX_QUANTUM_ALLOCATORS * sizeof(Allocator *));

        //
        // Smallest and largest size order for first quantum allocator.
        //
        smallestSizeOrder = SMALLEST_SIZE_ORDER;
        int largestSizeOrder = SMALLEST_SIZE_ORDER +
                               MAX_QUANTUM_ALLOCATOR_ORDERS - 1;

        //
        // Allocate quantum allocators.
        //
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            //
            // Partition size and size order for current quantum allocator.
            //
            uint64_t partitionSize =
                orderMul(MAX_PARTITION_QUANTUM, smallestSizeOrder);
            int partitionSizeOrder = sizeToOrder(partitionSize);

            //
            // Allocate side data for quantum allocator.
            //
            char *sideData = arena.allocate<char>(
                static_cast<uint64_t>(partitionCounts[i]) *
                static_cast<uint64_t>(sideDataSize) *
                static_cast<uint64_t>(MAX_PARTITION_QUANTUM)
            );

            //
            // Allocate partition allocator space.
            //
            Partition *partitions =
                arena.allocate<Partition>(
                    sizeof(Partition) * partitionCounts[i]
                );

            //
            // Allocate quantum allocator space.
            //
            QuantumAllocator *allocator =
                arena.allocate<QuantumAllocator>
                    (sizeof(QuantumAllocator));

            //
            // Initialize quantum allocator if not dry run.
            //
            if (creating) {
                new(allocator)
                QuantumAllocator(
                    sharing,
                    roster,
                    smallestSizeOrder,
                    largestSizeOrder,
                    partitionSizeOrder,
                    partitionCounts[i],
                    bases[i],
                    partitions,
                    sideDataSize,
                    sideData
                );

                //
                // Track allocator address.
                //
                quantumAllocators[i] = allocator;
            }

            smallestSizeOrder += MAX_QUANTUM_ALLOCATOR_ORDERS;
            largestSizeOrder += MAX_QUANTUM_ALLOCATOR_ORDERS;
        }

        //
        // Create slab allocator.
        //
        Slab *slabs = arena.allocate<Slab>(maxSlabCount * sizeof(Slab));
        char *slabSizeData = arena.allocate<char>(maxSlabCount * sideDataSize);

        SlabAllocator *slabAllocator =
            arena.allocate<SlabAllocator>(sizeof(SlabAllocator));

        if (creating) {
            new(slabAllocator)
            SlabAllocator(
                secure,
                maxSlabCount,
                slabs,
                sideDataSize,
                slabSizeData
            );
        }

        //
        // Create Director.
        //
        Director *director = arena.allocate<Director>(sizeof(Director));

        if (creating) {
            new (director)
            Director(
                arena.base(),
                arena.size(),
                sharing,
                secure,
                roster,
                quantumAllocators,
                slabAllocator,
                linkName
            );
        }

        return director;
    }

public:
    //
    // create creates a new Director based on configuration.
    //
    // address - zero or fixed based address for allocation.
    // linkName - shared link name.
    // secure - true if allocations are to be secure (zeroed.)
    // smallPartitionCount - partition count for small sized allocations.
    // mediumPartitionCount - partition count for small sized allocations.
    // largePartitionCount - partition count for small sized allocations.
    // maxSlabCount - maximum number of slabs.
    // sideDataSize - number of bytes reserved for side data.
    //
    inline static Director *create(
        uint64_t address,
        const char* linkName,
        bool secure,
        int smallPartitionCount,
        int mediumPartitionCount,
        int largePartitionCount,
        int maxSlabCount,
        int sideDataSize
    ) {
        assert((address == ZERO || isValidAddress(address)) &&
               "address is invalid");
        assert((address & MASK(orderToSize(LARGEST_SIZE_ORDER))) == ZERO &&
               "address must be a multiple of largest quantum size");
        assert(0 <= smallPartitionCount &&
               "small partition count out of range");
        assert(0 <= mediumPartitionCount &&
               "medium partition count out of range");
        assert(0 <= largePartitionCount &&
               "large partition count out of range");
        assert(0 <= maxSlabCount &&
               "slabs out of range");
        assert(0 <= sideDataSize && sideDataSize <= BYTES_PER_WORD &&
               "sideDataSize out of range");
        assert((!linkName || linkName[0] == '\0' || maxSlabCount == 0) &&
               "cannot share slabs");

        //
        // Map partition counts to allocators.
        //
        int partitionCounts[] = {
            smallPartitionCount,
            mediumPartitionCount,
            largePartitionCount
        };

        assert(sizeof(partitionCounts) / sizeof(int) ==
                    MAX_QUANTUM_ALLOCATORS &&
               "quantum allocator count not in sync");

        //
        // If is shared director request.
        //
        bool sharing = address != ZERO &&
                       linkName != nullptr &&
                       linkName[0];

        //
        // Calculate total size of allocations
        //
        Arena sizing;

        //
        // Dry run for calculating sizes.
        //
        createDirector(
            sizing,
            false,
            sharing,
            secure,
            partitionCounts,
            maxSlabCount,
            sideDataSize,
            linkName
        );

        uint64_t size = roundUp(sizing.allocated(), PAGE_SIZE);

        //
        // Default file descriptor.
        //
        int fd = -1;

        if (sharing) {
#ifdef _MSC_VER
            //
            // Not supported.
            //
            return nullptr;
#else
            //
            // Attempt to create shared link.
            //
            fd = shm_open(linkName, O_EXCL | O_CREAT | O_RDWR, 0600);

            //
            // If needs to be created.
            //
            if (fd != -1) {
                //
                // If just created then resize to the desired size.
                //
                int err = ftruncate(fd, size); (void)err;
                assert(err == 0 &&
                       "can not resize shared access");
            } else {
                //
                // Attempt to open existing shared link.
                //
                fd = shm_open(linkName, O_RDWR, 0600);

                if (fd == -1) {
                    return nullptr;
                }

                //
                // Map shared memory.
                //
                void *share = System::mapShared(size, address, fd);

                //
                // Can not map memory.
                //
                if (!share) {
                    return nullptr;
                }

                //
                // Shared mapping arena.
                //
                Arena mapping(share, size);

                //
                // Overlay existing memory.
                //
                return createDirector(
                    mapping,
                    false,
                    sharing,
                    secure,
                    partitionCounts,
                    maxSlabCount,
                    sideDataSize,
                    linkName
                );
            }
#endif
        }

        //
        // Reserve memory.
        //
        void *base = address == ZERO ?
            System::reserveAligned(size, orderToSize(LARGEST_SIZE_ORDER)) :
            System::reserve(size, address, M, fd);

        //
        // If can not allocate memory.
        //
        if (!base) {
            return nullptr;
        }

        //
        // Allocation arena.
        //
        Arena creating(base, size);

        //
        // Actually allocate structures.
        //
        return createDirector(
            creating,
            true,
            sharing,
            secure,
            partitionCounts,
            maxSlabCount,
            sideDataSize,
            linkName
        );
    }

    //
    // createSize returns the number of bytes required to create the Director.
    //
    // secure - true if allocations are to be secure (zeroed.)
    // smallPartitionCount - partition count for small sized allocations.
    // mediumPartitionCount - partition count for small sized allocations.
    // largePartitionCount - partition count for small sized allocations.
    // maxSlabCount - maximum number of slabs.
    // sideDataSize - number of bytes reserved for side data.
    //
    inline static uint64_t createSize(
        bool secure,
        int smallPartitionCount,
        int mediumPartitionCount,
        int largePartitionCount,
        int maxSlabCount,
        int sideDataSize
    ) {
        assert(0 <= smallPartitionCount &&
               "small partition count out of range");
        assert(0 <= mediumPartitionCount &&
               "medium partition count out of range");
        assert(0 <= largePartitionCount &&
               "large partition count out of range");
        assert(0 <= maxSlabCount &&
               "slabs out of range");
        assert(0 <= sideDataSize && sideDataSize <= BYTES_PER_WORD &&
               "sideDataSize out of range");

        //
        // Map partition counts to allocators.
        //
        int partitionCounts[] = {
            smallPartitionCount,
            mediumPartitionCount,
            largePartitionCount
        };

        assert(sizeof(partitionCounts) / sizeof(int) ==
                    MAX_QUANTUM_ALLOCATORS &&
               "quantum allocator count not in sync");

        //
        // Calculate total size of allocations
        //
        Arena sizing;

        //
        // Dry run for calculating sizes.
        //
        createDirector(
            sizing,
            false,
            false,
            secure,
            partitionCounts,
            maxSlabCount,
            sideDataSize,
            nullptr
        );

        return roundUp(sizing.allocated(), PAGE_SIZE);
    }

    //
    // Destructor.
    //
    static void destroy(Director *director, bool unlink) {
        director->_slabAllocator->release();

#ifndef _MSC_VER
        if (unlink && director->_linkName[0]) {
            shm_unlink(director->_linkName);
        }
#endif

        System::release(director->base(), director->size());
    }

    inline static Director *getDirector(qba_t *qba) {
        Director *director = reinterpret_cast<Director *>(qba);
        assert(director != nullptr &&
               director->in(director) &&
               "invalid director reference");

        return director;
    }

    //
    // findAllocatorBySize returns an allocator suited to allocate blocks of
    // "size" bytes.
    //
    // size - Size in bytes of block to allocate.
    //
    inline Allocator *findAllocatorBySize(uint64_t size) {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");

        return _roster->getAllocator(sizeToOrder(size));
    }

    //
    // getReference returns the current value of the user reference.
    //
    void *getReference() {
        return _reference.load();
    }

    //
    // setReference conditionally sets the value of the user reference. Returns
    // true if value was set.
    //
    // oldValue - Value expected to be the current reference value.
    // newValue - Value to set.
    //
    bool setReference(void *oldValue, void *newValue) {
        return _reference.compare_exchange_weak(oldValue, newValue);
    }

    //
    // reallocate tests the new size against the existing size to see if a
    // new block is appropriate. If so the new block is allocated, the contents
    // of the old block copied over, the old block deallocated and the new block
    // address returned. If not the old block address is returned. If the old
    // block was nullptr then a new empty block is returned. May return zero if
    // unable to allocate the new block (old block not deallocated.)
    //
    inline void *reallocate(void *oldAddress, uint64_t newSize) {
        if (!oldAddress) {
            return allocate(newSize);
        }

        uint64_t oldSize = allocationSize(oldAddress);

        if (oldSize < roundUpPowerOf2(newSize) ||
            sizeToOrder(newSize) < sizeToOrder(oldSize))
        {
            void *newAddress = allocate(newSize);

            if (newAddress && oldSize != ZERO) {
                System::copy(oldAddress, newAddress, oldSize);
                deallocate(oldAddress);
            }

            return newAddress;
        }

        return oldAddress;
    }

    //
    // allocate returns the address of a memory block at least "size" bytes
    // long. The block may be larger due to rounding up. allocate may return
    // nullptr if the required memory is not available.
    //
    // size - Number of bytes to allocate.
    //
    inline void *allocate(uint64_t size) {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");
        uint64_t alignedSize = roundUpPowerOf2(size);
        Allocator* allocator = findAllocatorBySize(alignedSize);

        if (allocator->isPartition()) {
            Partition *partition = allocator->asPartition();

            void *address = partition->allocate(alignedSize);

            if (address) {
                return address;
            }

            allocator = partition->getQuantumAllocator();
        }

        if (allocator->isQuantumAllocator()) {
            return allocator->asQuantumAllocator()->allocate(alignedSize);
        }

        if (allocator->isSlabAllocator()) {
            return allocator->asSlabAllocator()->allocate(alignedSize);
        }

        return nullptr;
    }

    //
    // deallocate makes the memory block pointed to by "address" available
    // for further allocation. If the "address" is the nullptr or outside the
    // range of the allocator the deallocate does nothing and returns false.
    //
    // address - address of memory block to deallocate.
    //
    inline void deallocate(void *address) {
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (allocator->in(address)) {
                if (_secure) {
                   allocator->clear(address);
                }

                allocator->deallocate(address);

                return;
            }
        }

        _slabAllocator->deallocate(address);
    }

    //
    // clear zeroes out the content of a memory block.
    //
    inline void clear(void *address) {
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (allocator->in(address)) {
                allocator->clear(address);

                return;
            }
        }

        _slabAllocator->clear(address);
    }

    //
    // This form of allocate allocates "count" blocks, each at least "size"
    // bytes long. Each block may be larger due to rounding up.  allocate may
    // return nullptr if the required memory is not available.  Use of this
    // form of allocate contractually requires you use the corresponding form
    // of deallocate(address, size, count).
    //
    // size - number of bytes to allocate per block.
    // count - number of blocks to allocate.
    //
    inline void *allocateCount(uint64_t size, int count) {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");
        assert(0 < count &&
               "count is out of range");

        uint64_t alignedSize = roundUpPowerOf2(size);
        Allocator* allocator = findAllocatorBySize(alignedSize);

        if (allocator->isPartition()) {
            Partition *partition = allocator->asPartition();

            void *address = partition->allocateCount(alignedSize, count);

            if (address) {
                return address;
            }

            allocator = partition->getQuantumAllocator();
        }

        if (allocator->isQuantumAllocator()) {
            return allocator->asQuantumAllocator()->
                allocateCount(alignedSize, count);
        }

        if (allocator->isSlabAllocator()) {
            return allocator->asSlabAllocator()->
                allocateCount(alignedSize, count);
        }

        return nullptr;
    }

    //
    // This form of deallocate makes the blocks of memory pointed to by
    // "address" available to further allocation. The memory should have been
    // allocated using the allocate(size, count) function.  If the "address" is
    // the nullptr or outside the range of the allocator the deallocate does
    // nothing and returns false.
    //
    // address - address of memory blocks to deallocate.
    // size - number of bytes to deallocate per block.
    // count - number of blocks to deallocate.
    //
    inline void deallocateCount(void *address, uint64_t size, int count) {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");
        assert(0 < count &&
               "count is out of range");

        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (allocator->in(address)) {
                allocator->deallocateCount(address, _secure, size, count);

                return;
            }
        }

        _slabAllocator->deallocateCount(address, size, count);
    }

    //
    // allocateBulk allocates addresses in bulk and puts them in the "addresses"
    // buffer. Returns the number addresses actually allocated (may be zero.)
    //
    // size - Size of blocks in bytes. Should be power of two.
    // count - length of addresses buffer.
    // addresses - address buffer.
    // contiguous - true if blocks should be contiguous (faster but wasteful.)
    //
    inline int allocateBulk(
        uint64_t size,
        int count,
        void **addresses,
        bool contiguous
    )
    {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");
        assert(0 <= count &&
               "count should be positive");
        assert(addresses != nullptr &&
               "addresses should not be null");
        int order = sizeToOrder(size);

        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (order <= allocator->largestSizeOrder()) {
                return allocator->
                    allocateBulk(size, count, addresses, contiguous);
            }
        }

        return _slabAllocator->allocateBulk(size, count, addresses, contiguous);
     }

    //
    // deallocateBulk is a more efficient way to deallocate addresses en masse.
    // This is faster than individual calls to deallocate since it reduces the
    // number of atomic writes to the quantum registry.
    //
    // count - length of addresses buffer.
    // addresses - address buffer.
    //
    inline void deallocateBulk(int count, void **addresses) {
        assert(0 <= count &&
               "count should be positive");
        assert(addresses != nullptr &&
               "addresses should not be null");

        int deallocated = 0;
        while (deallocated < count) {
            void *first = addresses[deallocated];
            bool found = false;

            for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
                QuantumAllocator *allocator = _quantumAllocators[i];

                if (allocator->in(first)) {
                    deallocated += allocator->
                        deallocateBulk(count, addresses, _secure);
                    found = true;

                    break;
                }
            }

            if (!found) {
                int slabs = _slabAllocator->deallocateBulk(count, addresses);
                deallocated += slabs != 0 ? slabs : 1;
            }
        }
    }

    //
    // allocateFit attempts to minimize the internal fragmentation for an
    // an allocation (see FitSize class.) Use of this form of allocate
    // contractually requires you use the corresponding
    // deallocateFit(address, size, count).
    //
    // size - Size of block in bytes.
    // degree - Degree of fragmentation between 1 and MAX_FIT_DEGREE.
    //
    inline void *allocateFit(uint64_t size, int degree) {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");
        assert(0 < degree && degree <= MAX_FIT_DEGREE &&
               "degree is out of range");
        FitSize fitSize(size, degree);

        return allocateCount(fitSize.size(), fitSize.count());
    }

    //
    // deallocateFit deallocates a memory block allocated by invoking
    // allocateFit.
    //
    // size - Size of block in bytes.
    // degree - Degree of fragmentation between 1 and MAX_FIT_DEGREE.
    //
    inline void deallocateFit(void *address, uint64_t size, int degree) {
        assert(size <= MAX_ALLOCATION_SIZE &&
               "size out of range");
        assert(0 < degree && degree <= MAX_FIT_DEGREE &&
               "degree is out of range");
        FitSize fitSize(size, degree);

        deallocateCount(address, fitSize.size(), fitSize.count());
    }

    //
    // allocationSize returns number of bytes allocated at the "address".
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline size_t allocationSize(void *address) {
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (allocator->in(address)) {
                return allocator->allocationSize(address);
            }
        }

        return _slabAllocator->allocationSize(address);
    }

    //
    // allocationBase returns the base address of an allocated block containing
    // the "address".
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline void *allocationBase(void *address) {
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (allocator->in(address)) {
                return allocator->allocationBase(address);
            }
        }

        return _slabAllocator->allocationBase(address);
    }

    //
    // allocationSideData returns the address of side data reserved for the
    // allocation at "address". The size of side data is a configuration
    // parameter. If the size of side data is zero then allocationSideData
    // returns nullptr.
    //
    // address - arbitrary address in an allocated memory block.
    //
    inline void *allocationSideData(void *address) {
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (allocator->in(address)) {
                return allocator->allocationSideData(address);
            }
        }

        return _slabAllocator->allocationSideData(address);
    }

    //
    // nextAllocation can be used to "walk" through all the allocations
    // managed by QBA. The first call should have an "address" of nullptr with
    // successive calls using the result of the previous call. The result
    // itself can not be used for memory access since the result may have been
    // deallocated after fetching (potential seg fault). The result can however
    // be used to make calls to allocationSize or allocationSideData. A result
    // of zero indicates no further blocks.
    //
    // address - nullptr or result of last call to nextAllocation.
    //
    inline void *nextAllocation(void *address) {
        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            QuantumAllocator *allocator = _quantumAllocators[i];

            if (!address || allocator->in(address)) {
                void *next = allocator->nextAllocation(address);

                if (next) {
                    return next;
                }

                address = nullptr;
            }
        }

        return _slabAllocator->nextAllocation(address);
    }

    //
    // stats fills in "counts" and "sizes" buffers with information known to
    // this allocator. Slots 1 to MAX_ALLOCATION_ORDER contain counts and
    // sizes of allocations of that size order.
    //
    // Slot 0 - Sum of all other slots.
    // Slot 1 - Maximums of administrative data (not necessarily active.)
    // Slot 2 - Unused.
    // Slot 3-52 - Totals for blocks sized 2^slot.
    // Slot 53 and above - Unused.
    //
    // counts - counts buffer.
    // sizes - sizes buffer.
    //
    inline void stats(uint64_t *counts, uint64_t *sizes) {
        assert(counts != nullptr &&
               "counts should not be null");
        assert(sizes != nullptr &&
               "sizes should not be null");
        memset(counts, 0, QB_STATS_SIZE * sizeof(uint64_t));
        memset(sizes, 0, QB_STATS_SIZE * sizeof(uint64_t));

        counts[1]++;
        sizes[1] += sizeof(Director);

        for (int i = 0; i < MAX_QUANTUM_ALLOCATORS; i++) {
            _quantumAllocators[i]->stats(counts, sizes);
        }

        _slabAllocator->stats(counts, sizes);

        uint64_t count = 0;
        uint64_t size = 0;

        for (int i = 1; i < QB_STATS_SIZE; i++) {
            count += counts[i];
            size += sizes[i];
        }

        counts[0] = count;
        sizes[0] = size;
    }
};

} // namespace qba

//----------------------------------------------------------------------------//

using namespace qba;

extern "C" {

//
// See qba.hpp for details.
//

int qba_version() {
    return QBA_VERSION;
}

const char *qba_version_string() {
    return QBA_VERSION_STRING;
}

qba_t *qba_create(
    intptr_t address,         // Fixed base address for allocations.
    const char* linkName,     // Shared link name.
    bool secure,              // true if allocations are zeroed.
    int smallPartitionCount,  // Partition count for small sized allocations.
    int mediumPartitionCount, // Partition count for medium sized allocations.
    int largePartitionCount,  // Partition count for large sized allocations.
    int maxSlabCount,         // Maximum number of slabs.
    int sideDataSize          // Size of size data.
) {
    Director *director = Director::create(
        address,
        linkName,
        secure,
        smallPartitionCount,
        mediumPartitionCount,
        largePartitionCount,
        maxSlabCount,
        sideDataSize
    );

    return reinterpret_cast<qba_t *>(director);
}

size_t qba_create_size(
    bool secure,              // true if allocations are zeroed.
    int smallPartitionCount,  // Partition count for small sized allocations.
    int mediumPartitionCount, // Partition count for medium sized allocations.
    int largePartitionCount,  // Partition count for large sized allocations.
    int maxSlabCount,         // Maximum number of slabs.
    int sideDataSize          // Size of size data.
) {
    return Director::createSize(
        secure,
        smallPartitionCount,
        mediumPartitionCount,
        largePartitionCount,
        maxSlabCount,
        sideDataSize
    );
}

void qba_destroy(qba_t *qba, bool unlink) {
    Director *director = Director::getDirector(qba);
    Director::destroy(director, unlink);
}

void *qba_get_reference(qba_t *qba) {
    Director *director = Director::getDirector(qba);
    return director->getReference();
}

bool qba_set_reference(qba_t *qba, void *oldValue, void *newValue) {
    Director *director = Director::getDirector(qba);
    return director->setReference(oldValue, newValue);
}

void *qba_allocate(qba_t *qba, uint64_t size) {
    Director *director = Director::getDirector(qba);

    return director->allocate(size);
}

void qba_deallocate(qba_t *qba, void *address) {
    Director *director = Director::getDirector(qba);
    director->deallocate(address);
}

void *qba_reallocate(qba_t *qba, void *oldAddress, uint64_t newSize) {
   Director *director = Director::getDirector(qba);

   return director->reallocate(oldAddress, newSize);
}

void qba_clear(qba_t *qba, void *address) {
    Director *director = Director::getDirector(qba);

    return director->clear(address);
}

size_t qba_size(qba_t *qba, void *address) {
    Director *director = Director::getDirector(qba);

    return director->allocationSize(address);
}

void *qba_base(qba_t *qba, void *address) {
    Director *director = Director::getDirector(qba);

    return director->allocationBase(address);
}

void *qba_side_data(qba_t *qba, void *address) {
    Director *director = Director::getDirector(qba);

    return director->allocationSideData(address);
}

void *qba_next(qba_t *qba, void *address) {
    Director *director = Director::getDirector(qba);

    return director->nextAllocation(address);
}

void qba_stats(qba_t *qba, uint64_t *counts, uint64_t *sizes) {
    Director *director = Director::getDirector(qba);

    return director->stats(counts, sizes);
}

int qba_allocate_bulk(
     qba_t *qba,
     uint64_t size,
     int count,
     void **addresses,
     bool contiguous
) {
    Director *director = Director::getDirector(qba);

    return director->allocateBulk(size, count, addresses, contiguous);
}

void qba_deallocate_bulk(qba_t *qba, int count, void **addresses) {
    Director *director = Director::getDirector(qba);
    director->deallocateBulk(count, addresses);
}

void *qba_allocate_count(qba_t *qba, uint64_t size, int count) {
    Director *director = Director::getDirector(qba);

    return director->allocateCount(size, count);
}

void qba_deallocate_count(qba_t *qba, void *address, uint64_t size, int count) {
    Director *director = Director::getDirector(qba);
    director->deallocateCount(address, size, count);
}

void *qba_allocate_fit(qba_t *qba, uint64_t size, int degree) {
    Director *director = Director::getDirector(qba);

    return director->allocateFit(size, degree);
}

void qba_deallocate_fit(qba_t *qba, void *address, uint64_t size, int degree) {
    Director *director = Director::getDirector(qba);
    director->deallocateFit(address, size, degree);
}

}
