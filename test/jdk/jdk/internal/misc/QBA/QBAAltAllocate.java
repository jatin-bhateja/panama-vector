/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
 */

/*
 * @test
 * @summary QBA test QBA::allocateBulk, QBA::deallocateBulk, QBA::allocateCount, QBA::deallocateCount, QBA::allocateFit, QBA::deallocateFit
 * @modules java.base/jdk.internal.misc
 *
 * @run main QBAAltAllocate
 */
import jdk.internal.misc.QBA;
import jdk.internal.misc.Unsafe;

public class QBAAltAllocate {
    public static void main(String[] args) {
        Unsafe unsafe = Unsafe.getUnsafe();
        QBA qba = QBA.create(true);

        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        final int SIZE = 16;
        final int ORDER = 63 - Long.numberOfLeadingZeros(SIZE);
        final int COUNT = 8;
        long[] addresses = new long[COUNT];
        long[] counts = new long[QBA.STATS_SIZE];
        long[] sizes = new long[QBA.STATS_SIZE];

        int count = qba.allocateBulk(SIZE, true, addresses);

        if (count != COUNT) {
            throw new RuntimeException("count not allocated");
        }

        for (int i = 1; i < COUNT; i++) {
            if (addresses[i - 1] + SIZE != addresses[i]) {
                throw new RuntimeException("addresses not contiguous");
            }
        }

        qba.deallocateBulk(addresses);

        count = qba.allocateBulk(SIZE, false, addresses);

        if (count != COUNT) {
            throw new RuntimeException("count not allocated");
        }

        qba.deallocateBulk(addresses);

        qba.stats(counts, sizes);

        if (counts[ORDER] != 0) {
            throw new RuntimeException("bulk allocations not deallocated");
        }

        long allocation = qba.allocateCount(SIZE, 4);

        if (allocation == 0) {
            throw new RuntimeException("count allocations not working");
        }

        qba.deallocateCount(allocation, SIZE, 4);

        qba.stats(counts, sizes);

        if (counts[ORDER] != 0) {
            throw new RuntimeException("count allocations not deallocated");
        }

         allocation = qba.allocateFit(48, 4);

        if (allocation == 0) {
            throw new RuntimeException("fit allocations not working");
        }

        long fit = qba.size(allocation);

        qba.stats(counts, sizes);

        qba.deallocateFit(allocation, 48, 4);

        qba.stats(counts, sizes);

        if (counts[ORDER] != 0) {
            throw new RuntimeException("fit allocations not deallocated");
        }

        qba.destroy();
    }
}
