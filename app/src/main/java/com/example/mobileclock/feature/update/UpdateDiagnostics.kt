package com.example.mobileclock.feature.update

import android.content.Context
import com.example.mobileclock.native.NativeRenderer

internal object UpdateDiagnostics {
    fun write(context: Context, message: String) {
        NativeRenderer.log(context.filesDir, CATEGORY, message)
    }

    private const val CATEGORY = "SelfUpdate"
}