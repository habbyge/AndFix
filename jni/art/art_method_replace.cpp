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
#include "common.h"

static int apilevel;

extern jboolean __attribute__ ((visibility ("hidden")))
art_setup(JNIEnv* env, int level) {
	apilevel = level;
	return JNI_TRUE;
}

/**
 * dest替换src
 */
extern void __attribute__ ((visibility ("hidden"))) 
art_replaceMethod(JNIEnv* env, jobject src, jobject dest) {
  if (apilevel > 23) {
    replace_7_0(env, src, dest);
  } else if (apilevel > 22) {
		replace_6_0(env, src, dest);
	} else if (apilevel > 21) {
		replace_5_1(env, src, dest);
	} else if (apilevel > 19) {
		replace_5_0(env, src, dest);
  } else {
    replace_4_4(env, src, dest);
  }
}

/**
 * 这里为啥需要设置被修复方法(原始方法)所在的 class 中的所有 field 字段为 public ？？？？？？
 * 答案：在 apktools 中会修改修复类、方法的后缀，加上 "_CF"，所以修改后，才能在新的修复类中访问原始类中的字段.
 */
extern void __attribute__ ((visibility ("hidden")))
art_setFieldFlag(JNIEnv* env, jobject field) {
  if (apilevel > 23) {
    setFieldFlag_7_0(env, field);
  } else if (apilevel > 22) {
    setFieldFlag_6_0(env, field);
  } else if (apilevel > 21) {
    setFieldFlag_5_1(env, field);
  } else  if (apilevel > 19) {
    setFieldFlag_5_0(env, field);
  } else {
    setFieldFlag_4_4(env, field);
  }
}
