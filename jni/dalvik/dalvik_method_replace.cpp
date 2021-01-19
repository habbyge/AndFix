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

#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include <stdbool.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dalvik.h"
#include "../common.h"

static void* dvm_dlsym(void* hand, const char* name) {
	void* ret = dlsym(hand, name);
	char msg[1024] = { 0 };
	snprintf(msg, sizeof(msg) - 1, "0x%x", ret);
	LOGD("%s = %s\n", name, msg);
	return ret;
}

/**
 * Android libdvm.so 与 libart.so
 * 系统升级到5.1之后，发现system/lib/下面没有libdvm.so了，只剩下了libart.so。对于libart模式，
 * 从4.4就在Developer optins里面就可以手动选择，到5.1算是转正了.
 * 
 * 1，什么是libdvm，libart
 * Android KK里引入了ART虚拟机作为Dalvik虚拟机的替代，其主要目的是把Bytecode的翻译优化从运行时
 * 提前到安装时，以空间换时间，从而达到更流畅的用户体验。目前，KK中Dalvik仍是默认的虚拟机，但用户
 * 可以在Developer Option中切换到ART虚拟机。坊间传闻在下一版可能会转正。Dalvik和ART的实现分别
 * 位于libdvm.so和libart.so这两个库。两个可以同时build也可以只build一个，通过Makefile中的变量
 * PRODUCT_RUNTIMES来控制（https://source.android.com/devices/tech/dalvik/art.html）.
 * 
 * ART本质和Dalvik是一样的，是将Java的Bytecode翻译成Native code。它的主要的实现代码在AOSP的art
 * 目录下，另外在libcore/libart/下还有一部分Java层的实现.
 * 
 * 2，dex翻译成机器码，在libdvm模式下和Libart模式下是有区别的
 * framework/native/cmds/installd/commands.c，涉及到的命令有dex2oat和dexopt，libdvm是运行
 * 时翻译；libart是在install的时候翻译.
 * 3，这些实现的逻辑在zygote的创建有关.
 */
extern jboolean __attribute__ ((visibility ("hidden"))) 
dalvik_setup(JNIEnv* env, int apilevel) {
	// 这里需要注意，后续的版本 dl系列函数不能使用了，被google限制了，可以使用这个方案来破解:
	// 1. 使用 /proc/pid(self)/maps 查找到对应 art/dvm 进程所在的地址空间，获取基地址；
	// 2. 把 libart.so/libdvm.so 使用open()打开，使用mmap()映射到内存空间，然后根据 Elf
	//    文件的格式，从 got/got.plt 中 获取对应目标符号(field/method)的偏移量；
	// 3. 基地址 + 偏移量 即是我们需要的符号地址: ArtMethod/ArtField.
	void* dvm_hand = dlopen("libdvm.so", RTLD_NOW); // dl打开dalvik虚拟机
	if (dvm_hand) {
		dvmDecodeIndirectRef_fnPtr = reinterpret_cast<dvmDecodeIndirectRef_func>(dvm_dlsym(
				dvm_hand, apilevel > 10
						? "_Z20dvmDecodeIndirectRefP6ThreadP8_jobject"
						: "dvmDecodeIndirectRef"));

		if (!dvmDecodeIndirectRef_fnPtr) {
			return JNI_FALSE;
		}

		dvmThreadSelf_fnPtr = reinterpret_cast<dvmThreadSelf_func>(dvm_dlsym(
				dvm_hand, apilevel > 10 ? "_Z13dvmThreadSelfv" : "dvmThreadSelf"));

		if (!dvmThreadSelf_fnPtr) {
			return JNI_FALSE;
		}
		jclass Method_Classs = env->FindClass("java/lang/reflect/Method");
		jClassMethod = env->GetMethodID(Method_Classs, "getDeclaringClass", "()Ljava/lang/Class;");

		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}

extern void __attribute__ ((visibility ("hidden"))) 
dalvik_replaceMethod(JNIEnv* env, jobject src, jobject dest) {
	jobject clazz = env->CallObjectMethod(dest, jClassMethod);

	// Dalvik中，Class对象对应的是 DvmObject结构体
	ClassObject* clz = (ClassObject*) dvmDecodeIndirectRef_fnPtr(dvmThreadSelf_fnPtr(), clazz);

	clz->status = CLASS_INITIALIZED; // 标记该Class对象初始化完毕

	Method* meth = (Method*) env->FromReflectedMethod(src);
	Method* target = (Method*) env->FromReflectedMethod(dest);
	LOGD("dalvikMethod: %s", meth->name);

//	meth->clazz = target->clazz;
	meth->accessFlags |= ACC_PUBLIC;
	meth->methodIndex = target->methodIndex;
	meth->jniArgInfo = target->jniArgInfo;
	meth->registersSize = target->registersSize;
	meth->outsSize = target->outsSize;
	meth->insSize = target->insSize;

	meth->prototype = target->prototype;
	meth->insns = target->insns;
	meth->nativeFunc = target->nativeFunc; // 关键的一步
}

extern void dalvik_setFieldFlag(JNIEnv* env, jobject field) {
	Field* dalvikField = (Field*) env->FromReflectedField(field);
	dalvikField->accessFlags = dalvikField->accessFlags & (~ACC_PRIVATE) | ACC_PUBLIC;
	LOGD("dalvik_setFieldFlag: %d ", dalvikField->accessFlags);
}
