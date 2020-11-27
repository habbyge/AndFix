/*
 *
 * Copyright (c) 2015, alipay.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * art.h
 * 
 * Android Google 官方源码网站: https://cs.android.com
 * 
 * Android中文社区提供：https://www.androidos.net.cn/
 *
 * AndFix是一种 native 的 hotfix 方案，它的替换过程是用 C 在 native层完成的.

 * 技术原理：
 * 方法替换 - Java层的每一个方法在虚拟机实现里面都对应着一个ArtMethod的结构体，只要把原方法的
 * 结构体内容替换成新的结构体的内容，在调用原方法的时候，真正执行的指令会是新方法的指令；这样就能
 * 实现热修复.
 * 
 * 
 */

#include <jni.h>

#ifdef HAVE_STDINT_H
    #include <stdint.h>    /* C99 */
    typedef uint8_t u1;
    typedef uint16_t u2;
    typedef uint32_t u4;
    typedef uint64_t u8;
    typedef int8_t s1;
    typedef int16_t s2;
    typedef int32_t s4;
    typedef int64_t s8;
#else
    typedef unsigned char u1;
    typedef unsigned short u2;
    typedef unsigned int u4;
    typedef unsigned long long u8;
    typedef signed char s1;
    typedef signed short s2;
    typedef signed int s4;
    typedef signed long long s8;
#endif

void replace_4_4(JNIEnv* env, jobject src, jobject dest);
void setFieldFlag_4_4(JNIEnv* env, jobject field);

void replace_5_0(JNIEnv* env, jobject src, jobject dest);
void setFieldFlag_5_0(JNIEnv* env, jobject field);

void replace_5_1(JNIEnv* env, jobject src, jobject dest);
void setFieldFlag_5_1(JNIEnv* env, jobject field);

void replace_6_0(JNIEnv* env, jobject src, jobject dest);
void setFieldFlag_6_0(JNIEnv* env, jobject field);

void replace_7_0(JNIEnv* env, jobject src, jobject dest);
void setFieldFlag_7_0(JNIEnv* env, jobject field);

