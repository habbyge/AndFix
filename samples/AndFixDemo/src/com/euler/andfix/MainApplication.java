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

package com.euler.andfix;

import java.io.IOException;

import android.app.Application;
import android.os.Environment;
import android.util.Log;

import com.alipay.euler.andfix.patch.PatchManager;

public class MainApplication extends Application {
	private static final String TAG = "AndFix.MainApplication";

	// 需要修复的文件，这里的后缀名都是apatch
	private static final String APATCH_PATH = "/out.apatch";

	@Override
	public void onCreate() {
		super.onCreate();

		// 初始化补丁管理器：支持多个补丁
		PatchManager mPatchManager = new PatchManager(this);
		mPatchManager.init("1.0");
		Log.i(TAG, "inited.");

		// load patch：；开始修复补丁包
		mPatchManager.loadPatch();
		Log.i(TAG, "apatch loaded.");

		// add patch at runtime
		try {
			// .apatch file path
			String patchFileString = Environment.getExternalStorageDirectory().getAbsolutePath() + APATCH_PATH;
			mPatchManager.addPatch(patchFileString);
			Log.d(TAG, "apatch:" + patchFileString + " added.");
		} catch (IOException e) {
			Log.e(TAG, "", e);
		}
	}
}
