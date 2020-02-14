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
 * @summary QBA test QBA::allocate and QBA::deallocate.
 * @modules java.base/jdk.internal.misc
 *
 * @run main QBAAllocate
 */
import jdk.internal.misc.QBA;
import jdk.internal.misc.Unsafe;

public class QBAAllocate {
    public static void main(String[] args) {
        Unsafe unsafe = Unsafe.getUnsafe();
        QBA qba = QBA.create(true);

        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        long allocation1 = qba.allocate(8);

        if (allocation1 == 0) {
            throw new RuntimeException("no allocation");
        }

        if (unsafe.getLong(null, allocation1) != 0) {
            throw new RuntimeException("allocation not zero");
        }

        unsafe.setMemory(null, allocation1, 8, (byte)0xFF);

        if (unsafe.getLong(null, allocation1) != -1) {
            throw new RuntimeException("allocation not -1");
        }

        qba.deallocate(allocation1);

        long allocation2 = qba.allocate(8);

        if (allocation1 != allocation2) {
            throw new RuntimeException("not recycling allocation");
        }

        if (unsafe.getLong(null, allocation2) != 0) {
            throw new RuntimeException("recycled allocation not zero");
        }

        long allocation3 = qba.allocate(8);

        if (allocation2 == allocation3) {
            throw new RuntimeException("allocation is not distinct");
        }

        qba.deallocate(allocation2);
        qba.deallocate(allocation3);

        for (long size = 0; size < 64L; size++) {
            long allocation = qba.allocate(size);

            if (allocation == 0) {
                throw new RuntimeException("no allocation for " + size);
            }

            qba.deallocate(allocation);
        }

        for (long size = 1; size <= 128L * 1024L * 1024L; size <<= 1) {
            long allocation = qba.allocate(size);

            if (allocation == 0) {
                throw new RuntimeException("no allocation for " + size);
            }

            qba.deallocate(allocation);
        }

        qba.destroy();
    }
}
