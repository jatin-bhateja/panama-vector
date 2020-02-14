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
 * @summary QBA test QBA::reallocate.
 * @modules java.base/jdk.internal.misc
 *
 * @run main QBAReallocate
 */
import jdk.internal.misc.QBA;
import jdk.internal.misc.Unsafe;

public class QBAReallocate {
    public static void main(String[] args) {
        Unsafe unsafe = Unsafe.getUnsafe();
        QBA qba = QBA.create(true);

        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        long size = 8;

        long allocation = qba.allocate(size);
        unsafe.setMemory(null, allocation, size, (byte)0xFF);

        long reallocation = qba.reallocate(allocation, size - 1);

        if (allocation != reallocation) {
            throw new RuntimeException("didn't use same allocation");
        }

        reallocation = qba.reallocate(allocation, size + 1);

        if (allocation == reallocation) {
            throw new RuntimeException("Used same allocation");
        }

        if (qba.size(reallocation) < size + 1) {
            throw new RuntimeException("Reallocation not big enough");
        }

        if (unsafe.getLong(null, reallocation) != -1) {
            throw new RuntimeException("Reallocation not copied");
        }

        qba.deallocate(reallocation);

        qba.destroy();
    }
}
