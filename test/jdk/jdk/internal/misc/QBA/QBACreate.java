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
 * @summary QBA test QBA::create and QBA::createSize.
 * @modules java.base/jdk.internal.misc
 *
 * @run main QBACreate
 */
import jdk.internal.misc.QBA;

public class QBACreate {
    public static void main(String[] args) {
        QBA qba;

        qba = QBA.create(false);

        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        qba.destroy();

        qba = QBA.create(false, 8, 8, 8, 1024, 0);

        if (qba == null) {
            throw new RuntimeException("qba not created");
        }

        qba.destroy();

        long K = 1024;
        long M = K * K;
        int INT_MAX = 16 * 1024;
        long LONG_MAX = (long)INT_MAX;
        qba = QBA.create(false, INT_MAX, INT_MAX, INT_MAX, INT_MAX, 0);

        if (qba != null) {
            throw new RuntimeException("qba create not handling low memory");
        }

        long needed = QBA.createSize(false, 8, 8, 8, 1024, 0);

        long approximate = 8 * LONG_MAX * 8 +
                           8 * LONG_MAX * 2 * K +
                           8 * LONG_MAX * 512 * K;

        if (needed < approximate || approximate + M < needed) {
            throw new RuntimeException("qba create size is incorrect");
        }
    }
}
