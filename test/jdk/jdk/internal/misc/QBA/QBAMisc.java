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
 * @summary QBA test QBA::clear, QBA::size, QBA::base, QBA::sideData, QBA::next, QBA::stats
 * @modules java.base/jdk.internal.misc
 *
 * @run main QBAMisc
 */
import jdk.internal.misc.QBA;
import jdk.internal.misc.Unsafe;

public class QBAMisc {
    public static void main(String[] args) {
        Unsafe unsafe = Unsafe.getUnsafe();
        QBA qba = QBA.create(true, 8, 8, 8, 2048, 8);

        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        long size = 1024;
        int order = 63 - Long.numberOfLeadingZeros(size);

        long allocation = qba.allocate(size);

        unsafe.setMemory(null, allocation, size, (byte)0xFF);

        if (unsafe.getLong(allocation) != -1 || unsafe.getLong(allocation + size - 8) != -1) {
            throw new RuntimeException("memory not set");
        }

        qba.clear(allocation);

        if (unsafe.getLong(allocation) != 0 || unsafe.getLong(allocation + size - 8) != 0) {
             throw new RuntimeException("memory not clear");
        }

        for (long i = 1; i < size; i++) {
            if (qba.base(allocation + i) != allocation) {
                throw new RuntimeException("base not working correctly");
            }
        }

        long sideData = qba.sideData(allocation);
        unsafe.putLong(sideData, 0x12345678);
        sideData = qba.sideData(allocation);

        if (unsafe.getLong(sideData) != 0x12345678) {
            throw new RuntimeException("side data not working correctly");
        }

        long next = qba.allocate(size);
        long peek = qba.next(allocation);

        if (qba.next(allocation) != next) {
            throw new RuntimeException("next not working correctly");
        }

        long[] counts = new long[QBA.STATS_SIZE];
        long[] sizes = new long[QBA.STATS_SIZE];

        qba.stats(counts, sizes);

        if (counts[order] != 2 && sizes[order] != 2 * size) {
            throw new RuntimeException("stats not working correctly");
        }

        qba.deallocate(allocation);
        qba.deallocate(next);

        for (size = 8; size <= 128L * 1024L * 1024L; size <<= 1) {
            allocation = qba.allocate(size);

            if (qba.size(allocation) != size) {
                throw new RuntimeException("size incorrect " + size);
            }

            qba.deallocate(allocation);
        }

        qba.destroy();
    }
}
