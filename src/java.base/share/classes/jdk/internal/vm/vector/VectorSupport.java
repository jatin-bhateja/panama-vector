/*
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
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

package jdk.internal.vm.vector;

import jdk.internal.vm.annotation.IntrinsicCandidate;
import jdk.internal.misc.Unsafe;

import java.util.function.*;

public class VectorSupport {
    static {
        registerNatives();
    }

    private static final Unsafe U = Unsafe.getUnsafe();

    // Unary
    public static final int VECTOR_OP_ABS  = 0;
    public static final int VECTOR_OP_NEG  = 1;
    public static final int VECTOR_OP_SQRT = 2;

    // Binary
    public static final int VECTOR_OP_ADD  = 4;
    public static final int VECTOR_OP_SUB  = 5;
    public static final int VECTOR_OP_MUL  = 6;
    public static final int VECTOR_OP_DIV  = 7;
    public static final int VECTOR_OP_MIN  = 8;
    public static final int VECTOR_OP_MAX  = 9;

    public static final int VECTOR_OP_AND  = 10;
    public static final int VECTOR_OP_OR   = 11;
    public static final int VECTOR_OP_XOR  = 12;

    // Ternary
    public static final int VECTOR_OP_FMA  = 13;

    // Broadcast int
    public static final int VECTOR_OP_LSHIFT  = 14;
    public static final int VECTOR_OP_RSHIFT  = 15;
    public static final int VECTOR_OP_URSHIFT = 16;

    public static final int VECTOR_OP_CAST        = 17;
    public static final int VECTOR_OP_REINTERPRET = 18;

    // Mask manipulation operations
    public static final int VECTOR_OP_MASK_TRUECOUNT = 19;
    public static final int VECTOR_OP_MASK_FIRSTTRUE = 20;
    public static final int VECTOR_OP_MASK_LASTTRUE  = 21;
    public static final int VECTOR_OP_MASK_TOLONG    = 22;

    // Rotate operations
    public static final int VECTOR_OP_LROTATE = 23;
    public static final int VECTOR_OP_RROTATE = 24;

    // Compression expansion operations
    public static final int VECTOR_OP_COMPRESS = 25;
    public static final int VECTOR_OP_EXPAND = 26;

    // Math routines
    public static final int VECTOR_OP_TAN = 101;
    public static final int VECTOR_OP_TANH = 102;
    public static final int VECTOR_OP_SIN = 103;
    public static final int VECTOR_OP_SINH = 104;
    public static final int VECTOR_OP_COS = 105;
    public static final int VECTOR_OP_COSH = 106;
    public static final int VECTOR_OP_ASIN = 107;
    public static final int VECTOR_OP_ACOS = 108;
    public static final int VECTOR_OP_ATAN = 109;
    public static final int VECTOR_OP_ATAN2 = 110;
    public static final int VECTOR_OP_CBRT = 111;
    public static final int VECTOR_OP_LOG = 112;
    public static final int VECTOR_OP_LOG10 = 113;
    public static final int VECTOR_OP_LOG1P = 114;
    public static final int VECTOR_OP_POW = 115;
    public static final int VECTOR_OP_EXP = 116;
    public static final int VECTOR_OP_EXPM1 = 117;
    public static final int VECTOR_OP_HYPOT = 118;

    // See src/hotspot/share/opto/subnode.hpp
    //     struct BoolTest, and enclosed enum mask
    public static final int BT_eq = 0;  // 0000
    public static final int BT_ne = 4;  // 0100
    public static final int BT_le = 5;  // 0101
    public static final int BT_ge = 7;  // 0111
    public static final int BT_lt = 3;  // 0011
    public static final int BT_gt = 1;  // 0001
    public static final int BT_overflow = 2;     // 0010
    public static final int BT_no_overflow = 6;  // 0110
    // never = 8    1000
    // illegal = 9  1001
    // Unsigned comparisons apply to BT_le, BT_ge, BT_lt, BT_gt for integral types
    public static final int BT_unsigned_compare = 0b10000;
    public static final int BT_ule = BT_le | BT_unsigned_compare;
    public static final int BT_uge = BT_ge | BT_unsigned_compare;
    public static final int BT_ult = BT_lt | BT_unsigned_compare;
    public static final int BT_ugt = BT_gt | BT_unsigned_compare;

    // BasicType codes, for primitives only:
    public static final int
        T_FLOAT   = 6,
        T_DOUBLE  = 7,
        T_BYTE    = 8,
        T_SHORT   = 9,
        T_INT     = 10,
        T_LONG    = 11;

    /* ============================================================================ */

    public static class VectorSpecies<E> {}

    public static class VectorPayload {
        private final Object payload; // array of primitives

        public VectorPayload(Object payload) {
            this.payload = payload;
        }

        protected final Object getPayload() {
            return VectorSupport.maybeRebox(this).payload;
        }
    }

    public static class Vector<E> extends VectorPayload {
        public Vector(Object payload) {
            super(payload);
        }
    }

    public static class VectorShuffle<E> extends VectorPayload {
        public VectorShuffle(Object payload) {
            super(payload);
        }
    }
    public static class VectorMask<E> extends VectorPayload {
        public VectorMask(Object payload) {
            super(payload);
        }
    }

    /* ============================================================================ */
    public interface BroadcastOperation<VM extends VectorPayload,
                                        S extends VectorSpecies<?>> {
        VM broadcast(long l, S s);
    }

    @IntrinsicCandidate
    public static
    <VM extends VectorPayload,
     S extends VectorSpecies<E>,
     E>
    VM broadcastCoerced(Class<? extends VM> vmClass, Class<E> eClass,
                        int length,
                        long bits, S s,
                        BroadcastOperation<VM, S> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.broadcast(bits, s);
    }

    /* ============================================================================ */
    public interface ShuffleIotaOperation<S extends VectorSpecies<?>,
                                          SH extends VectorShuffle<?>> {
        SH apply(int length, int start, int step, S s);
    }

    @IntrinsicCandidate
    public static
    <E,
     S extends VectorSpecies<E>,
     SH extends VectorShuffle<E>>
    SH shuffleIota(Class<E> eClass, Class<? extends SH> shClass, S s,
                   int length,
                   int start, int step, int wrap,
                   ShuffleIotaOperation<S, SH> defaultImpl) {
       assert isNonCapturingLambda(defaultImpl) : defaultImpl;
       return defaultImpl.apply(length, start, step, s);
    }

    public interface ShuffleToVectorOperation<V extends Vector<?>,
                                              SH extends VectorShuffle<?>> {
       V apply(SH sh);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     SH extends VectorShuffle<E>,
     E>
    V shuffleToVector(Class<? extends Vector<E>> vClass, Class<E> eClass, Class<? extends SH> shClass, SH sh,
                      int length,
                      ShuffleToVectorOperation<V, SH> defaultImpl) {
      assert isNonCapturingLambda(defaultImpl) : defaultImpl;
      return defaultImpl.apply(sh);
    }

    /* ============================================================================ */
    public interface IndexOperation<V extends Vector<?>,
                                    S extends VectorSpecies<?>> {
        V index(V v, int step, S s);
    }

    //FIXME @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     E,
     S extends VectorSpecies<E>>
    V indexVector(Class<? extends V> vClass, Class<E> eClass,
                  int length,
                  V v, int step, S s,
                  IndexOperation<V, S> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.index(v, step, s);
    }

    /* ============================================================================ */

    public interface ReductionOperation<V extends Vector<?>,
                                        M extends VectorMask<?>> {
        long apply(V v, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    long reductionCoerced(int oprId,
                          Class<? extends V> vClass, Class<? extends M> mClass, Class<E> eClass,
                          int length,
                          V v, M m,
                          ReductionOperation<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, m);
    }


    /* ============================================================================ */

    public interface VecExtractOp<V extends Vector<?>> {
        long apply(V v, int i);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     E>
    long extract(Class<? extends V> vClass, Class<E> eClass,
                 int length,
                 V v, int i,
                 VecExtractOp<V> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, i);
    }

    /* ============================================================================ */

    public interface VecInsertOp<V extends Vector<?>> {
        V apply(V v, int i, long val);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     E>
    V insert(Class<? extends V> vClass, Class<E> eClass,
             int length,
             V v, int i, long val,
             VecInsertOp<V> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, i, val);
    }

    /* ============================================================================ */

    public interface UnaryOperation<V extends Vector<?>,
                                    M extends VectorMask<?>> {
        V apply(V v, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    V unaryOp(int oprId,
              Class<? extends V> vClass, Class<? extends M> mClass, Class<E> eClass,
              int length,
              V v, M m,
              UnaryOperation<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, m);
    }

    /* ============================================================================ */

    public interface BinaryOperation<VM extends VectorPayload,
                                     M extends VectorMask<?>> {
        VM apply(VM v1, VM v2, M m);
    }

    @IntrinsicCandidate
    public static
    <VM extends VectorPayload,
     M extends VectorMask<E>,
     E>
    VM binaryOp(int oprId,
                Class<? extends VM> vmClass, Class<? extends M> mClass, Class<E> eClass,
                int length,
                VM v1, VM v2, M m,
                BinaryOperation<VM, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v1, v2, m);
    }

    /* ============================================================================ */

    public interface TernaryOperation<V extends Vector<?>,
                                      M extends VectorMask<?>> {
        V apply(V v1, V v2, V v3, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    V ternaryOp(int oprId,
                Class<? extends V> vClass, Class<? extends M> mClass, Class<E> eClass,
                int length,
                V v1, V v2, V v3, M m,
                TernaryOperation<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v1, v2, v3, m);
    }

    /* ============================================================================ */

    // Memory operations

    public interface LoadOperation<C,
                                   VM extends VectorPayload,
                                   S extends VectorSpecies<?>> {
        VM load(C container, int index, S s);
    }

    @IntrinsicCandidate
    public static
    <C,
     VM extends VectorPayload,
     E,
     S extends VectorSpecies<E>>
    VM load(Class<? extends VM> vmClass, Class<E> eClass,
            int length,
            Object base, long offset,
            C container, int index, S s,
            LoadOperation<C, VM, S> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.load(container, index, s);
    }

    /* ============================================================================ */

    public interface LoadVectorMaskedOperation<C,
                                               V extends Vector<?>,
                                               S extends VectorSpecies<?>,
                                               M extends VectorMask<?>> {
        V load(C container, int index, S s, M m);
    }

    @IntrinsicCandidate
    public static
    <C,
     V extends Vector<?>,
     E,
     S extends VectorSpecies<E>,
     M extends VectorMask<E>>
    V loadMasked(Class<? extends V> vClass, Class<M> mClass, Class<E> eClass,
                 int length,
                 Object base, long offset,
                 M m, C container, int index, S s,
                 LoadVectorMaskedOperation<C, V, S, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.load(container, index, s, m);
    }

    /* ============================================================================ */

    public interface LoadVectorOperationWithMap<C,
                                                V extends Vector<?>,
                                                S extends VectorSpecies<?>,
                                                M extends VectorMask<?>> {
        V loadWithMap(C container, int index, int[] indexMap, int indexM, S s, M m);
    }

    @IntrinsicCandidate
    public static
    <C,
     V extends Vector<?>,
     W extends Vector<Integer>,
     S extends VectorSpecies<E>,
     M extends VectorMask<E>,
     E>
    V loadWithMap(Class<? extends V> vClass, Class<M> mClass, Class<E> eClass,
                  int length,
                  Class<? extends Vector<Integer>> vectorIndexClass,
                  Object base, long offset,
                  W index_vector,
                  M m, C container, int index, int[] indexMap, int indexM, S s,
                  LoadVectorOperationWithMap<C, V, S, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.loadWithMap(container, index, indexMap, indexM, s, m);
    }

    /* ============================================================================ */

    public interface StoreVectorOperation<C,
                                          V extends Vector<?>> {
        void store(C container, int index, V v);
    }

    @IntrinsicCandidate
    public static
    <C,
     V extends Vector<?>>
    void store(Class<?> vClass, Class<?> eClass,
               int length,
               Object base, long offset,
               V v, C container, int index,
               StoreVectorOperation<C, V> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        defaultImpl.store(container, index, v);
    }

    public interface StoreVectorMaskedOperation<C,
                                                V extends Vector<?>,
                                                M extends VectorMask<?>> {
        void store(C container, int index, V v, M m);
    }

    @IntrinsicCandidate
    public static
    <C,
     V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    void storeMasked(Class<? extends V> vClass, Class<M> mClass, Class<E> eClass,
                     int length,
                     Object base, long offset,
                     V v, M m, C container, int index,
                     StoreVectorMaskedOperation<C, V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        defaultImpl.store(container, index, v, m);
    }

    /* ============================================================================ */

    public interface StoreVectorOperationWithMap<C,
                                                 V extends Vector<?>,
                                                 M extends VectorMask<?>> {
        void storeWithMap(C container, int index, V v, int[] indexMap, int indexM, M m);
    }

    @IntrinsicCandidate
    public static
    <C,
     V extends Vector<E>,
     W extends Vector<Integer>,
     M extends VectorMask<E>,
     E>
    void storeWithMap(Class<? extends V> vClass, Class<M> mClass, Class<E> eClass,
                      int length,
                      Class<? extends Vector<Integer>> vectorIndexClass,
                      Object base, long offset,
                      W index_vector,
                      V v, M m, C container, int index, int[] indexMap, int indexM,
                      StoreVectorOperationWithMap<C, V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        defaultImpl.storeWithMap(container, index, v, indexMap, indexM, m);
    }

    /* ============================================================================ */

    @IntrinsicCandidate
    public static
    <M extends VectorMask<E>,
     E>
    boolean test(int cond,
                 Class<?> mClass, Class<?> eClass,
                 int length,
                 M m1, M m2,
                 BiFunction<M, M, Boolean> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(m1, m2);
    }

    /* ============================================================================ */

    public interface VectorCompareOp<V extends Vector<?>,
                                     M extends VectorMask<?>> {
        M apply(int cond, V v1, V v2, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    M compare(int cond,
              Class<? extends V> vectorClass, Class<M> mClass, Class<E> eClass,
              int length,
              V v1, V v2, M m,
              VectorCompareOp<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(cond, v1, v2, m);
    }

    /* ============================================================================ */
    public interface VectorRearrangeOp<V extends Vector<?>,
                                       SH extends VectorShuffle<?>,
                                       M extends VectorMask<?>> {
        V apply(V v, SH sh, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     SH extends VectorShuffle<E>,
     M  extends VectorMask<E>,
     E>
    V rearrangeOp(Class<? extends V> vClass, Class<SH> shClass, Class<M> mClass, Class<E> eClass,
                  int length,
                  V v, SH sh, M m,
                  VectorRearrangeOp<V, SH, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, sh, m);
    }

    /* ============================================================================ */

    public interface VectorBlendOp<V extends Vector<?>,
                                   M extends VectorMask<?>> {
        V apply(V v1, V v2, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    V blend(Class<? extends V> vClass, Class<M> mClass, Class<E> eClass,
            int length,
            V v1, V v2, M m,
            VectorBlendOp<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v1, v2, m);
    }

    /* ============================================================================ */

    public interface VectorBroadcastIntOp<V extends Vector<?>,
                                          M extends VectorMask<?>> {
        V apply(V v, int n, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    V broadcastInt(int opr,
                   Class<? extends V> vClass, Class<? extends M> mClass, Class<E> eClass,
                   int length,
                   V v, int n, M m,
                   VectorBroadcastIntOp<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, n, m);
    }

    /* ============================================================================ */

    public interface VectorConvertOp<VOUT extends VectorPayload,
                                     VIN extends VectorPayload,
                                     S extends VectorSpecies<?>> {
        VOUT apply(VIN v, S s);
    }

    // Users of this intrinsic assume that it respects
    // REGISTER_ENDIAN, which is currently ByteOrder.LITTLE_ENDIAN.
    // See javadoc for REGISTER_ENDIAN.

    @IntrinsicCandidate
    public static <VOUT extends VectorPayload,
                    VIN extends VectorPayload,
                      S extends VectorSpecies<?>>
    VOUT convert(int oprId,
              Class<?> fromVectorClass, Class<?> fromeClass, int fromVLen,
              Class<?>   toVectorClass, Class<?>   toeClass, int   toVLen,
              VIN v, S s,
              VectorConvertOp<VOUT, VIN, S> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v, s);
    }

    /* ============================================================================ */

    public interface ComExpOperation<V extends Vector<?>,
                                     M extends VectorMask<?>> {
        V apply(V v1, V v2, M m);
    }

    @IntrinsicCandidate
    public static
    <V extends Vector<E>,
     M extends VectorMask<E>,
     E>
    V comExpOp(int opr,
              Class<? extends V> vClass, Class<? extends M> mClass, Class<E> eClass,
              int length, V v1, V v2, M m,
              ComExpOperation<V, M> defaultImpl) {
        assert isNonCapturingLambda(defaultImpl) : defaultImpl;
        return defaultImpl.apply(v1, v2, m);
    }

    /* ============================================================================ */

    @IntrinsicCandidate
    public static
    <VP extends VectorPayload>
    VP maybeRebox(VP v) {
        // The fence is added here to avoid memory aliasing problems in C2 between scalar & vector accesses.
        // TODO: move the fence generation into C2. Generate only when reboxing is taking place.
        U.loadFence();
        return v;
    }

    /* ============================================================================ */
    public interface VectorMaskOp<M extends VectorMask<?>> {
        long apply(M m);
    }

    @IntrinsicCandidate
    public static
    <M extends VectorMask<E>,
     E>
    long maskReductionCoerced(int oper,
                              Class<? extends M> mClass, Class<?> eClass,
                              int length,
                              M m,
                              VectorMaskOp<M> defaultImpl) {
       assert isNonCapturingLambda(defaultImpl) : defaultImpl;
       return defaultImpl.apply(m);
    }

    /* ============================================================================ */

    // query the JVM's supported vector sizes and types
    public static native int getMaxLaneCount(Class<?> etype);

    /* ============================================================================ */

    public static boolean isNonCapturingLambda(Object o) {
        return o.getClass().getDeclaredFields().length == 0;
    }

    /* ============================================================================ */

    private static native int registerNatives();
}
