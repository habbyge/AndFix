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

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.alipay.euler.andfix.AndFixManager;
import com.alipay.euler.andfix.util.FileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.SortedSet;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentSkipListSet;

/**
 * patch manager
 *
 * Android 使用 PathClassLoader 作为其类加载器(加载已经安装到系统路径的apk包)，
 * DexClassLoader 可以从 .jar 和 .apk 类型的文件内部加载 classes.dex文件.
 */
public class PatchManager {
    private static final String TAG = "AndFix.PatchManager";

    // patch extension
    private static final String SUFFIX = ".apatch"; // patch文件的后缀
    private static final String DIR = "apatch";
    private static final String SP_NAME = "_andfix_";
    private static final String SP_VERSION = "version";

    // 可以看到这个保存补丁(patch)文件目录是：/data/data/com.lxyx.habbyge/files/apatch/xxx.apatch

    /**
     * context
     */
    private final Context mContext;
    /**
     * AndFix manager
     */
    private final AndFixManager mAndFixManager;
    /**
     * patch directory
     */
    private final File mPatchDir;
    /**
     * patchs
     */
    private final SortedSet<Patch> mPatchs; // 支持多个patch(修复包)
    /**
     * classloaders
     */
    private final Map<String, ClassLoader> mClassLoaderMap;

    /**
     * @param context context
     */
    public PatchManager(Context context) {
        mContext = context;
        mAndFixManager = new AndFixManager(mContext);
        mPatchDir = new File(mContext.getFilesDir(), DIR);

        // 线程安全的有序的集合，适用于高并发的场景
        mPatchs = new ConcurrentSkipListSet<Patch>();
        mClassLoaderMap = new ConcurrentHashMap<String, ClassLoader>();
    }

    /**
     * initialize
     *
     * @param appVersion App version
     */
    public void init(String appVersion) {
        if (!mPatchDir.exists() && !mPatchDir.mkdirs()) {// make directory fail
            Log.e(TAG, "patch dir create error.");
            return;
        } else if (!mPatchDir.isDirectory()) { // not directory
            Log.e(TAG, "mPatchDir delete result=" + mPatchDir.delete());
            return;
        }

        SharedPreferences sp = mContext.getSharedPreferences(SP_NAME, Context.MODE_PRIVATE);
        String ver = sp.getString(SP_VERSION, null);
        if (ver == null || !ver.equalsIgnoreCase(appVersion)) {
            cleanPatch();
            sp.edit().putString(SP_VERSION, appVersion).apply();
        } else {
            initPatchs();
        }
    }

    private void initPatchs() {
        File[] files = mPatchDir.listFiles();
        for (File file : files) {
            addPatch(file);
        }
    }

    /**
     * add patch file
     */
    private Patch addPatch(File pathFile) {
        Patch patch = null;
        if (pathFile.getName().endsWith(SUFFIX)) {
            try {
                patch = new Patch(pathFile);
                mPatchs.add(patch);
            } catch (IOException e) {
                Log.e(TAG, "addPatch", e);
            }
        }
        return patch;
    }

    private void cleanPatch() {
        File[] files = mPatchDir.listFiles();
        for (File file : files) {
            mAndFixManager.removeOptFile(file);
            if (!FileUtil.deleteFile(file)) {
                Log.e(TAG, file.getName() + " delete error.");
            }
        }
    }

    /**
     * 实时打补丁的接口函数
     * add patch at runtime
     * @param path patch path
     */
    public void addPatch(String path) throws IOException {
        File src = new File(path);
        File dest = new File(mPatchDir, src.getName());
        if (!src.exists()) {
            throw new FileNotFoundException(path);
        }
        if (dest.exists()) {
            Log.d(TAG, "patch [" + path + "] has be loaded.");
            return;
        }
        FileUtil.copyFile(src, dest);
        Patch patch = addPatch(dest);
        if (patch != null) {
            loadPatch(patch);
        }
    }

    /**
     * remove all patchs
     */
    @SuppressWarnings("unused")
    public void removeAllPatch() {
        cleanPatch();
        SharedPreferences sp = mContext.getSharedPreferences(SP_NAME, Context.MODE_PRIVATE);
        sp.edit().clear().apply();
    }

    /**
     * load patch,call when plugin be loaded. used for plugin architecture.
     * need name and classloader of the plugin
     *
     * @param patchName   patch name
     * @param classLoader classloader
     */
    @SuppressWarnings("unused")
    public void loadPatch(String patchName, ClassLoader classLoader) {
        mClassLoaderMap.put(patchName, classLoader);
        Set<String> patchNames;
        List<String> classes;
        for (Patch patch : mPatchs) {
            patchNames = patch.getPatchNames();
            if (patchNames.contains(patchName)) {
                classes = patch.getClasses(patchName);
                mAndFixManager.fix(patch.getFile(), classLoader, classes);
            }
        }
    }

    /**
     * load patch, call when application start
     */
    @SuppressWarnings("unused")
    public void loadPatch() {
        mClassLoaderMap.put("*", mContext.getClassLoader());// wildcard
        Set<String> patchNames;
        List<String> classes;
        for (Patch patch : mPatchs) {
            patchNames = patch.getPatchNames();
            for (String patchName : patchNames) {
                classes = patch.getClasses(patchName);
                mAndFixManager.fix(patch.getFile(), mContext.getClassLoader(), classes);
            }
        }
    }

    /**
     * load specific patch
     * @param patch patch
     */
    private void loadPatch(Patch patch) {
        Set<String> patchNames = patch.getPatchNames();
        ClassLoader classLoader;
        List<String> classes;
        for (String patchName : patchNames) {
            if (mClassLoaderMap.containsKey("*")) {
                classLoader = mContext.getClassLoader();
            } else {
                classLoader = mClassLoaderMap.get(patchName);
            }
            if (classLoader != null) {
                classes = patch.getClasses(patchName);
                mAndFixManager.fix(patch.getFile(), classLoader, classes);
            }
        }
    }
}
