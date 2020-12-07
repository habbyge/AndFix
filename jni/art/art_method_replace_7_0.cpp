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

#include "art.h"
#include "art_7_0.h"
#include "common.h"

/**
 * src 替换为 dest
 */
void replace_7_0(JNIEnv* env, jobject src, jobject dest) {
	
	// jni的反射支持:
	// 开发者如果知道 方法 或 域 的名称和类型，可以使用JNI来调用Java方法或访问Java域。Java反射API
	// 提供运行时自省。JNI提供一组函数来使用Java反射API.
	// FromReflectedMethod() 该函数是通过 java.lang.reflect.Method 或 
	// java.lang.reflect.Constructor 来获取其反射的目标方法对应的 methodId.
	// 这里的思路来自于：跟踪Java层代码：Method.invoke()调用链得到：
	// 实现在 art/runtime/native/java_lang_reflect_Method.cc 里面，这个 jni 方法最终调用了
    // art/runtime/reflection.cc 的 InvokeMethod 方法.
    // private native Object invokeNative(Object obj, Object[] args, Class<?> declaringClass,
    //                                   Class<?>[] parameterTypes, Class<?> returnType,
    //                                   int slot, boolean noAccessCheck)
    //                                   throws IllegalAccessException,
    //                                   IllegalArgumentException,
    //                                   InvocationTargetException;
    // 接着调用：
    // object InvokeMethod(const ScopedObjectAccessAlreadyRunnable& soa, jobject javaMethod,
    //                     jobject javaReceiver, jobject javaArgs, bool accessible)
    // 其中第2个参数 javaMethod 就是Java层我们进行反射调用的那个Method对象，在jni层反映为一个jobject；
    // InvokeMethod 这个 native方法首先通过 mirror::ArtMethod::FromReflectedMethod 获取了 Java
    // 对象的在 native 层的 ArtMethod 指针, ok，到这里就知道了，为啥直接通过 FromReflectedMethod()
    // 就能获取 ArtMethod 指针了吧.
	art::mirror::ArtMethod* smeth = (art::mirror::ArtMethod*) env->FromReflectedMethod(src);
	art::mirror::ArtMethod* dmeth = (art::mirror::ArtMethod*) env->FromReflectedMethod(dest);
	// 为啥可以根据Android版本(适配性的需要)自己定义 art::mirror::ArtMethod 类，并强制类型转换？
	// 我的理解是：对于二进制的字节码Elf文件来说，其实 ArtMethod的类路径是没有意义的，更进一步说，
	// "这块儿存储大小" 符合 ArtMethod 的大小即可，且该类中的的各个字段，同样满足 "这块儿存储空间"的要求。

	reinterpret_cast<art::mirror::Class*>(dmeth->declaring_class_)->clinit_thread_id_ =
			reinterpret_cast<art::mirror::Class*>(smeth->declaring_class_)->clinit_thread_id_;

	reinterpret_cast<art::mirror::Class*>(dmeth->declaring_class_)->status_ =
			reinterpret_cast<art::mirror::Class*>(smeth->declaring_class_)->status_ -1;
			
	// for reflection invoke
	reinterpret_cast<art::mirror::Class*>(dmeth->declaring_class_)->super_class_ = 0;

	smeth->declaring_class_ = dmeth->declaring_class_;
	smeth->access_flags_ = dmeth->access_flags_  | 0x0001;
	smeth->dex_code_item_offset_ = dmeth->dex_code_item_offset_;
	smeth->dex_method_index_ = dmeth->dex_method_index_;
	smeth->method_index_ = dmeth->method_index_;
	smeth->hotness_count_ = dmeth->hotness_count_;

	smeth->ptr_sized_fields_.dex_cache_resolved_methods_ =
			dmeth->ptr_sized_fields_.dex_cache_resolved_methods_;
	smeth->ptr_sized_fields_.dex_cache_resolved_types_ =
			dmeth->ptr_sized_fields_.dex_cache_resolved_types_;

	smeth->ptr_sized_fields_.entry_point_from_jni_ = 
			dmeth->ptr_sized_fields_.entry_point_from_jni_;

	smeth->ptr_sized_fields_.entry_point_from_quick_compiled_code_ = 
			dmeth->ptr_sized_fields_.entry_point_from_quick_compiled_code_;

	LOGD("replace_7_0: %d , %d",
			smeth->ptr_sized_fields_.entry_point_from_quick_compiled_code_,
			dmeth->ptr_sized_fields_.entry_point_from_quick_compiled_code_);

}

/**
 * @param field java 层的 Fieild 类型的对象.
 */
void setFieldFlag_7_0(JNIEnv* env, jobject field) {
	art::mirror::ArtField* artField = (art::mirror::ArtField*) env->FromReflectedField(field);
	artField->access_flags_ = artField->access_flags_ & (~0x0002) | 0x0001;
	LOGD("setFieldFlag_7_0: %d ", artField->access_flags_);
}
