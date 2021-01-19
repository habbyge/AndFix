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

package com.alipay.euler.andfix.patch;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.Manifest;

public class Patch implements Comparable<Patch> {
	private static final String ENTRY_NAME = "META-INF/PATCH.MF";
	private static final String CLASSES = "-Classes";
	private static final String PATCH_CLASSES = "Patch-Classes";
	private static final String CREATED_TIME = "Created-Time";
	private static final String PATCH_NAME = "Patch-Name";

	/**
	 * patch file
	 */
	private final File mFile;
	/**
	 * name
	 */
	private String mName;
	/**
	 * create time
	 */
	private Date mTime;
	/**
	 * classes of patch
	 * <PatchName, List<该patch文件中包括的所有class文件名>>
	 * 所有需要修复的类名之后保存到这个列表中，后面会通过修复包名称获取到他的修复类名称列表
	 */
	private Map<String, List<String>> mClassesMap;

	public Patch(File file) throws IOException {
		mFile = file;
		init();
	}

	/**
	 * 每个 修复包.apatch 其实是一个 JarFile 文件，这里会去读取 MF 文件中的信息，然后获取到本次需要修复的类信息，
	 * 需要修复的类名称直接使用逗号分割，META-INF/PATCH.MF文件格式如下:
	 * Manifest-Version: 1.0
	 * Patch-Name: app-release-fix
	 * Created-Time: 9 Nov 2020 01:53:27 GMT
	 * From-File: app-release-fix.apk
	 * To-File: app-release-online.apk
	 * Patch-Classes: cn.wjdiankong.andfix.Utils_CF
	 * Created-By: 1.0(ApkPatch)
	 */
	private void init() throws IOException {
		JarFile jarFile = null;
		InputStream inputStream = null;
		try {
			jarFile = new JarFile(mFile);
			JarEntry entry = jarFile.getJarEntry(ENTRY_NAME);
			inputStream = jarFile.getInputStream(entry);
			Manifest manifest = new Manifest(inputStream);
			Attributes mainAttributes = manifest.getMainAttributes();
			mName = mainAttributes.getValue(PATCH_NAME); // 补丁包名：app-release-fix
			mTime = new Date(mainAttributes.getValue(CREATED_TIME)); // 9 Nov 2020 01:53:27 GMT

			mClassesMap = new HashMap<String, List<String>>();

			Attributes.Name attrName;
			String name;
			List<String> strings;
			for (Object attr : mainAttributes.keySet()) {
				attrName = (Attributes.Name) attr;
				name = attrName.toString();
				if (name.endsWith(CLASSES)) { // -Classes，说明是类
					strings = Arrays.asList(mainAttributes.getValue(attrName).split(","));
					if (name.equalsIgnoreCase(PATCH_CLASSES)) { // Patch-Classes
						mClassesMap.put(mName, strings); // 补丁(patch)包中包含的需要素有修复类
					} else { // remove count(-Classes)
						mClassesMap.put(name.trim().substring(0, name.length() - 8), strings);
					}
				}
			}
		} finally {
			if (jarFile != null) {
				jarFile.close();
			}
			if (inputStream != null) {
				inputStream.close();
			}
		}

	}

	public String getName() {
		return mName;
	}

	public File getFile() {
		return mFile;
	}

	public Set<String> getPatchNames() {
		return mClassesMap.keySet();
	}

	public List<String> getClasses(String patchName) {
		return mClassesMap.get(patchName);
	}

	public Date getTime() {
		return mTime;
	}

	@Override
	public int compareTo(Patch another) {
		return mTime.compareTo(another.getTime());
	}
}
