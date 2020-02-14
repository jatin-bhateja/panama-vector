/*
 * Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "jni.h"
#include "jvm.h"
#include "prims/qbaJNI.hpp"
#include "runtime/qba.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/os.hpp"

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    version0
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_jdk_internal_misc_QBA_version0
  (JNIEnv *env, jclass qbaClass) {
    return qba_version();
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    versionString0
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_jdk_internal_misc_QBA_versionString0
  (JNIEnv *env, jclass qbaClass) {
    return env->NewStringUTF(qba_version_string());
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    create0
 * Signature: (JIZIIIII)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_create0
  (JNIEnv *env, jclass qbaClass, jlong address, jstring linkName, jboolean secure,
   jint smallPartitionCount, jint mediumPartitionCount, jint largePartitionCount,
   jint maxSlabCount, jint sideDataSize) {
    jlong result = 0;

    if (linkName != NULL) {
        const char *name = env->GetStringUTFChars(linkName, NULL);
        result = (jlong)qba_create(address, name, secure,
                 smallPartitionCount, mediumPartitionCount, largePartitionCount,
                 maxSlabCount, sideDataSize);
        env->ReleaseStringUTFChars(linkName, name);
     } else {
        result = (jlong)qba_create(address, NULL, secure,
                 smallPartitionCount, mediumPartitionCount, largePartitionCount,
                 maxSlabCount, sideDataSize);
     }
     return result;
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    createSize0
 * Signature: (ZIIIII)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_createSize0
  (JNIEnv *env, jclass qbaClass, jboolean secure,
   jint smallPartitionCount, jint mediumPartitionCount, jint largePartitionCount,
   jint maxSlabCount, jint sideDataSize) {
  return qba_create_size(secure,
          smallPartitionCount, mediumPartitionCount, largePartitionCount,
          maxSlabCount, sideDataSize);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    destroy0
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_destroy0
  (JNIEnv *env, jclass qbaClass, jlong qba, jboolean unlink) {
  qba_destroy((qba_t *)qba, unlink);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    getReference0
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_getReference0
  (JNIEnv *env, jclass qbaClass, jlong qba) {
  return (jlong)qba_get_reference((qba_t *)qba);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    setReference0
 * Signature: (JJJ)Z
 */
JNIEXPORT jboolean JNICALL Java_jdk_internal_misc_QBA_setReference0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong oldValue, jlong newValue){
  return qba_set_reference((qba_t *)qba, (void *)oldValue, (void *)newValue);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    clear0
 * Signature: (JJ)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_clear0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address) {
  qba_clear((qba_t *)qba, (void *)address);
}


/*
 * Class:     jdk_internal_misc_QBA
 * Method:    allocate0
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_allocate0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong size) {
    return (jlong)qba_allocate((qba_t *)qba, size);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    deallocate0
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_deallocate0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address) {
  qba_deallocate((qba_t *)qba, (void *)address);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    reallocate0
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_reallocate0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address, jlong size) {
    return (jlong)qba_reallocate((qba_t *)qba, (void *)address, size);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    size0
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_size0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address) {
    return qba_size((qba_t *)qba, (void*)address);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    base0
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_base0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address) {
    return (jlong)qba_base((qba_t *)qba, (void*)address);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    sideData0
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_sideData0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address) {
    return (jlong)qba_side_data((qba_t *)qba, (void*)address);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    next0
 * Signature: (J)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_next0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address) {
    return (jlong)qba_next((qba_t *)qba, (void *)address);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    stats0
 * Signature: ([J[J)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_stats0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlongArray counts, jlongArray sizes) {
    jlong *countsElements = env->GetLongArrayElements(counts, NULL);
    jlong *sizesElements = env->GetLongArrayElements(sizes, NULL);
    qba_stats((qba_t *)qba, (uint64_t *)countsElements, (uint64_t *)sizesElements);
    env->ReleaseLongArrayElements(sizes, sizesElements, 0);
    env->ReleaseLongArrayElements(counts, countsElements, 0);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    allocateBulk0
 * Signature: (JZ[J)I
 */
JNIEXPORT jint JNICALL Java_jdk_internal_misc_QBA_allocateBulk0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong size, jboolean contiguous, jlongArray addresses) {
    jsize count = env->GetArrayLength(addresses);
    jlong *elements = env->GetLongArrayElements(addresses, NULL);
    jint result = qba_allocate_bulk((qba_t *)qba, size, count, (void **)elements, contiguous);
    env->ReleaseLongArrayElements(addresses, elements, 0);
    return result;
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    deallocateBulk0
 * Signature: ([J)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_deallocateBulk0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlongArray addresses) {
    jsize count = env->GetArrayLength(addresses);
    jlong *elements = env->GetLongArrayElements(addresses, NULL);
    qba_deallocate_bulk((qba_t *)qba, (int)count, (void **)elements);
    env->ReleaseLongArrayElements(addresses, elements, JNI_ABORT);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    allocateCount0
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_allocateCount0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong size, jint count) {
    return (jlong)qba_allocate_count((qba_t *)qba, size, count);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    deallocateCount0
 * Signature: (JJI)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_deallocateCount0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address, jlong size, jint count) {
    qba_deallocate_count((qba_t *)qba, (void *)address, size, count);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    allocateFit0
 * Signature: (JI)J
 */
JNIEXPORT jlong JNICALL Java_jdk_internal_misc_QBA_allocateFit0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong size, jint degree) {
    return (jlong)qba_allocate_fit((qba_t *)qba, size, degree);
}

/*
 * Class:     jdk_internal_misc_QBA
 * Method:    deallocateFit0
 * Signature: (JJI)V
 */
JNIEXPORT void JNICALL Java_jdk_internal_misc_QBA_deallocateFit0
  (JNIEnv *env, jclass qbaClass, jlong qba, jlong address, jlong size, jint degree) {
    qba_deallocate_fit((qba_t *)qba, (void *)address, size, degree);
}

#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)

static JNINativeMethod jdk_internal_misc_QBA_methods[] = {
    { CC"version0",         CC"()I",                          FN_PTR(Java_jdk_internal_misc_QBA_version0) },
    { CC"versionString0",   CC"()Ljava/lang/String;",         FN_PTR(Java_jdk_internal_misc_QBA_versionString0) },
    { CC"create0",          CC"(JLjava/lang/String;ZIIIII)J", FN_PTR(Java_jdk_internal_misc_QBA_create0) },
    { CC"createSize0",      CC"(ZIIIII)J",                    FN_PTR(Java_jdk_internal_misc_QBA_createSize0) },
    { CC"destroy0",         CC"(JZ)V",                        FN_PTR(Java_jdk_internal_misc_QBA_destroy0) },
    { CC"getReference0",    CC"(J)J",                         FN_PTR(Java_jdk_internal_misc_QBA_getReference0) },
    { CC"setReference0",    CC"(JJJ)Z",                       FN_PTR(Java_jdk_internal_misc_QBA_setReference0) },
    { CC"allocate0",        CC"(JJ)J",                        FN_PTR(Java_jdk_internal_misc_QBA_allocate0) },
    { CC"deallocate0",      CC"(JJ)V",                        FN_PTR(Java_jdk_internal_misc_QBA_deallocate0) },
    { CC"reallocate0",      CC"(JJJ)J",                       FN_PTR(Java_jdk_internal_misc_QBA_reallocate0) },
    { CC"clear0",           CC"(JJ)V",                        FN_PTR(Java_jdk_internal_misc_QBA_clear0) },
    { CC"size0",            CC"(JJ)J",                        FN_PTR(Java_jdk_internal_misc_QBA_size0) },
    { CC"base0",            CC"(JJ)J",                        FN_PTR(Java_jdk_internal_misc_QBA_base0) },
    { CC"sideData0",        CC"(JJ)J",                        FN_PTR(Java_jdk_internal_misc_QBA_sideData0) },
    { CC"next0",            CC"(JJ)J",                        FN_PTR(Java_jdk_internal_misc_QBA_next0) },
    { CC"stats0",           CC"(J[J[J)V",                     FN_PTR(Java_jdk_internal_misc_QBA_stats0) },
    { CC"allocateBulk0",    CC"(JJZ[J)I",                     FN_PTR(Java_jdk_internal_misc_QBA_allocateBulk0) },
    { CC"deallocateBulk0",  CC"(J[J)V",                       FN_PTR(Java_jdk_internal_misc_QBA_deallocateBulk0) },
    { CC"allocateCount0",   CC"(JJI)J",                       FN_PTR(Java_jdk_internal_misc_QBA_allocateCount0) },
    { CC"deallocateCount0", CC"(JJJI)V",                      FN_PTR(Java_jdk_internal_misc_QBA_deallocateCount0) },
    { CC"allocateFit0",     CC"(JJI)J",                       FN_PTR(Java_jdk_internal_misc_QBA_allocateFit0) },
    { CC"deallocateFit0",   CC"(JJJI)V",                      FN_PTR(Java_jdk_internal_misc_QBA_deallocateFit0) }
};

JVM_ENTRY(void, JVM_RegisterJDKInternalMiscQBAMethods(JNIEnv *env, jclass qbaClass)) {
  ThreadToNativeFromVM ttnfv(thread);

  int ok = env->RegisterNatives(qbaClass, jdk_internal_misc_QBA_methods, sizeof(jdk_internal_misc_QBA_methods)/sizeof(JNINativeMethod));
  guarantee(ok == 0, "register jdk.internal.misc.QBA natives");
} JVM_END
