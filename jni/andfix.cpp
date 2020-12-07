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

// IP寄存器(r12)：Intra-Procedure-call Scratch Register；内部程序调用暂存寄存器

/**
 * Android热修复原理(https://zhuanlan.zhihu.com/p/64730686)
 * 
 * AndFix的原理就是方法的替换，把有bug的方法替换成补丁文件中的方法。
 * 注：在Native层使用指针替换的方式替换bug方法，已达到修复bug的目的。
 *
 * AndFix采用native hook的方式，这套方案直接使用 art_replaceMethod 替换 class中方
 * 法的实现。由于它并没有整体替换class, 而field在class中的相对地址在class加载时已确定，
 * 所以AndFix无法支持新增或者删除filed的情况(通过替换init与clinit只可以修改field的数值)。
 * Andfix可以支持的补丁场景相对有限，仅仅可以使用它来修复特定问题。
 */

#include <jni.h>
#include <stdio.h>
#include <cassert>

#include "common.h"

#define JNIREG_CLASS "com/alipay/euler/andfix/AndFix"

// dalvik
extern jboolean dalvik_setup(JNIEnv* env, int apilevel);
extern void dalvik_replaceMethod(JNIEnv* env, jobject src, jobject dest);
extern void dalvik_setFieldFlag(JNIEnv* env, jobject field);
//art
extern jboolean art_setup(JNIEnv* env, int apilevel);
extern void art_replaceMethod(JNIEnv* env, jobject src, jobject dest);
extern void art_setFieldFlag(JNIEnv* env, jobject field);

static bool isArt;

static jboolean setup(JNIEnv* env, jclass clazz, jboolean isart, jint apilevel) {
	isArt = isart;
	LOGD("vm is: %s , apilevel is: %i", (isArt ? "art" : "dalvik"), (int) apilevel);
	if (isArt) {
		return art_setup(env, (int) apilevel);
	} else {
		return dalvik_setup(env, (int) apilevel);
	}
}

/**
 * dest替换src
 */
static void replaceMethod(JNIEnv* env, jclass clazz, jobject src, jobject dest) {
	if (isArt) {
		art_replaceMethod(env, src, dest);
	} else {
		dalvik_replaceMethod(env, src, dest);
	}
}

static void setFieldFlag(JNIEnv* env, jclass clazz, jobject field) {
	if (isArt) {
		art_setFieldFlag(env, field);
	} else {
		dalvik_setFieldFlag(env, field);
	}
}
/*
 * JNI registration.
 */
static JNINativeMethod gMethods[] = {
/* name, signature, funcPtr */
    {
        "setup",
        "(ZI)Z",
        (void*) setup
    },
    {
        "replaceMethod",
		"(Ljava/lang/reflect/Method;Ljava/lang/reflect/Method;)V",
		(void*) replaceMethod
	},
	{
	    "setFieldFlag",
		"(Ljava/lang/reflect/Field;)V",
		(void*) setFieldFlag
	},
};

/*
 * Register several native methods for one class.
 */
static int registerNativeMethods(JNIEnv* env, 
								 const char* className, 
								 JNINativeMethod* gMethods, 
								 int numMethods) {
									 
	jclass clazz;
	clazz = env->FindClass(className);
	if (clazz == NULL) {
		return JNI_FALSE;
	}
	if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
		return JNI_FALSE;
	}

	return JNI_TRUE;
}

/*
 * Register native methods for all classes we know about.
 */
static int registerNatives(JNIEnv* env) {

	if (!registerNativeMethods(env, JNIREG_CLASS, gMethods,
	        sizeof(gMethods) / sizeof(gMethods[0]))) {

		return JNI_FALSE;
	}
	return JNI_TRUE;
}

/*
 * Set some test stuff up.
 *
 * Returns the JNI version on success, -1 on failure.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	JNIEnv* env = NULL;
	jint result = -1;

	if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		return -1;
	}
	assert(env != NULL);

	if (!registerNatives(env)) { //注册
		return -1;
	}
	/* success -- return valid version number */
	result = JNI_VERSION_1_4;

	return result;
}

