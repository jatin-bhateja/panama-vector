/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
 * or visit www.oracle.com if you need additional information or have
 * questions.
 */
package jdk.incubator.vector;

import jdk.internal.misc.Unsafe;
import jdk.internal.vm.annotation.ForceInline;

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;

/**
 * A {@code Vector} is designed for use in computations that can be transformed
 * by a runtime compiler, on supported hardware, to Single Instruction Multiple
 * Data (SIMD) computations leveraging vector hardware registers and vector
 * hardware instructions.  Such SIMD computations exploit data parallelism to
 * perform the same operation on multiple data points simultaneously in a
 * faster time it would ordinarily take to perform the same operation
 * sequentially on each data point.
 * <p>
 * A Vector represents an ordered immutable sequence of values of the same
 * element type {@code e} that is one of the following primitive types
 * {@code byte}, {@code short}, {@code int}, {@code long}, {@code float}, or
 * {@code double}).  The type variable {@code E} corresponds to the boxed
 * element type, specifically the class that wraps a value of {@code e} in an
 * object (such the {@code Integer} class that wraps a value of {@code int}}.
 * A Vector has a {@link #shape() shape} {@code S}, extending type
 * {@link Shape}, that governs the total {@link #bitSize() size} in bits
 * of the sequence of values.
 * <p>
 * The number of values in the sequence is referred to as the Vector
 * {@link #length() length}.  The length also corresponds to the number of
 * Vector lanes.  The lane element at lane index {@code N} (from {@code 0},
 * inclusive, to length, exclusive) corresponds to the {@code N + 1}'th value in
 * the sequence.
 * Note: this arrangement
 * of Vector bit size, Vector length, element bit size, and lane element index
 * has no bearing on how a Vector instance and its sequence of elements may be
 * arranged in memory or represented as a value in a vector hardware register.
 * <p>
 * Vector declares a set of vector operations (methods) that are common to all
 * element types (such as addition).  Sub-classes of Vector with a concrete
 * boxed element type declare further operations that are specific to that
 * element type (such as access to element values in lanes, logical operations
 * on values of integral elements types, or transcendental operations on values
 * of floating point element types).
 * There are six sub-classes of Vector corresponding to the supported set
 * of element types, {@link ByteVector<S>}, {@link ShortVector<S>},
 * {@link IntVector<S>} {@link LongVector<S>}, {@link FloatVector<S>}, and
 * {@link DoubleVector<S>}.
 * <p>
 * Vector values, instances of Vector, are created from a special kind of
 * factory called a {@link Species}.  A Species has an
 * element type and shape and creates Vector values of the same element type
 * and shape.
 * A species can be {@link #speciesInstance obtained} given an element type and
 * shape, or a preferred species can be {@link #speciesInstance obtained} given
 * just an element type where the most optimal shape is selected for the current
 * platform.  It is recommended that Species instances be held in
 * {@code static final} fields for optimal creation and usage of Vector values
 * by the runtime compiler.
 * <p>
 * Vector operations can be grouped into various categories and their behaviour
 * generally specified as follows:
 * <ul>
 * <li>
 * A vector unary operation (1-ary) operates on one input vector to produce a
 * result vector.
 * For each lane of the input vector the
 * lane element is operated on using the specified scalar unary operation and
 * the element result is placed into the vector result at the same lane.
 * The following pseudocode expresses the behaviour of this operation category,
 * where {@code e} is the element type and {@code EVector} corresponds to the
 * primitive Vector type:
 *
 * <pre>{@code
 * EVector<S> a = ...;
 * e[] ar = new e[a.length()];
 * for (int i = 0; i < a.length(); i++) {
 *     ar[i] = scalar_unary_op(a.get(i));
 * }
 * EVector<S> r = a.species().fromArray(ar, 0);
 * }</pre>
 *
 * Unless otherwise specified the input and result vectors will have the same
 * element type and shape.
 *
 * <li>
 * A vector binary operation (2-ary) operates on two input
 * vectors to produce a result vector.
 * For each lane of the two input vectors,
 * a and b say, the corresponding lane elements from a and b are operated on
 * using the specified scalar binary operation and the element result is placed
 * into the vector result at the same lane.
 * The following pseudocode expresses the behaviour of this operation category:
 *
 * <pre>{@code
 * EVector<S> a = ...;
 * EVector<S> b = ...;
 * e[] ar = new e[a.length()];
 * for (int i = 0; i < a.length(); i++) {
 *     ar[i] = scalar_binary_op(a.get(i), b.get(i));
 * }
 * EVector<S> r = a.species().fromArray(ar, 0);
 * }</pre>
 *
 * Unless otherwise specified the two input and result vectors will have the
 * same element type and shape.
 *
 * <li>
 * Generalizing from unary (1-ary) and binary (2-ary) operations, a vector n-ary
 * operation operates in n input vectors to produce a
 * result vector.
 * N lane elements from each input vector are operated on
 * using the specified n-ary scalar operation and the element result is placed
 * into the vector result at the same lane.
 * Unless otherwise specified the n input and result vectors will have the same
 * element type and shape.
 *
 * <li>
 * A vector reduction operation operates on all the lane
 * elements of an input vector.
 * An accumulation function is applied to all the
 * lane elements to produce a scalar result.
 * If the reduction operation is associative then the result may be accumulated
 * by operating on the lane elements in any order using a specified associative
 * scalar binary operation and identity value.  Otherwise, the reduction
 * operation specifies the behaviour of the accumulation function.
 * The following pseudocode expresses the behaviour of this operation category
 * if it is associative:
 * <pre>{@code
 * EVector<S> a = ...;
 * e r = <identity value>;
 * for (int i = 0; i < a.length(); i++) {
 *     r = assoc_scalar_binary_op(r, a.get(i));
 * }
 * }</pre>
 *
 * Unless otherwise specified the scalar result type and element type will be
 * the same.
 *
 * <li>
 * A vector binary test operation operates on two input vectors to produce a
 * result mask.  For each lane of the two input vectors, a and b say, the
 * the corresponding lane elements from a and b are operated on using the
 * specified scalar binary test operation and the boolean result is placed
 * into the mask at the same lane.
 * The following pseudocode expresses the behaviour of this operation category:
 * <pre>{@code
 * EVector<S> a = ...;
 * EVector<S> b = ...;
 * boolean[] ar = new boolean[a.length()];
 * for (int i = 0; i < a.length(); i++) {
 *     ar[i] = scalar_binary_test_op(a.get(i), b.get(i));
 * }
 * Mask<E, S> r = a.species().maskFromArray(ar, 0);
 * }</pre>
 *
 * Unless otherwise specified the two input vectors and result mask will have
 * the same element type and shape.
 *
 * <li>
 * The prior categories of operation can be said to operate within the vector
 * lanes, where lane access is uniformly applied to all vectors, specifically
 * the scalar operation is applied to elements taken from input vectors at the
 * same lane, and if appropriate applied to the result vector at the same lane.
 * A further category of operation is a cross-lane vector operation where lane
 * access is defined by the arguments to the operation.  Cross-lane operations
 * generally rearrange lane elements, by permutation (commonly controlled by a
 * {@link Shuffle}) or by blending (commonly controlled by a {@link Mask}).
 * Such an operation explicitly specifies how it rearranges lane elements.
 * </ul>
 *
 * If a vector operation is represented as an instance method then first input
 * vector corresponds to {@code this} vector and subsequent input vectors are
 * arguments of the method.  Otherwise, if the an operation is represented as a
 * static method then all input vectors are arguments of the method.
 * <p>
 * If a vector operation does not belong to one of the above categories then
 * the operation explicitly specifies how it processes the lane elements of
 * input vectors, and where appropriate expresses the behaviour using
 * pseudocode.
 *
 * <p>
 * Many vector operations provide an additional {@link Mask mask} accepting
 * variant.
 * The mask controls which lanes are selected for application of the scalar
 * operation.  Masks are a key component for the support of control flow in
 * vector computations.
 * <p>
 * For certain operation categories the mask accepting variants can be specified
 * in generic terms.  If a lane of the mask is set then the scalar operation is
 * applied to corresponding lane elements, otherwise if a lane of a mask is not
 * set then a default scalar operation is applied and its result is placed into
 * the vector result at the same lane. The default operation is specified for
 * the following operation categories:
 * <ul>
 * <li>
 * For a vector n-ary operation the default operation is a function that returns
 * it's first argument, specifically a lane element of the first input vector.
 * <li>
 * For an associative vector reduction operation the default operation is a
 * function that returns the identity value.
 * <li>
 * For vector binary test operation the default operation is a function that
 * returns false.
 *</ul>
 * Otherwise, the mask accepting variant of the operation explicitly specifies
 * how it processes the lane elements of input vectors, and where appropriate
 * expresses the behaviour using pseudocode.
 *
 * <p>
 * For convenience many vector operations, of arity greater than one, provide
 * an additional scalar accepting variant.  This variant accepts compatible
 * scalar values instead of vectors for the second and subsequent input vectors,
 * if any.
 * Unless otherwise specified the scalar variant behaves as if each scalar value
 * is transformed to a vector using the vector Species
 * {@code broadcast} operation, and
 * then the vector accepting vector operation is applied using the transformed
 * values.
 *
 * <p>
 * This is a value-based
 * class; use of identity-sensitive operations (including reference equality
 * ({@code ==}), identity hash code, or synchronization) on instances of
 * {@code Vector} may have unpredictable results and should be avoided.
 *
 * @param <E> the boxed element type of elements in this vector
 * @param <S> the type of shape of this vector
 */
public abstract class Vector<E, S extends Vector.Shape> {

    Vector() {}

    /**
     * Returns the species of this vector.
     *
     * @return the species of this vector
     */
    public abstract Species<E, S> species();

    // @@@

    /**
     * Returns the primitive element type of this vector.
     *
     * @return the primitive element type of this vector
     */
    public Class<E> elementType() { return species().elementType(); }

    /**
     * Returns the element size, in bits, of this vector.
     *
     * @return the element size, in bits
     */
    public int elementSize() { return species().elementSize(); }

    /**
     * Returns the shape of this vector.
     *
     * @return the shape of this vector
     */
    public S shape() { return species().shape(); }

    /**
     * Returns the number of vector lanes (the length).
     *
     * @return the number of vector lanes
     */
    public int length() { return species().length(); }

    /**
     * Returns the total vector size, in bits.
     *
     * @return the total vector size, in bits
     */
    public int bitSize() { return species().bitSize(); }

    //Arithmetic

    /**
     * Adds this vector to an input vector.
     * <p>
     * This is a vector binary operation where the primitive addition operation
     * ({@code +}) is applied to lane elements.
     *
     * @param b the input vector
     * @return the result of adding this vector to the input vector
     */
    public abstract Vector<E, S> add(Vector<E, S> b);

    /**
     * Adds this vector to an input vector, selecting lane elements
     * controlled by a mask.
     * <p>
     * This is a vector binary operation where the primitive addition operation
     * ({@code +}) is applied to lane elements.
     *
     * @param b the input vector
     * @param m the mask controlling lane selection
     * @return the result of adding this vector to the given vector
     */
    public abstract Vector<E, S> add(Vector<E, S> b, Mask<E, S> m);

    public abstract Vector<E, S> addSaturate(Vector<E, S> o);

    public abstract Vector<E, S> addSaturate(Vector<E, S> o, Mask<E, S> m);

    public abstract Vector<E, S> sub(Vector<E, S> o);

    public abstract Vector<E, S> sub(Vector<E, S> o, Mask<E, S> m);

    public abstract Vector<E, S> subSaturate(Vector<E, S> o);

    public abstract Vector<E, S> subSaturate(Vector<E, S> o, Mask<E, S> m);

    public abstract Vector<E, S> mul(Vector<E, S> o);

    public abstract Vector<E, S> mul(Vector<E, S> o, Mask<E, S> m);

    /**
     * Negates this vector.
     * <p>
     * This is a vector unary operation where the primitive negation operation
     * ({@code -})is applied to lane elements.
     *
     * @return the result of negating this vector
     */
    public abstract Vector<E, S> neg();

    public abstract Vector<E, S> neg(Mask<E, S> m);

    //Maths from java.math
    public abstract Vector<E, S> abs();

    public abstract Vector<E, S> abs(Mask<E, S> m);

    public abstract Vector<E, S> min(Vector<E, S> o);

    public abstract Vector<E, S> max(Vector<E, S> o);

    //TODO: Parity

    //Comparisons

    //TODO: N.B. Floating point NaN behaviors?
    //TODO: Check the JLS

    /**
     * Tests if this vector is equal to the given vector.
     * <p>
     * This is a vector binary test operation where the primitive equals
     * operation ({@code ==}) is applied to lane elements.
     *
     * @param b the given vector
     * @return the result mask of testing if this vector is equal to the given
     * vector
     */
    public abstract Mask<E, S> equal(Vector<E, S> b);

    public abstract Mask<E, S> notEqual(Vector<E, S> o);

    /**
     * Tests if this vector is less than the given vector.
     * <p>
     * This is a vector binary test operation where the primitive less than
     * operation ({@code <}) is applied to lane elements.
     *
     * @param b the given vector
     * @return the mask result of testing if this vector is less than the given
     * vector
     */
    public abstract Mask<E, S> lessThan(Vector<E, S> b);

    public abstract Mask<E, S> lessThanEq(Vector<E, S> o);

    public abstract Mask<E, S> greaterThan(Vector<E, S> o);

    public abstract Mask<E, S> greaterThanEq(Vector<E, S> o);

    //Elemental shifting

    /**
     * Rotates left the lane elements of this vector by the given number of
     * lanes, {@code i}, modulus the vector length.
     * <p>
     * This is a cross-lane operation that permutes the lane elements of this
     * vector.
     * For each lane of the input vector, at lane index {@code N}, the lane
     * element is assigned to the result vector at lane index
     * {@code (i + N) % this.length()}.
     *
     * @param i the number of lanes to rotate left
     * @return the result of rotating left lane elements of this vector by the
     * given number of lanes
     */
    public abstract Vector<E, S> rotateEL(int i); //Rotate elements left

    /**
     * Rotates right the lane elements of this vector by the given number of
     * lanes, {@code i}, modulus the vector length.
     * <p>
     * This is a cross-lane operation that permutes the lane elements of this
     * vector and behaves as if rotating left the lane elements by
     * {@code this.length() - (i % this.length())} lanes.
     *
     * @param i the number of lanes to rotate left
     * @return the result of rotating right lane elements of this vector by the
     * given number of lanes
     */
    public abstract Vector<E, S> rotateER(int i); //Rotate elements right

    public abstract Vector<E, S> shiftEL(int i); //shift elements left

    public abstract Vector<E, S> shiftER(int i); //shift elements right

    /**
     * Blends the lane elements of this vector with those of an input vector,
     * selecting lanes controlled by a mask.
     * <p>
     * For each lane of the mask, at lane index {@code N}, if the mask lane
     * is set then the lane element at {@code N} from the input vector is
     * selected and placed into the resulting vector at {@code N},
     * otherwise the the lane element at {@code N} from this input vector is
     * selected and placed into the resulting vector at {@code N}.
     *
     * @param b the input vector
     * @param m the mask controlling lane selection
     * @return the result of blending the lane elements of this vector with
     * those of an input vector
     */
    public abstract Vector<E, S> blend(Vector<E, S> b, Mask<E, S> m);

    /**
     * Shuffles the lane elements of this vector and those of an input vector,
     * selecting lane indexes controlled by a shuffle.
     * <p>
     * This is a cross-lane operation that permutes the lane elements of this
     * vector and tine input vector.
     * For each lane of the shuffle, at lane index {@code N}, if the shuffle
     * lane element, {@code I}, is less than the length of this vector then the
     * lane element at {@code I} from this vector is selected and placed into
     * the resulting vector at {@code N}, otherwise the lane element at
     * {@code I - this.length()} from the input vector is selected and placed
     * into the resulting vector at {@code N}.
     *
     * @param b the input vector
     * @param s the shuffle controlling lane index selection
     * @return the result of shuffling the lane elements of this vector and
     * those of an input vector
     * @throws IndexOutOfBoundsException if any lane element is {@code < 0} or
     * {@code >= 2 * this.length())
     */
    public abstract Vector<E, S> shuffle(Vector<E, S> b, Shuffle<E, S> s);

    /**
     * Shuffles the lane elements of this vector selecting lane indexes
     * controlled by a shuffle.
     * <p>
     * This is a cross-lane operation that permutes the lane elements of this
     * vector.
     * For each lane of the shuffle, at lane index {@code N} with lane
     * element {@code I}, the lane element at {@code I} from this vector is
     * selected and placed into the resulting vector at {@code N}.
     *
     * @param s the shuffle controlling lane index selection
     * @return the result of shuffling the lane elements of this vector
     * @throws IndexOutOfBoundsException if any lane element is {@code < 0} or
     * {@code >= this.length())
     */
    public abstract Vector<E, S> swizzle(Shuffle<E, S> s);


    // Conversions

    // Bitwise preserving

    /**
     * Transforms this vector to a vector of the given species shape {@code T}
     * and element type {@code F}.
     * <p>
     * This method behaves as if it returns the result of calling
     * {@link Species#reshape(Vector) reshape} on the given species with this
     * vector:
     * <pre>{@code
     * return species.reshape(this);
     * }</pre>
     *
     * @param species the species
     * @param <F> the boxed element type of the species
     * @param <T> the type of shape of the species
     * @return a vector transformed by shape and element type
     * @see Species#reshape(Vector)
     */
    @ForceInline
    public <F, T extends Shape> Vector<F, T> reshape(Species<F, T> species) {
        return species.reshape(this);
    }

    /**
     * Transforms this vector to a vector of the given species element type
     * {@code F}, where this vector's shape {@code S} is preserved.
     * <p>
     * This method behaves as if it returns the result of calling
     * {@link Species#rebracket(Vector) rebracket} on the given species with this
     * vector:
     * <pre>{@code
     * return species.rebracket(this);
     * }</pre>
     *
     * @param species the species
     * @param <F> the boxed element type of the species
     * @return a vector transformed element type
     * @see Species#rebracket(Vector)
     */
    @ForceInline
    public <F> Vector<F, S> rebracket(Species<F, S> species) {
        return species.reshape(this);
    }

    /**
     * Transforms this vector to a vector of the given species shape {@code T},
     * where this vector's element type {@code E} is preserved.
     * <p>
     * This method behaves as if it returns the result of calling
     * {@link Species#resize(Vector) resize} on the given species with this vector:
     * <pre>{@code
     * return species.resize(this);
     * }</pre>
     *
     * @param species the species
     * @param <T> the type of shape of the species
     * @return a vector transformed by shape
     * @see Species#resize(Vector)
     */
    public abstract <T extends Shape> Vector<E, T> resize(Species<E, T> species);

    // Cast

    /**
     * Converts this vector to a vector of the given species shape {@code T} and
     * element type {@code F}.
     * <p>
     * This method behaves as if it returns the result of calling
     * {@link Species#cast(Vector) cast} on the given species with this vector:
     * <pre>{@code
     * return species.cast(this);
     * }</pre>
     *
     * @param species the species
     * @param <F> the boxed element type of the species
     * @param <T> the type of shape of the species
     * @return a vector converted by shape and element type
     * @see Species#cast(Vector)
     */
    @ForceInline
    public <F, T extends Shape> Vector<F, T> cast(Species<F, T> species) {
        return species.cast(this);
    }

    //Array stores

    /**
     * Stores this vector into a byte array starting at an offset.
     * <p>
     * Bytes are extracted from primitive lane elements according to the
     * native byte order of the underlying platform.
     * <p>
     * This method behaves as it calls the
     * byte buffer, offset, and mask accepting
     * {@link #intoByteBuffer(ByteBuffer, int, Mask) method} as follows:
     * <pre>{@code
     * return this.intoByteBuffer(ByteBuffer.wrap(a), i, this.maskAllTrue());
     * }</pre>
     *
     * @param a the byte array
     * @param i the offset into the array
     * @return a vector loaded from a byte array
     * @throws IndexOutOfBoundsException if {@code i < 0} or
     * {@code i > a.length - (this.length() * this.elementSize() / Byte.SIZE)}
     */
    public abstract void intoByteArray(byte[] a, int i);

    /**
     * Stores this vector into a byte array starting at an offset and using a mask.
     * <p>
     * Bytes are extracted from primitive lane elements according to the
     * native byte order of the underlying platform.
     * <p>
     * This method behaves as it calls the
     * byte buffer, offset, and mask accepting
     * {@link #intoByteBuffer(ByteBuffer, int, Mask) method} as follows:
     * <pre>{@code
     * return this.intoByteBuffer(ByteBuffer.wrap(a), i, m);
     * }</pre>
     *
     * @param a the byte array
     * @param i the offset into the array
     * @throws IndexOutOfBoundsException if the offset is {@code < 0},
     * or {@code > a.length},
     * for any vector lane index {@code N} where the mask at lane {@code N}
     * is set
     * {@code i >= a.length - (N * this.elementSize() / Byte.SIZE)}
     */
    public abstract void intoByteArray(byte[] a, int i, Mask<E, S> m);

    /**
     * Stores this vector into a {@link ByteBuffer byte buffer} starting at the
     * buffer's position.
     * <p>
     * Bytes are extracted from primitive lane elements according to the
     * native byte order of the underlying platform.
     * <p>
     * This method behaves as if it calls the byte buffer, offset, and mask
     * accepting
     * {@link #intoByteBuffer(ByteBuffer, int, Mask)} method} as follows:
     * <pre>{@code
     *   this.intoByteBuffer(b, b.position(), this.maskAllTrue())
     * }</pre>
     *
     * @param b the byte buffer
     * @throws IndexOutOfBoundsException if there are fewer than
     * {@code this.length() * this.elementSize() / Byte.SIZE} bytes
     * remaining in the byte buffer
     */
    public abstract void intoByteBuffer(ByteBuffer b);

    /**
     * Stores this vector into a {@link ByteBuffer byte buffer} starting at the
     * buffer's position and using a mask.
     * <p>
     * Bytes are extracted from primitive lane elements according to the
     * native byte order of the underlying platform.
     * <p>
     * This method behaves as if it calls the byte buffer, offset, and mask
     * accepting
     * {@link #intoByteBuffer(ByteBuffer, int, Mask)} method} as follows:
     * <pre>{@code
     *   this.intoByteBuffer(b, b.position(), m)
     * }</pre>
     *
     * @param b the byte buffer
     * @param m the mask
     * @throws IndexOutOfBoundsException if for any vector lane index
     * {@code N} where the mask at lane {@code N} is set
     * {@code b.position() >= b.limit() - (N * this.elementSize() / Byte.SIZE)}
     */
    public abstract void intoByteBuffer(ByteBuffer b, Mask<E, S> m);

    /**
     * Stores this vector into a {@link ByteBuffer byte buffer} starting at an
     * offset into the byte buffer.
     * <p>
     * Bytes are extracted from primitive lane elements according to the
     * native byte order of the underlying platform.
     * <p>
     * This method behaves as if it calls the byte buffer, offset, and mask
     * accepting
     * {@link #intoByteBuffer(ByteBuffer, int, Mask)} method} as follows:
     * <pre>{@code
     *   this.intoByteBuffer(b, i, this.maskAllTrue())
     * }</pre>
     *
     * @param b the byte buffer
     * @param i the offset into the byte buffer
     * @throws IndexOutOfBoundsException if the offset is {@code < 0},
     * or {@code > b.limit()},
     * or if there are fewer than
     * {@code this.length() * this.elementSize() / Byte.SIZE} bytes
     * remaining in the byte buffer from the given offset
     */
    public abstract void intoByteBuffer(ByteBuffer b, int i);

    /**
     * Stores this vector into a {@link ByteBuffer byte buffer} starting at an
     * offset into the byte buffer and using a mask.
     * <p>
     * This method behaves as if the byte buffer is viewed as a primitive
     * {@link java.nio.Buffer buffer} for the primitive element type,
     * according to the native byte order of the underlying platform, and
     * the lane elements of this vector are put into the buffer if the
     * corresponding mask lane is set.
     * The following pseudocode expresses the behaviour, where
     * {@coce EBuffer} is the primitive buffer type, {@code e} is the
     * primitive element type, and {@code EVector<S>} is the primitive
     * vector type for this vector:
     * <pre>{@code
     * EBuffer eb = b.duplicate().
     *     order(ByteOrder.nativeOrder()).position(i).
     *     asEBuffer();
     * e[] es = ((EVector<S>)this).toArray();
     * for (int n = 0; n < t.length && m.isSet(n); n++) {
     *     eb.put(n, es[n]);
     * }
     * }</pre>
     *
     * @param b the byte buffer
     * @param i the offset into the byte buffer
     * @param m the mask
     * @throws IndexOutOfBoundsException if the offset is {@code < 0},
     * or {@code > b.limit()},
     * for any vector lane index {@code N} where the mask at lane {@code N}
     * is set
     * {@code i >= b.limit() - (N * this.elementSize() / Byte.SIZE)} bytes
     */
    public abstract void intoByteBuffer(ByteBuffer b, int i, Mask<E, S> m);


    /**
     * A factory for creating {@link Vector}, {@link Mask} and {@link Shuffle}
     * values of the same element type and shape.
     *
     * @param <E> the boxed element type of this species
     * @param <S> the type of shape of this species
     */
    public static abstract class Species<E, S extends Shape> {
        Species() {}

        /**
         * Returns the primitive element type of vectors produced by this
         * species.
         *
         * @return the primitive element type
         */
        public abstract Class<E> elementType();

        /**
         * Returns the element size, in bits, of vectors produced by this
         * species.
         *
         * @return the element size, in bits
         */
        public abstract int elementSize();

        /**
         * Returns the shape of masks, shuffles, and vectors produced by this
         * species.
         *
         * @return the primitive element type
         */
        public abstract S shape();

        /**
         * Returns the mask, shuffe, or vector lanes produced by this species.
         *
         * @return the the number of lanes
         */
        public int length() { return shape().length(this); }

        /**
         * Returns the total vector size, in bits, of vectors produced by this
         * species.
         *
         * @return the total vector size, in bits
         */
        public int bitSize() { return shape().bitSize(); }

        // Factory

        /**
         * Returns a vector where all lane elements are set to the default
         * primitive value.
         *
         * @return a zero vector
         */
        public abstract Vector<E, S> zero();

        /**
         * Loads a vector from a byte array starting at an offset.
         * <p>
         * Bytes are composed into primitive lane elements according to the
         * native byte order of the underlying platform
         * <p>
         * This method behaves as if it returns the result of calling the
         * byte buffer, offset, and mask accepting
         * {@link #fromByteBuffer(ByteBuffer, int, Mask) method} as follows:
         * <pre>{@code
         * return this.fromByteBuffer(ByteBuffer.wrap(a), i, this.maskAllTrue());
         * }</pre>
         *
         * @param a the byte array
         * @param i the offset into the array
         * @return a vector loaded from a byte array
         * @throws IndexOutOfBoundsException if {@code i < 0} or
         * {@code i > a.length - (this.length() * this.elementSize() / Byte.SIZE)}
         */
        public abstract Vector<E, S> fromByteArray(byte[] a, int i);

        /**
         * Loads a vector from a byte array starting at an offset and using a
         * mask.
         * <p>
         * Bytes are composed into primitive lane elements according to the
         * native byte order of the underlying platform.
         * <p>
         * This method behaves as if it returns the result of calling the
         * byte buffer, offset, and mask accepting
         * {@link #fromByteBuffer(ByteBuffer, int, Mask) method} as follows:
         * <pre>{@code
         * return this.fromByteBuffer(ByteBuffer.wrap(a), i, m);
         * }</pre>
         *
         * @param a the byte array
         * @param i the offset into the array
         * @param m the mask
         * @return a vector loaded from a byte array
         * @throws IndexOutOfBoundsException if {@code i < 0} or
         * {@code i > a.length - (this.length() * this.elementSize() / Byte.SIZE)}
         * @throws IndexOutOfBoundsException if the offset is {@code < 0},
         * or {@code > a.length},
         * for any vector lane index {@code N} where the mask at lane {@code N}
         * is set
         * {@code i >= a.length - (N * this.elementSize() / Byte.SIZE)}
         */
        public abstract Vector<E, S> fromByteArray(byte[] a, int i, Mask<E, S> m);

        /**
         * Loads a vector from a {@link ByteBuffer byte buffer} starting at the
         * buffer's position.
         * <p>
         * Bytes are composed into primitive lane elements according to the
         * native byte order of the underlying platform.
         * <p>
         * This method behaves as if it returns the result of calling the
         * byte buffer, offset, and mask accepting
         * {@link #fromByteBuffer(ByteBuffer, int, Mask) method} as follows:
         * <pre>{@code
         *   return this.fromByteBuffer(b, b.position(), this.maskAllTrue())
         * }</pre>
         *
         * @param b the byte buffer
         * @return a vector loaded from a byte buffer
         * @throws IndexOutOfBoundsException if there are fewer than
         * {@code this.length() * this.elementSize() / Byte.SIZE} bytes
         * remaining in the byte buffer
         */
        public abstract Vector<E, S> fromByteBuffer(ByteBuffer b);

        /**
         * Loads a vector from a {@link ByteBuffer byte buffer} starting at the
         * buffer's position and using a mask.
         * <p>
         * Bytes are composed into primitive lane elements according to the
         * native byte order of the underlying platform.
         * <p>
         * This method behaves as if it returns the result of calling the
         * byte buffer, offset, and mask accepting
         * {@link #fromByteBuffer(ByteBuffer, int, Mask)} method} as follows:
         * <pre>{@code
         *   return this.fromByteBuffer(b, b.position(), m)
         * }</pre>
         *
         * @param b the byte buffer
         * @param m the mask
         * @return a vector loaded from a byte buffer
         * @throws IndexOutOfBoundsException if for any vector lane index
         * {@code N} where the mask at lane {@code N} is set
         * {@code b.position() >= b.limit() - (N * this.elementSize() / Byte.SIZE)}
         */
        public abstract Vector<E, S> fromByteBuffer(ByteBuffer b, Mask<E, S> m);

        /**
         * Loads a vector from a {@link ByteBuffer byte buffer} starting at an
         * offset into the byte buffer.
         * <p>
         * Bytes are composed into primitive lane elements according to the
         * native byte order of the underlying platform.
         * <p>
         * This method behaves as if it returns the result of calling the
         * byte buffer, offset, and mask accepting
         * {@link #fromByteBuffer(ByteBuffer, int, Mask)} method} as follows:
         * <pre>{@code
         *   return this.fromByteBuffer(b, i, this.maskAllTrue())
         * }</pre>
         *
         * @param b the byte buffer
         * @param i the offset into the byte buffer
         * @return a vector loaded from a byte buffer
         * @throws IndexOutOfBoundsException if the offset is {@code < 0},
         * or {@code > b.limit()},
         * or if there are fewer than
         * {@code this.length() * this.elementSize() / Byte.SIZE} bytes
         * remaining in the byte buffer from the given offset
         */
        public abstract Vector<E, S> fromByteBuffer(ByteBuffer b, int i);

        /**
         * Loads a vector from a {@link ByteBuffer byte buffer} starting at an
         * offset into the byte buffer and using a mask.
         * <p>
         * This method behaves as if the byte buffer is viewed as a primitive
         * {@link java.nio.Buffer buffer} for the primitive element type,
         * according to the native byte order of the underlying platform, and
         * the returned vector is loaded with a mask from a primitive array
         * obtained from the primitive buffer.
         * The following pseudocode expresses the behaviour, where
         * {@coce EBuffer} is the primitive buffer type, {@code e} is the
         * primitive element type, and {@code ESpecies<S>} is the primitive
         * species for {@code e}:
         * <pre>{@code
         * EBuffer eb = b.duplicate().
         *     order(ByteOrder.nativeOrder()).position(i).
         *     asEBuffer();
         * e[] es = new e[this.length()];
         * for (int n = 0; n < t.length && m.isSet(n); n++) {
         *     es[n] = eb.get(n);
         * }
         * Vector<E, S> r = ((ESpecies<S>)this).fromArray(es, 0, m);
         * }</pre>
         *
         * @param b the byte buffer
         * @param i the offset into the byte buffer
         * @return a vector loaded from a byte buffer
         * @throws IndexOutOfBoundsException if the offset is {@code < 0},
         * or {@code > b.limit()},
         * for any vector lane index {@code N} where the mask at lane {@code N}
         * is set
         * {@code i >= b.limit() - (N * this.elementSize() / Byte.SIZE)}
         */
        public abstract Vector<E, S> fromByteBuffer(ByteBuffer b, int i, Mask<E, S> m);

        //Mask and shuffle constructions

        /**
         * Returns a mask where each lane is set or unset according to a given
         * {@code boolean} value.
         * <p>
         * For each mask lane, where {@code N} is the mask lane index,
         * if the given {@code boolean} value at index {@code N} is {@code true}
         * then the mask lane at index {@code N} is set, otherwise it is unset.
         *
         * @@@ What should happen if bits.length < this.length() ? use the
         * default value or throw IndexOutOfBoundsException
         *
         * @param bits the given {@code boolean} values
         * @return a mask where each lane is set or unset according to a given
         * {@code boolean} value
         */
        public abstract Mask<E, S> maskFromValues(boolean... bits);

        /**
         * Loads a mask from a {@code boolean} array starting at an offset.
         * <p>
         * For each mask lane, where {@code N} is the mask lane index,
         * if the array element at index {@code i + N} is {@code true} then the
         * mask lane at index {@code N} is set, otherwise it is unset.
         *
         * @param a the {@code boolean} array
         * @param i the offset into the array
         * @return the mask loaded from a {@code boolean} array
         * @throws IndexOutOfBoundsException if {@code i < 0}, or
         * {@code i > a.length - this.length()}
         */
        public abstract Mask<E, S> maskFromArray(boolean[] a, int i);

        /**
         * Returns a mask where all lanes are a set.
         *
         * @return a mask where all lanes are a set
         */
        public abstract Mask<E, S> maskAllTrue();

        /**
         * Returns a mask where all lanes are unset.
         *
         * @return a mask where all lanes are unset
         */
        public abstract Mask<E, S> maskAllFalse();

        /**
         * Returns a shuffle where each lane element to a given {@code int}
         * value.
         * <p>
         * For each shuffle lane, where {@code N} is the shuffle lane index, the
         * the {@code int} value at index {@code N} is placed into the resulting
         * shuffle at lane index {@code N}.
         *
         * @@@ What should happen if indexes.length < this.length() ? use the
         * default value or throw IndexOutOfBoundsException
         *
         * @param indexes the given {@code int} values
         * @return a shufle where each lane element is set to a given
         * {@code int} value
         */
        public abstract Shuffle<E, S> shuffleFromValues(int... indexes);

        /**
         * Loads a shuffle from an {@code int} array starting at offset.
         * <p>
         * For each shuffle lane, where {@code N} is the shuffle lane index, the
         * array element at index {@code i + N} is placed into the
         * resulting shuffle at lane index {@code N}.
         *
         * @param a the {@code int} array
         * @param i the offset into the array
         * @return the shuffle loaded from an {@code int} array
         * @throws IndexOutOfBoundsException if {@code i < 0}, or
         * {@code i > a.length - this.length()}
         */
        public abstract Shuffle<E, S> shuffleFromArray(int[] a, int i);

        /**
         * Returns a shuffle containing lane elements of an {@code int}
         * vector.
         * <p>
         * For each vector lane, where {@code N} is the vector lane index, the
         * lane element at index {@code N} is placed into the resulting shuffle
         * at lane index {@code N}.
         *
         * @param v the {@code int} vector
         * @return a shuffle containing lane elements of an {@code int} vector
         */
        public abstract Shuffle<E, S> shuffleFromVector(Vector<Integer, S> v);

        // Shuffle iota, 0...N

        // Vector type/shape transformations

        /**
         * Transforms an input vector of shape {@code T} and element type
         * {@code F} to a vector of this species shape {@code S} and element
         * type {@code E}.
         * <p>
         * The underlying bits of the input vector are copied to the resulting
         * vector without modification, but those bits, before copying, may be
         * truncated if the vector bit size is greater than this species bit
         * size, or appended to with zero bits if the vector bit size is less
         * than this species bit size.
         * <p>
         * The method behaves as if the input vector is stored into a byte array
         * and then the returned vector is loaded from the byte array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int alen = Math.max(v.bitSize(), this.bitSize()) / Byte.SIZE;
         * byte[] a = new byte[alen];
         * v.intoByteArray(a, 0);
         * return this.fromByteArray(a, 0);
         * }</pre>
         *
         * @param v the input vector
         * @param <F> the boxed element type of the vector
         * @param <T> the type of shape of the vector
         * @return a vector transformed, by shape and element type, from an
         * input vector
         */
        public abstract <F, T extends Shape> Vector<E, S> reshape(Vector<F, T> v);

        /**
         * Transforms an input vector of element type {@code F} to a vector of
         * this species element type {@code E}, where the this species shape
         * {@code S} is preserved.
         * <p>
         * The underlying bits of the input vector are copied without
         * modification to the resulting vector.
         * <p>
         * The method behaves as if the input vector is stored into a byte array
         * and then the returned vector is loaded from the byte array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * byte[] a = new byte[v.bitSize() / Byte.SIZE];
         * v.intoByteArray(a, 0);
         * return this.fromByteArray(a, 0);
         * }</pre>
         *
         * @param v the input vector
         * @param <F> the boxed element type of the vector
         * @return a vector transformed, by element type, from an input vector
         */
        public abstract <F> Vector<E, S> rebracket(Vector<F, S> v);

        /**
         * Transforms an input vector of shape {@code T} to a vector of this
         * species shape {@code S}, where the this species element type
         * {@code E} is preserved.
         * <p>
         * The lane elements of the input vector are copied without
         * modification to the resulting vector, but those lane elements, before
         * copying, may be truncated if the vector length is greater than this
         * species length, or appended to with default element values if the
         * vector length is less than this species length.
         * <p>
         * The method behaves as if the input vector is stored into a byte array
         * and then the returned vector is loaded from the byte array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int alen = Math.max(v.bitSize(), this.bitSize()) / Byte.SIZE;
         * byte[] a = new byte[alen];
         * v.intoByteArray(a, 0);
         * return fromByteArray(a, 0);
         * }</pre>
         *
         * @param v the input vector
         * @param <T> the type of shape of the vector
         * @return a vector transformed, by shape, from an input vector
         */
        public abstract <T extends Shape> Vector<E, S> resize(Vector<E, T> v);

        /**
         * Converts an input vector of shape {@code T} and element type
         * {@code F} to a vector of this species shape {@code S} and element
         * type {@code E}.
         * <p>
         * For each input vector lane up to the length of the input vector or
         * this species, which ever is the minimum, and where {@code N} is the
         * vector lane index, the element at index {@code N} of primitive type
         * {@code F} is converted, according to primitive conversion rules
         * specified by the Java Language Specification, to a value of primitive
         * type {@code E} and placed into the resulting vector at lane index
         * {@code N}.  If this species length is greater than the input
         * vector length then the default primitive value is placed into
         * subsequent lanes of the resulting vector.
         *
         * @param v the input vector
         * @param <F> the boxed element type of the vector
         * @param <T> the type of shape of the vector
         * @return a vector, converted by shape and element type, from an input
         * vector.
         */
        public abstract <F, T extends Shape> Vector<E, S> cast(Vector<F, T> v);


        // Mask type/shape transformations

        /**
         * Transforms an input mask of shape {@code T} and element type
         * {@code F} to a vector of this species shape {@code S} and element
         * type {@code E}.
         * <p>
         * The lane elements of the input mask are copied to the resulting
         * mask without modification, but those lane elements, before copying,
         * may be truncated if the mask length is greater than this species
         * length, or appended to with false values if the mask length less than
         * this species length.
         * <p>
         * The method behaves as if the input mask is stored into a boolean
         * array and then the returned mask is loaded from the boolean array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int alen = Math.max(m.length(), this.length());
         * boolean[] a = new boolean[alen];
         * m.intoArray(a, 0);
         * return this.maskFromArray(a, 0);
         * }</pre>
         *
         * @param m the input mask
         * @param <F> the boxed element type of the mask
         * @param <T> the type of shape of the mask
         * @return a mask transformed, by shape and element type, from an
         * input mask
         */
        public <F, T extends Shape> Mask<E, S> reshape(Mask<F, T> m) {
            return maskFromValues(m.toArray());
        }

        /**
         * Transforms an input mask of element type {@code F} to a mask of
         * this species element type {@code E}, where the this species shape
         * {@code S} is preserved.
         * <p>
         * The lane elements of the input mask are copied without
         * modification to the resulting mask.
         * <p>
         * The method behaves as if the input mask is stored into a boolean
         * array and then the returned mask is loaded from the boolean array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * boolean[] a = new byte[m.length()];
         * m.intoArray(a, 0);
         * return this.maskFromArray(a, 0);
         * }</pre>
         *
         * @param m the input mask
         * @param <F> the boxed element type of the mask
         * @return a mask transformed, by element type, from an input mask
         */
        @ForceInline
        public <F> Mask<E, S> rebracket(Mask<F, S> m) {
            return reshape(m);
        }

        /**
         * Transforms an input mask of shape {@code T} to a mask of this
         * species shape {@code S}, where the this species element type
         * {@code E} is preserved.
         * <p>
         * The lane elements of the input mask are copied to the resulting
         * mask without modification, but those lane elements, before copying,
         * may be truncated if the mask length is greater than this species
         * length, or appended to with false values if the mask length less than
         * this species length.
         * <p>
         * The method behaves as if the input mask is stored into a boolean
         * array and then the returned mask is loaded from the boolean array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int alen = Math.max(m.length(), this.length());
         * boolean[] a = new boolean[alen];
         * m.intoArray(a, 0);
         * return this.maskFromArray(a, 0);
         * }</pre>
         *
         * @param m the input mask
         * @param <T> the type of shape of the mask
         * @return a mask transformed, by shape, from an input mask
         */
        @ForceInline
        public <T extends Shape> Mask<E, S> resize(Mask<E, T> m) {
            return reshape(m);
        }


        // Shuffle type/shape transformations

        /**
         * Transforms an input shuffle of shape {@code T} and element type
         * {@code F} to a shuffle of this species shape {@code S} and element
         * type {@code E}.
         * <p>
         * The lane elements of the input shuffle are copied to the resulting
         * shuffle without modification, but those lane elements, before copying,
         * may be truncated if the shuffle length is greater than this species
         * length, or appended to with zero values if the shuffle length less than
         * this species length.
         * <p>
         * The method behaves as if the input shuffle is stored into a int
         * array and then the returned shuffle is loaded from the int array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int alen = Math.max(s.length(), this.length());
         * int[] a = new int[blen];
         * s.intoArray(a, 0);
         * return this.shuffleFromArray(a, 0);
         * }</pre>
         *
         * @param s the input shuffle
         * @param <F> the boxed element type of the shuffle
         * @param <T> the type of shape of the shuffle
         * @return a shuffle transformed, by shape and element type, from an
         * input shuffle
         */
        public <F, T extends Shape> Shuffle<E, S> reshape(Shuffle<F, T> s) {
            return shuffleFromValues(s.toArray());
        }

        /**
         * Transforms an input shuffle of element type {@code F} to a shuffle of
         * this species element type {@code E}, where the this species shape
         * {@code S} is preserved.
         * <p>
         * The lane elements of the input shuffle are copied without
         * modification to the resulting shuffle.
         * <p>
         * The method behaves as if the input shuffle is stored into a int
         * array and then the returned shuffle is loaded from the int array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int[] a = new byte[s.length()];
         * s.intoArray(a, 0);
         * return this.shuffleFromArray(a, 0);
         * }</pre>
         *
         * @param s the input shuffle
         * @param <F> the boxed element type of the shuffle
         * @return a shuffle transformed, by element type, from an input shuffle
         */
        @ForceInline
        public <F> Shuffle<E, S> rebracket(Shuffle<F, S> s) {
            return reshape(s);
        }

        /**
         * Transforms an input shuffle of shape {@code T} to a shuffle of this
         * species shape {@code S}, where the this species element type
         * {@code E} is preserved.
         * <p>
         * The lane elements of the input shuffle are copied to the resulting
         * shuffle without modification, but those lane elements, before copying,
         * may be truncated if the shuffle length is greater than this species
         * length, or appended to with zero values if the shuffle length less than
         * this species length.
         * <p>
         * The method behaves as if the input shuffle is stored into a int
         * array and then the returned shuffle is loaded from the int array.
         * The following pseudocode expresses the behaviour:
         * <pre>{@code
         * int alen = Math.max(m.length(), this.length());
         * boolean[] a = new boolean[alen];
         * m.intoArray(a, 0);
         * return this.maskFromArray(a, 0);
         * }</pre>
         *
         * @param s the input shuffle
         * @param <T> the type of shape of the shuffle
         * @return a shuffle transformed, by shape, from an input shuffle
         */
        @ForceInline
        public <T extends Shape> Shuffle<E, S> resize(Shuffle<E, T> s) {
            return reshape(s);
        }


        // Species/species transformations

        // Returns a species for a given element type and the length of this
        // species.
        // The length of the returned species will be equal to the length of
        // this species.
        //
        // Throws IAE if no shape exists for the element type and this species length,
//        public <F> Species<F, ?> toSpeciesWithSameNumberOfLanes(Class<F> c) {
//            // @@@ TODO implement and find better name
//            throw new UnsupportedOperationException();
//        }

    }

    /**
     * A {@code Shape} governs the total size, in bits, of a
     * {@link Vector}, {@link Mask}, or {@code Shuffle}.  The shape in
     * combination with the element type together govern the number of lanes.
     */
    public static abstract class Shape {
        Shape() {}

        /**
         * Returns the size, in bits, of this shape.
         *
         * @return the size, in bits, of this shape.
         */
        public abstract int bitSize();

        // @@@ remove this method
        public int length(Species<?, ?> s) { return bitSize() / s.elementSize(); }
    }

    /**
     * A {@code Mask} represents an ordered immutable sequence of {@code boolean}
     * values.  A Mask can be used with a mask accepting vector operation to
     * control the selection and operation of lane elements of input vectors.
     * <p>
     * The number of values in the sequence is referred to as the Mask
     * {@link #length() length}.  The length also corresponds to the number of
     * Mask lanes.  The lane element at lane index {@code N} (from {@code 0},
     * inclusive, to length, exclusive) corresponds to the {@code N + 1}'th
     * value in the sequence.
     * A Mask and Vector of the same element type and shape have the same number
     * of lanes.
     * <p>
     * A lane is said to be <em>set</em> if the lane element is {@code true},
     * otherwise a lane is said to be <em>unset</em> if the lane element is
     * {@code false}.
     * <p>
     * Mask declares a limited set of unary, binary and reductive mask
     * operations.
     * <ul>
     * <li>
     * A mask unary operation (1-ary) operates on one input mask to produce a
     * result mask.
     * For each lane of the input mask the
     * lane element is operated on using the specified scalar unary operation and
     * the boolean result is placed into the mask result at the same lane.
     * The following pseudocode expresses the behaviour of this operation category:
     *
     * <pre>{@code
     * Mask<E, S> a = ...;
     * boolean[] ar = new boolean[a.length()];
     * for (int i = 0; i < a.length(); i++) {
     *     ar[i] = boolean_unary_op(a.isSet(i));
     * }
     * Mask<E, S> r = a.species().maskFromArray(ar, 0);
     * }</pre>
     *
     * <li>
     * A mask binary operation (2-ary) operates on two input
     * masks to produce a result mask.
     * For each lane of the two input masks,
     * a and b say, the corresponding lane elements from a and b are operated on
     * using the specified scalar binary operation and the boolean result is placed
     * into the mask result at the same lane.
     * The following pseudocode expresses the behaviour of this operation category:
     *
     * <pre>{@code
     * Mask<E, S> a = ...;
     * Mask<E, S> b = ...;
     * boolean[] ar = new boolean[a.length()];
     * for (int i = 0; i < a.length(); i++) {
     *     ar[i] = scalar_binary_op(a.isSet(i), b.isSet(i));
     * }
     * Mask<E, S> r = a.species().maskFromArray(ar, 0);
     * }</pre>
     *
     * @param <E> the boxed element type of this mask
     * @param <S> the type of shape of this mask
     */
    public static abstract class Mask<E, S extends Shape> {
        Mask() {}

        /**
         * Returns the species of this mask.
         *
         * @return the species of this mask
         */
        public abstract Species<E, S> species();

        /**
         * Returns the number of mask lanes (the length).
         *
         * @return the number of mask lanes
         */
        public int length() { return species().length(); }

        /**
         * Returns the lane elements of this mask packed into a {@code long}
         * value for at most the first 64 lane elements.
         * <p>
         * The lane elements are packed in the order of least significant bit
         * to most significant bit.
         * For each mask lane where {@code N} is the mask lane index, if the
         * mask lane is set then the {@code N}'th bit is set to one in the
         * resulting {@code long} value, otherwise the {@code N}'th bit is set
         * to zero.
         *
         * @return the lane elements of this mask packed into a {@code long}
         * value.
         */
        public abstract long toLong();

        /**
         * Returns an {@code boolean} array containing the lane elements of this
         * mask.
         * <p>
         * This method behaves as if it {@link #intoArray(boolean[], int)} stores}
         * this mask into an allocated array and returns that array as
         * follows:
         * <pre>{@code
         * boolean[] a = new boolean[this.length()];
         * this.intoArray(a, 0);
         * return a;
         * }</pre>
         *
         * @return an array containing the the lane elements of this vector
         */
        public abstract boolean[] toArray();

        /**
         * Stores this mask into a {@code boolean} array starting at offset.
         * <p>
         * For each mask lane, where {@code N} is the mask lane index,
         * the lane element at index {@code N} is stored into the array at index
         * {@code i + N}.
         *
         * @param a the array
         * @param i the offset into the array
         * @throws IndexOutOfBoundsException if {@code i < 0}, or
         * {@code i > a.length - this.length()}
         */
        public abstract void intoArray(boolean[] a, int i);

        /**
         * Returns {@code true} if any of the mask lanes are set.
         *
         * @return {@code true} if any of the mask lanes are set, otherwise
         * {@code false}.
         */
        public abstract boolean anyTrue();

        /**
         * Returns {@code true} if all of the mask lanes are set.
         *
         * @return {@code true} if all of the mask lanes are set, otherwise
         * {@code false}.
         */
        public abstract boolean allTrue();

        /**
         * Returns the number of mask lanes that are set.
         *
         * @return the number of mask lanes that are set.
         */
        public abstract int trueCount();

        // TODO: LZ count/numberOfLeadingZeros
        // TODO: xor, shiftl, shiftr

        /**
         * Logically ands this mask with an input mask.
         * <p>
         * This is a mask binary operation where the logical and operation
         * ({@code &&} is applied to lane elements.
         *
         * @param o the input mask
         * @return the result of logically and'ing this mask with an input mask
         */
        public abstract Mask<E, S> and(Mask<E, S> o);

        /**
         * Logically ors this mask with an input mask.
         * <p>
         * This is a mask binary operation where the logical or operation
         * ({@code ||} is applied to lane elements.
         *
         * @param o the input mask
         * @return the result of logically or'ing this mask with an input mask
         */
        public abstract Mask<E, S> or(Mask<E, S> o);

        /**
         * Logically negates this mask.
         * <p>
         * This is a mask unary operation where the logical not operation
         * ({@code !} is applied to lane elements.
         *
         * @return the result of logically negating this mask.
         */
        public abstract Mask<E, S> not();

        /**
         * Returns a vector representation of this mask.
         * <p>
         * For each mask lane, where {@code N} is the mask lane index,
         * if the mask lane is set then an element value whose most significant
         * bit is set is placed into the resulting vector at lane index
         * {@code N}, otherwise the default element value is placed into the
         * resulting vector at lane index {@code N}.
         *
         * @return a vector representation of this mask.
         */
        public abstract Vector<E, S> toVector();

        /**
         * Tests if the lane at index {@code i} is set
         * @param i the lane index
         *
         * @return true if the lane at index {@code i} is set, otherwise false
         */
        // @@@ Rename to isSet
        public abstract boolean getElement(int i);

        /**
         * Transforms this mask to a mask of the given species shape {@code T}
         * and element type {@code F}.
         * <p>
         * This method behaves as if it returns the result of calling
         * {@link Species#reshape(Mask) reshape} on the given species with this
         * mask:
         * <pre>{@code
         * return species.reshape(this);
         * }</pre>
         *
         * @param species the species
         * @param <F> the boxed element type of the species
         * @param <T> the type of shape of the species
         * @return a mask transformed by shape and element type
         * @see Species#reshape(Mask)
         */
        @ForceInline
        public <F, T extends Shape> Mask<F, T> reshape(Species<F, T> species) {
            return species.reshape(this);
        }

        /**
         * Transforms this mask to a mask of the given species element type
         * {@code F}, where this mask's shape {@code S} is preserved.
         * <p>
         * This method behaves as if it returns the result of calling
         * {@link Species#rebracket(Mask) rebracket} on the given species with
         * this mask:
         * <pre>{@code
         * return species.rebracket(this);
         * }</pre>
         *
         * @param species the species
         * @param <F> the boxed element type of the species
         * @return a mask transformed element type
         * @see Species#rebracket(Mask)
         */
        @ForceInline
        public <F> Mask<F, S> rebracket(Species<F, S> species) {
            return species.reshape(this);
        }

        /**
         * Transforms this mask to a mask of the given species shape {@code T},
         * where this mask's element type {@code E} is preserved.
         * <p>
         * This method behaves as if it returns the result of calling
         * {@link Species#resize(Mask) resize} on the given species with this
         * mask:
         * <pre>{@code
         * return species.resize(this);
         * }</pre>
         *
         * @param species the species
         * @param <T> the type of shape of the species
         * @return a mask transformed by shape
         * @see Species#resize(Mask)
         */
        @ForceInline
        public <T extends Shape> Mask<E, T> resize(Species<E, T> species) {
            return species.reshape(this);
        }
    }

    /**
     * A {@code Shuffle} represents an ordered immutable sequence of
     * {@code int} values.  A Shuffle can be used with a shuffle accepting
     * vector operation to control the rearrangement of lane elements of input
     * vectors
     * <p>
     * The number of values in the sequence is referred to as the Shuffle
     * {@link #length() length}.  The length also corresponds to the number of
     * Shuffle lanes.  The lane element at lane index {@code N} (from {@code 0},
     * inclusive, to length, exclusive) corresponds to the {@code N + 1}'th
     * value in the sequence.
     * A Shuffle and Vector of the same element type and shape have the same
     * number of lanes.
     * <p>
     * A {@code Shuffle<E, S>} is a specialized and limited form of an
     * {@code IntVector<S>} where the Shuffle's lane elements correspond to
     * lane index values.
     * A Shuffle describes how a lane element of a vector may cross lanes from
     * its lane index, {@code i} say, to another lane index whose value is the
     * Shuffle's lane element at lane index {@code i}.
     *
     * @param <E> the boxed element type of this mask
     * @param <S> the type of shape of this mask
     */
    public static abstract class Shuffle<E, S extends Shape> {
        Shuffle() {}

        /**
         * Returns the species of this shuffle.
         *
         * @return the species of this shuffle
         */
        public abstract Species<E, S> species();

        /**
         * Returns the specialized {@code int} species of this shuffle.
         *
         * @return the specialized {@code int} species of this shuffle
         * @see #toVector
         */
        public abstract IntVector.IntSpecies<S> intSpecies();

        /**
         * Returns the number of shuffle lanes (the length).
         *
         * @return the number of shuffle lanes
         */
        public int length() { return species().length(); }

        /**
         * Returns an {@code int} array containing the lane elements of this
         * shuffle.
         * <p>
         * This method behaves as if it {@link #intoArray(int[], int)} stores}
         * this shuffle into an allocated array and returns that array as
         * follows:
         * <pre>{@code
         *   int[] a = new int[this.length()];
         *   this.intoArray(a, 0);
         *   return a;
         * }</pre>
         *
         * @return an array containing the the lane elements of this vector
         */
        public abstract int[] toArray();

        /**
         * Stores this shuffle into an {@code int} array starting at offset.
         * <p>
         * For each shuffle lane, where {@code N} is the shuffle lane index,
         * the lane element at index {@code N} is stored into the array at index
         * {@code i + N}.
         *
         * @param a the array
         * @param i the offset into the array
         * @throws IndexOutOfBoundsException if {@code i < 0}, or
         * {@code i > a.length - this.length()}
         */
        public abstract void intoArray(int[] a, int i);

        // @@@ rotate/shift/EL/ER

        /**
         * Returns an {@link IntVector}, of the same shape as this shuffle,
         * containing the lane elements of this shuffle.
         * <p>
         * This method behaves as if it returns the result of creating an
         * {@link IntVector} given this shuffle's {@code int} species and an
         * {@code int} array obtained from this shuffle's lane elements, as
         * follows:
         * <pre>{@code
         *   int[] a = this.toArray();
         *   return this.intSpecies().fromArray(a, 0);
         * }</pre>
         *
         * @return an {@link IntVector}, of the same shape as this shuffle,
         * containing the lane elements of this shuffle
         */
        public abstract IntVector<S> toVector();

        /**
         * Gets the {@code int} lane element at lane index {@code i}
         *
         * @param i the lane index
         * @return the {@code int} lane element at lane index {@code i}
         */
        public int getElement(int i) { return toArray()[i]; }

        /**
         * Transforms this shuffle to a shuffle of the given species shape {@code T}
         * and element type {@code F}.
         * <p>
         * This method behaves as if it returns the result of calling
         * {@link Species#reshape(Shuffle) reshape} on the given species with this
         * shuffle:
         * <pre>{@code
         * return species.reshape(this);
         * }</pre>
         *
         * @param species the species
         * @param <F> the boxed element type of the species
         * @param <T> the type of shape of the species
         * @return a shuffle transformed by shape and element type
         * @see Species#reshape(Shuffle)
         */
        @ForceInline
        public <F, T extends Shape> Shuffle<F, T> reshape(Species<F, T> species) {
            return species.reshape(this);
        }

        /**
         * Transforms this shuffle to a shuffle of the given species element type
         * {@code F}, where this shuffle's shape {@code S} is preserved.
         * <p>
         * This method behaves as if it returns the result of calling
         * {@link Species#rebracket(Shuffle) rebracket} on the given species with this
         * shuffle:
         * <pre>{@code
         * return species.rebracket(this);
         * }</pre>
         *
         * @param species the species
         * @param <F> the boxed element type of the species
         * @return a shuffle transformed element type
         * @see Species#rebracket(Shuffle)
         */
        @ForceInline
        public <F> Shuffle<F, S> rebracket(Species<F, S> species) {
            return species.reshape(this);
        }

        /**
         * Transforms this shuffle to a shuffle of the given species shape {@code T},
         * where this shuffle's element type {@code E} is preserved.
         * <p>
         * This method behaves as if it returns the result of calling
         * {@link Species#resize(Shuffle) resize} on the given species with this shuffle:
         * <pre>{@code
         * return species.resize(this);
         * }</pre>
         *
         * @param species the species
         * @param <T> the type of shape of the species
         * @return a shuffle transformed by shape
         * @see Species#resize(Shuffle)
         */
        @ForceInline
        public <T extends Shape> Shuffle<E, T> resize(Species<E, T> species) {
            return species.reshape(this);
        }
    }

    /**
     * Finds a preferred species for an element type.
     * <p>
     * A preferred species is a species chosen by the platform that has a
     * shape of maximal bit size.  A preferred species for different element
     * types will have the same shape, and therefore vectors created from
     * such species will be shape compatible.
     *
     * @param c the element type
     * @param <E> the boxed element type
     * @return a preferred species for an element type
     * @throws IllegalArgumentException if no such species exists for the
     * element type
     */
    @SuppressWarnings("unchecked")
    public static <E> Vector.Species<E, ?> preferredSpeciesInstance(Class<E> c) {
        Unsafe u = Unsafe.getUnsafe();

        int vectorLength = u.getMaxVectorSize(c);
        int vectorBitSize = bitSizeForVectorLength(c, vectorLength);
        Shape s = shapeForVectorBitSize(vectorBitSize);
        return speciesInstance(c, s);
    }

    // @@@ public static method on Species?
    private static int bitSizeForVectorLength(Class<?> c, int elementSize) {
        if (c == float.class) {
            return Float.SIZE * elementSize;
        }
        else if (c == double.class) {
            return Double.SIZE * elementSize;
        }
        else if (c == byte.class) {
            return Byte.SIZE * elementSize;
        }
        else if (c == short.class) {
            return Short.SIZE * elementSize;
        }
        else if (c == int.class) {
            return Integer.SIZE * elementSize;
        }
        else if (c == long.class) {
            return Long.SIZE * elementSize;
        }
        else {
            throw new IllegalArgumentException("Bad vector type: " + c.getName());
        }
    }

    // @@@ public static method on Shape?
    private static Shape shapeForVectorBitSize(int bitSize) {
        switch (bitSize) {
            case 64:
                return Shapes.S_64_BIT;
            case 128:
                return Shapes.S_128_BIT;
            case 256:
                return Shapes.S_256_BIT;
            case 512:
                return Shapes.S_512_BIT;
            default:
                throw new IllegalArgumentException("Bad vector bit size: " + bitSize);
        }
    }

    /**
     * Finds a species for an element type and shape.
     *
     * @param c the element type
     * @param s the shape
     * @param <E> the boxed element type
     * @param <S> the type of shape
     * @return a species for an element type and shape
     * @throws IllegalArgumentException if no such species exists for the
     * element type and/or shape
     */
    @SuppressWarnings("unchecked")
    public static <E, S extends Shape> Vector.Species<E, S> speciesInstance(Class<E> c, S s) {
        if (c == float.class) {
            return (Vector.Species<E, S>) FloatVector.speciesInstance(s);
        }
        else if (c == double.class) {
            return (Vector.Species<E, S>) DoubleVector.speciesInstance(s);
        }
        else if (c == byte.class) {
            return (Vector.Species<E, S>) ByteVector.speciesInstance(s);
        }
        else if (c == short.class) {
            return (Vector.Species<E, S>) ShortVector.speciesInstance(s);
        }
        else if (c == int.class) {
            return (Vector.Species<E, S>) IntVector.speciesInstance(s);
        }
        else if (c == long.class) {
            return (Vector.Species<E, S>) LongVector.speciesInstance(s);
        }
        else {
            throw new IllegalArgumentException("Bad vector element type: " + c.getName());
        }
    }
}
