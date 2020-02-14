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
 * @summary C2 Intrinsification of QBA allocate, reallocate and reallocate.
 * @modules java.base/jdk.internal.misc
 *
 * @run main compiler.intrinsics.qba.QBAIntrinsics
 */
package compiler.intrinsics.qba;

import jdk.internal.misc.QBA;

public class QBAIntrinsics {
    static final long ITERATIONS = 100000;
    static final QBA qba = QBA.create(false);

    public static void test(long expected1, long expected2) {
        long allocation1 = qba.allocate(8);
        long allocation2 = qba.reallocate(allocation1, 16);
        qba.deallocate(allocation2);

        if (allocation1 != expected1 || allocation2 != expected2) {
            throw new RuntimeException(
                "results from qba intrinsics not consistent");
        }
    }

    public static void main(String[] args) {
        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        long expected1 = qba.allocate(8);
        long expected2 = qba.reallocate(expected1, 16);
        qba.deallocate(expected2);

        for (long i = 0; i < ITERATIONS; i++) {
            test(expected1, expected2);
        }
    }
 }
