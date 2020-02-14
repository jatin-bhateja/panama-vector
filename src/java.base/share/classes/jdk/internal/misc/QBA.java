/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 */

package jdk.internal.misc;

import jdk.internal.HotSpotIntrinsicCandidate;

public final class QBA {
    private final static int MAX_PARTITIONS = 16 * 1024;
    private final static int MAX_SIDE_DATA = 64;

    static {
        registerNatives();
    }

    private final long director;

    private QBA(long director) {
        this.director = director;
    }

    /*
     * Return returns the version of the QBA encoded as an integer.
     *
     * bits 31-24: Unused.
     * bits 23-16: Release number.
     * bits 15-8:  Major number.
     * bits 7-0:   Minor number.
     *
     * @return an integer an encoding of the QBA version.
     */
    public static int version() {
        return version0();
    }

    /*
     * Returns version information as an informative C string.
     *
     * @return the QBA version as a string.
     */
    public static String versionString() {
        return versionString0();
    }

    /*
     * Returns a new instance of QBA for use with other methods.
     *
     * @param address               fixed base address for allocations.
     * @param linkName              link name for sharing.
     * @param secure                true if allocations are zeroed.
     * @param smallPartitionCount   partition count for small sized allocations.
     * @param mediumPartitionCount  partition count for medium sized allocations.
     * @param largePartitionCount   partition count for large sized allocations.
     * @param maxSlabCount          maximum number of slabs.
     * @param sideDataSize          size of size data.
     * @return instance of qba or null if space is not available.
     */
    public static QBA create(long address, String linkName, boolean secure,
        int smallPartitionCount,
        int mediumPartitionCount,
        int largePartitionCount,
        int maxSlabCount,
        int sideDataSize
    ) {
        if (smallPartitionCount < 0 || MAX_PARTITIONS < smallPartitionCount) {
            throw new IllegalArgumentException(
               "small partition count out of range");
        }
        if (mediumPartitionCount < 0 || MAX_PARTITIONS < mediumPartitionCount) {
            throw new IllegalArgumentException(
               "medium partition count out of range");
        }
        if (largePartitionCount < 0 || MAX_PARTITIONS < largePartitionCount) {
            throw new IllegalArgumentException(
               "large partition count out of range");
        }
        if (maxSlabCount < 0 || MAX_PARTITIONS < maxSlabCount) {
            throw new IllegalArgumentException(
               "maximum slab count out of range");
        }
        if (sideDataSize < 0 || MAX_SIDE_DATA < sideDataSize) {
             throw new IllegalArgumentException(
               "side data size out of range");
        }
        long director = create0(address, linkName, secure,
                smallPartitionCount, mediumPartitionCount, largePartitionCount,
                maxSlabCount, sideDataSize);
        return director == 0L ? null : new QBA(director);
    }

    /*
     * Returns a new instance of QBA for use with other methods.
     *
     * @param secure                true if allocations are zeroed.
     * @param smallPartitionCount   partition count for small sized allocations.
     * @param mediumPartitionCount  partition count for medium sized allocations.
     * @param largePartitionCount   partition count for large sized allocations.
     * @param maxSlabCount          maximum number of slabs.
     * @param sideDataSize          size of size data.
     * @return instance of qba or null if space is not available.
     */
    public static QBA create(boolean secure,
        int smallPartitionCount,
        int mediumPartitionCount,
        int largePartitionCount,
        int maxSlabCount,
        int sideDataSize
    ) {
        return create(0L, null, secure,
                smallPartitionCount, mediumPartitionCount, largePartitionCount,
                maxSlabCount, sideDataSize);
    }

    /*
     * Returns a new instance of QBA for use with other methods.
     *
     * @param secure                true if allocations are zeroed.
     * @return instance of qba or null if space is not available.
     */
    public static QBA create(boolean secure) {
        return create(0L, null, secure, 32, 16, 8, 2048, 0);
    }

    /*
     * Returns the size of a new instance of QBA.
     *
     * @param secure                true if allocations are zeroed.
     * @param smallPartitionCount   partition count for small sized allocations.
     * @param mediumPartitionCount  partition count for medium sized allocations.
     * @param largePartitionCount   partition count for large sized allocations.
     * @param maxSlabCount          maximum number of slabs.
     * @param sideDataSize          size of size data.
     */
    public static long createSize(boolean secure,
       int smallPartitionCount,
       int mediumPartitionCount,
       int largePartitionCount,
       int maxSlabCount,
       int sideDataSize) {
         if (smallPartitionCount < 0 || MAX_PARTITIONS < smallPartitionCount) {
             throw new IllegalArgumentException(
                "small partition count out of range");
         }
         if (mediumPartitionCount < 0 || MAX_PARTITIONS < mediumPartitionCount) {
             throw new IllegalArgumentException(
                "medium partition count out of range");
         }
         if (largePartitionCount < 0 || MAX_PARTITIONS < largePartitionCount) {
             throw new IllegalArgumentException(
                "large partition count out of range");
         }
         if (maxSlabCount < 0 || MAX_PARTITIONS < maxSlabCount) {
             throw new IllegalArgumentException(
                "maximum slab count out of range");
         }
         if (sideDataSize < 0 || MAX_SIDE_DATA < sideDataSize) {
              throw new IllegalArgumentException(
                "side data size out of range");
         }
         return createSize0(secure,
            smallPartitionCount, mediumPartitionCount, largePartitionCount,
            maxSlabCount, sideDataSize);
    }

    /*
     * Release all memory associated with instance of QBA.
     *
     * @param unlink  true if should unlink shared link.
     */
    public void destroy(boolean unlink) {
        destroy0(director, unlink);
    }

    /*
     * Release all memory associated with instance of QBA.
     */
    public void destroy() {
        destroy(true);
    }

    /*
     * Get the current value of the user reference.
     *
     * @return current value of the user reference.
     */
    public long getReference() {
        return getReference0(director);
    }

    /*
     * Conditionally set the value of the user reference.
     *
     * @param oldValue  expected old value.
     * @param newValue  value to set.
     * @return true if set.
     */
    public boolean setReference(long oldValue, long newValue) {
        return setReference0(director, oldValue, newValue);
    }

    /*
     * Returns a memory block address of size equal to or greater than "size"
     * bytes. allocate will return zero if the size is zero or if it is unable
     * to allocate a block of that size. The allocated block should be recycled
     * by invoking deallocate or reallocate.
     *
     * @param size  size of required block in bytes.
     * @return address of allocated block or zero if can not be allocated.
     *
     */
    public long allocate(long size) {
        return allocate0(director, size);
    }

    /*
     * Recycles a memory block previously allocated by allocate or reallocate.
     * If the supplied address is zero or not in memory managed by QBA then
     * deallocate does nothing.
     *
     * @param address  address of an allocated memory block or zero.
     */
    public void deallocate(long address) {
        deallocate0(director, address);
    }

    /*
     * Ensures that the memory block returned is sized equal to or greater than
     * "size" bytes. If the original block fits then the old block "address" is
     * returned. If original block is zero, smaller or significantly larger
     * then a new memory block is allocated, the contents of the old block
     * copied (if not zero) to the new block, the old block deallocated and the
     * new block address returned. The original block size is based on the size
     * returned by the size method. May return zero if unable to allocate the
     * new block (old block not deallocated.)
     *
     * @param address  address of an allocated memory block or zero.
     * @param size     size of required resize in bytes.
     * @return address of resized block or zero if can not be allocated.
     */
    public long reallocate(long address, long size) {
        return reallocate0(director, address, size);
    }

    /*
     * Clears the content of the allocation.
     *
     * @param address  address of an allocated memory block.
     */
    public void clear(long address) {
        clear0(director, address);
    }

    /*
     * Returns the number of bytes allocated to a memory block. This value may
     * exceed the size of the original request due to rounding. Zero is
     * returned if the supplied address is zero or not in memory managed by
     * QBA.
     *
     * @param address  address of an allocated memory block or zero.
     * @return size of allocation in bytes.
     */
    public long size(long address) {
        return size0(director, address);
    }

    /*
     * Recovers the base allocation address from any arbitrary address in a
     * memory block. Zero is returned if the supplied address is zero or not in
     * memory managed by QBA.
     *
     * @param address  arbitrary address in an allocated memory block.
     * @return base address of allocated memory block or zero.
     */
    public long base(long address) {
        return base0(director, address);
    }

    /*
     * Returns the address of side data corresponding to an allocated memory
     * block. The size of the side data is configuration specific. sideData may
     * return zero if side data is not available or the supplied address is
     * zero or not in memory managed by QBA.
     *
     * @param address  address of an allocated memory block or zero.
     * @return address of block's size data or zero.
     */
    public long sideData(long address) {
        return sideData0(director, address);
    }

    /*
     * Can be used to "walk" through all the allocations managed by QBA. The
     * first call should have an "address" of zero with successive calls using
     * the result of the previous call. The result itself can not be used for
     * memory access since the result may have been deallocated after fetching
     * (potential seg fault). The result can however be used to when invoking
     * size or side_data. A result of zero indicates no further blocks.
     *
     * @param address  address of an allocated memory block or zero.
     * @return next allocation or zero.
     */
    public long next(long address) {
        return next0(director, address);
    }

    public final static int STATS_SIZE = 64;

    /*
     * Can be used to sample the current allocation state of QBA. The arguments
     * are two long arrays of length STATS_SIZE. The counts array receives the
     * allocation count in each category and sizes array receives the
     * allocation size in each category. Categories are as follows;
     *
     *     Slot 0 - Sum of all other slots.
     *     Slot 1 - Maximums of administrative data (not necessarily active.)
     *     Slot 2 - Unused.
     *     Slot 3-52 - Totals for blocks sized 2^slot.
     *     Slot 53 and above - Unused.
     *
     * Slot 0 is likely the most interesting but if the count of 16 byte
     * allocations is required, for example, then use counts[4] (2^4 == 16).
     *
     * @param counts  array that will receive allocation counts.
     * @param sizes   array that will receive allocation sizes.
     */
    public void stats(long[] counts, long[] sizes) {
        if (counts == null || counts.length != STATS_SIZE) {
            throw new IllegalArgumentException(
                "counts must be a long array of length " + STATS_SIZE);
        }
        if (sizes == null || sizes.length != STATS_SIZE) {
            throw new IllegalArgumentException(
                "sizes must be a long array of length " + STATS_SIZE);
        }

        stats0(director, counts, sizes);
    }

    /*
     * Quickly allocates "count" memory blocks of "size" bytes. allocateBulk is
     * useful for allocating memory block pools. allocateBulk returns an array
     * of the addresses actually allocated, which may be length zero or less
     * than count depending on conditions. The "contiguous" flag indicates that
     * allocations should be contiguous. Contiguous is faster with the risk of
     * creating external fragmentation or not completing the request
     * satisfactorily (result of zero length.)
     *
     * @param size        size of required blocks in bytes.
     * @param contiguous  true if the allocations are to be contiguous.
     * @param addresses   address buffer.
     * @return Number of addresses allocated.
     */
    public int allocateBulk(long size, boolean contiguous, long[] addresses) {
        return allocateBulk0(director, size, contiguous, addresses);
    }

    /*
     * Quickly deallocates multiple memory blocks. deallocateBulk is useful for
     * discarding memory block pools, but can also be applied to other
     * allocations.
     *
     * @param addresses  array of addresses.
     */
    public void deallocateBulk(long[] addresses) {
        if (addresses == null) {
            throw new IllegalArgumentException("addresses should be non-null");
        }
        deallocateBulk0(director, addresses);
    }

    /*
     * Quickly allocates "count" consecutive memory blocks of "size" bytes.
     * allocateCount is useful for reducing internal fragmentation when the
     * application can fit allocations into smaller allocation units and is
     * willing to risk increasing external fragmentation or not completing the
     * request satisfactorily (result of zero.) Only deallocateCount and
     * sideData can be invoked with an allocation made using allocateCount.
     *
     * @param size        power of two size of required blocks in bytes.
     * @param count       maximum number of consecutive blocks to allocate.
     * @return address of allocated blocks or zero if can not be allocated.
     */
    public long allocateCount(long size, int count) {
        if (count < 0) {
            throw new IllegalArgumentException("count should be positive");
        }
        return allocateCount0(director, size, count);
    }

    /*
     * Recycles memory blocks allocated using allocateCount.
     *
     * @param address  allocation address returned by allocateCount.
     * @param size     power of two size of allocated blocks in bytes.
     * @param count    number of consecutive blocks to allocated.
     */
    public void deallocateCount(long address, long size, int count) {
        deallocateCount0(director, address, size, count);
    }

    /*
     * Allocates enough memory blocks necessary to keep internal fragmentation
     * to a specified degree.
     *
     * degree 1 = 25% average fragmentation (same as allocate.)
     *        2 = 12.5%
     *        3 = 6.25%
     *        4 = 3.125%
     *
     * allocateFit is useful for reducing internal fragmentation when the
     * application can fit allocations into smaller allocation units and is
     * willing to risk increasing external fragmentation or not completing the
     * request satisfactorily (result of zero.) Only deallocateFit and sideData
     * can be invoked with an allocation made using allocateFit.
     *
     * @param size    size of required blocks in bytes.
     * @param degree  degree of acceptable internal fragmentation.
     * @return address of allocated blocks or zero if can not be allocated.
     */
    public long allocateFit(long size, int degree) {
        if (degree < 1 || 4 < degree) {
            throw new IllegalArgumentException(
                "degree must be a value from 1 to 4");
        }
        return allocateFit0(director, size, degree);
    }


    /*
     * Recycles memory blocks allocated using allocateFit.
     *
     * @param address  allocation address returned by allocateFit.
     * @param size     size of required block in bytes.
     * @param degree   degree of acceptable internal fragmentation.
     */
    public  void deallocateFit(long address, long size, int degree) {
        if (degree < 1 || 4 < degree) {
            throw new IllegalArgumentException(
                "degree must be a value from 1 to 4");
        }
        deallocateFit0(director, address, size, degree);
    }

    private static native void registerNatives();
    private static native int version0();
    private static native String versionString0();
    private static native long create0(
        long address, String linkName, boolean secure,
        int smallPartitionCount,
        int mediumPartitionCount,
        int largePartitionCount,
        int maxSlabCount,
        int sideDataSize
    );
    private static native long createSize0(boolean secure,
       int smallPartitionCount,
       int mediumPartitionCount,
       int largePartitionCount,
       int maxSlabCount,
       int sideDataSize
    );
    private static native void destroy0(long qba, boolean unlink);
    private static native long getReference0(long qba);
    private static native boolean setReference0(long qba, long oldValue, long newValue);
    @HotSpotIntrinsicCandidate
    private static native long allocate0(long qba, long size);
    @HotSpotIntrinsicCandidate
    private static native void deallocate0(long qba, long address);
    @HotSpotIntrinsicCandidate
    private static native long reallocate0(long qba, long address, long size);
    private static native void clear0(long qba, long address);
    private static native long size0(long qba, long address);
    private static native long base0(long qba, long address);
    private static native long sideData0(long qba, long address);
    private static native long next0(long qba, long address);
    private static native void stats0(long qba, long[] counts, long[] sizes);
    private static native int allocateBulk0(long qba, long size, boolean contiguous, long[] addresses);
    private static native void deallocateBulk0(long qba, long[] addresses);
    private static native long allocateCount0(long qba, long size, int count);
    private static native void deallocateCount0(long qba, long address, long size, int count);
    private static native long allocateFit0(long qba, long size, int degree);
    private static native void deallocateFit0(long qba, long address, long size, int degree);
}
