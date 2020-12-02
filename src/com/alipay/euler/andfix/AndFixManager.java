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

package com.alipay.euler.andfix;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.Enumeration;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import android.content.Context;
import android.util.Log;

import com.alipay.euler.andfix.annotation.MethodReplace;
import com.alipay.euler.andfix.security.SecurityChecker;

import dalvik.system.DexFile;

/**
 * AndFix替换原理: https://developer.aliyun.com/article/74598#
 *
 * Andfix是 "即时生效" 方案.
 * (在4.4以下用的是dalvik虚拟机，而在4.4以上用的是art虚拟机，因此现在根本不用考虑dalvik场景了).
 *
 * -为何唯独Andfix能够做到即时生效呢？
 * 原因是这样的，在app运行到一半的时候，所有需要发生变更的Class已经被加载过了，在Android上是无法对
 * 一个Class进行卸载的。而腾讯系的方案，都是让Classloader去加载新的类。如果不重启，原来的类还在虚
 * 拟机中，就无法加载新类。因此，只有在下次重启的时候，在还没走到业务逻辑之前抢先加载补丁中的新类，这
 * 样后续访问这个类时，就会Resolve为新的类。从而达到热修复的目的。
 * Andfix采用的方案是，在已经加载了的类中直接在Native层替换掉原有方法，是在原来类的基础上进行修改的,
 * 核心方案是:AndFix.replaceMethod()，是在Java层通过反射机制得到的Method对象所对应的jobject。
 * src对应的是需要被替换的原有方法.而dest对应的就是新方法,新方法存在于补丁包的新类中,也就是补丁方法,
 * 其中 FromReflectedMethod 得到的 jmethodID 即是对应的 art::mirror::ArtMethod 对象的基地址.
 * 每一个Java方法在art中都对应着一个ArtMethod，ArtMethod记录了这个Java方法的所有信息，包括所属类、
 * 访问权限、代码执行地址等等。
 * 其中，ArtMethod中最重要的字段是:
 * (1) entry_point_from_interprete_
 * (2) entry_point_from_quick_compiled_code_
 * 从名字可以看出来，他们就是方法的执行入口。我们知道，Java代码在Android中会被编译为Dex Code.
 * art中可以采用解释模式或者AOT机器码模式执行，这两个 "字段" 分别对应这两种执行模式:
 * 1. 解释模式，就是取出Dex Code，逐条解释执行就行了。如果方法的调用者是以解释模式运行的，在调用这个
 * 方法时，就会取得这个方法对应的ArtMethod.entry_point_from_interpreter_，然后跳转过去执行.
 * 2. AOT的方式，就会先预编译好Dex Code对应的机器码，然后运行期直接执行机器码就行了，不需要一条条地
 * 解释执行Dex Code. 如果方法的调用者是以AOT机器码方式执行的，在调用这个方法时，就是跳转到
 * ArtMethod.entry_point_from_quick_compiled_code_ 执行.
 *
 * 那我们是不是只需要替换这几个 entry_point_* 入口地址就能够实现方法替换了呢？
 * 并没有这么简单。因为不论是解释模式或是AOT机器码模式，在运行期间还会需要用到ArtMethod里面的其他成
 * 员字段。就以AOT机器码模式为例，虽然Dex Code被编译成了机器码。但是机器码并不是可以脱离虚拟机而单独
 * 运行的，AOT机器码的执行过程，还是会有对于虚拟机以及ArtMethod其他成员字段的依赖.
 *
 * - Art兼容性问题的根源
 * 目前市面上几乎所有的native替换方案，比如Andfix和另一种Hook框架Legend，都是写死了ArtMethod结构
 * 体,这会带来巨大的兼容性问题. 从刚才的分析可以看到，虽然Andfix是把底层结构强转为了
 * art::mirror::ArtMethod，但这里的 art::mirror::ArtMethod 并非等同于app运行时所在设备虚拟机
 * 底层的 art::mirror::ArtMethod, 而是Andfix自己构造的 art::mirror::ArtMethod, 虽然 AndFix
 * 中的 ArtMethod 结构里的各个成员的大小是和AOSP开源代码里完全一致的(这是由于Android源码是公开的,
 * Andfix 里面的这个 ArtMethod 自然是遵照 Android 虚拟机 art 源码里面的 ArtMethod 构建的), 但
 * 是厂商是可以任意修改这部分代码，因此这里才是兼容性问题的根源.
 *
 * 突破底层结构的差异:
 *
 */
public class AndFixManager {
	private static final String TAG = "AndFixManager";

	private static final String DIR = "apatch_opt";

	/**
	 * context
	 */
	private final Context mContext;

	/**
	 * classes will be fixed
	 */
	private static final Map<String, Class<?>> mFixedClass = new ConcurrentHashMap<>();

	/**
	 * whether support AndFix
	 */
	private boolean mSupport;

	/**
	 * security check
	 */
	private SecurityChecker mSecurityChecker;

	/**
	 * optimize directory
	 */
	private File mOptDir;

	public AndFixManager(Context context) {
		mContext = context;
		mSupport = Compat.isSupport();
		if (mSupport) {
			mSecurityChecker = new SecurityChecker(mContext);
			mOptDir = new File(mContext.getFilesDir(), DIR);
			if (!mOptDir.exists() && !mOptDir.mkdirs()) {// make directory fail
				mSupport = false;
				Log.e(TAG, "opt dir create error.");
			} else if (!mOptDir.isDirectory()) {// not directory
				boolean ret = mOptDir.delete();
				Log.i(TAG, "mOptDir.delete(): " + ret);
				mSupport = false;
			}
		}
	}

	/**
	 * delete optimize file of patch file
	 * 
	 * @param file
	 *            patch file
	 */
	public synchronized void removeOptFile(File file) {
		File optfile = new File(mOptDir, file.getName());
		if (optfile.exists() && !optfile.delete()) {
			Log.e(TAG, optfile.getName() + " delete error.");
		}
	}

	/**
	 * fix
	 * @param patchPath patch path
	 */
	public synchronized void fix(String patchPath) {
		fix(new File(patchPath), mContext.getClassLoader(), null);
	}

	/**
	 * FIX
	 * 
	 * @param pathFile
	 *            patch file
	 * @param classLoader
	 *            classloader of class that will be fixed
	 * @param classNames
	 *            classes will be fixed
	 */
	public synchronized
	void fix(File pathFile, ClassLoader classLoader, List<String> classNames) {
		if (!mSupport) {
			return;
		}

		if (!mSecurityChecker.verifyApk(pathFile)) { // security check fail
			return;
		}

		try {
			File optfile = new File(mOptDir, pathFile.getName());
			boolean saveFingerprint = true;
			if (optfile.exists()) {
				// need to verify fingerprint when the optimize file exist,
				// prevent someone attack on jailbreak device with
				// Vulnerability-Parasyte.
				// btw:exaggerated android Vulnerability-Parasyte
				// http://secauo.com/Exaggerated-Android-Vulnerability-Parasyte.html

				if (mSecurityChecker.verifyOpt(optfile)) {
					saveFingerprint = false;
				} else if (!optfile.delete()) {
					return;
				}
			}

			final DexFile dexFile = DexFile.loadDex(pathFile.getAbsolutePath(),
													optfile.getAbsolutePath(),
													Context.MODE_PRIVATE);

			if (saveFingerprint) {
				mSecurityChecker.saveOptSig(optfile);
			}

			ClassLoader patchClassLoader = new ClassLoader(classLoader) {

				@Override
				protected Class<?> findClass(String className) throws ClassNotFoundException {
					Class<?> clazz = dexFile.loadClass(className, this);
					final String packagePath = "com.alipay.euler.andfix";
					if (clazz == null && className.startsWith(packagePath)) {
						return Class.forName(className);// annotation’s class
														// not found
					}
					if (clazz == null) {
						throw new ClassNotFoundException(className);
					}
					return clazz;
				}
			};

			Enumeration<String> entrys = dexFile.entries();
			Class<?> clazz;
			while (entrys.hasMoreElements()) {
				String entry = entrys.nextElement();
				if (classNames != null && !classNames.contains(entry)) {
					continue;// skip, not need fix
				}
				clazz = dexFile.loadClass(entry, patchClassLoader);
				if (clazz != null) {
					fixClass(clazz, classLoader);
				}
			}
		} catch (IOException e) {
			Log.e(TAG, "pacth", e);
		}
	}

	/**
	 * fix class
	 * @param clazz class
	 */
	private void fixClass(Class<?> clazz, ClassLoader classLoader) {
		Method[] methods = clazz.getDeclaredMethods();
		MethodReplace methodReplace;
		String className;
		String methodName;
		for (Method method : methods) {
			methodReplace = method.getAnnotation(MethodReplace.class);
			if (methodReplace == null) {
				continue;
			}
			className = methodReplace.clazz();
			methodName = methodReplace.method();
			if (!isEmpty(className) && !isEmpty(methodName)) {
				replaceMethod(classLoader, className, methodName, method);
			}
		}
	}

	/**
	 * replace method
	 * 
	 * @param classLoader classloader
	 * @param className name of target class
	 * @param methodname name of target method
	 * @param srcMethod source method
	 */
	private void replaceMethod(ClassLoader classLoader, String className,
							   String methodname, Method srcMethod) {

		try {
			String key = className + "@" + classLoader.toString();
			Class<?> clazz = mFixedClass.get(key);
			if (clazz == null) { // class not load
				Class<?> clzz = classLoader.loadClass(className);
				// initialize target class
				clazz = AndFix.initTargetClass(clzz);
			}
			if (clazz != null) { // initialize class OK
				mFixedClass.put(key, clazz);
				Method src = clazz.getDeclaredMethod(methodname, srcMethod.getParameterTypes());
				AndFix.addReplaceMethod(src, srcMethod); // 前者 替换 后者
			}
		} catch (Exception e) {
			Log.e(TAG, "replaceMethod", e);
		}
	}

	private static boolean isEmpty(String string) {
		return string == null || string.length() <= 0;
	}
}
