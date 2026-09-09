package com.example.mobileclock.feature.update

import android.content.Context
import com.example.mobileclock.native.NativeRenderer

// Единая точка логирования Kotlin-событий обновления. NativeRenderer записывает
// их в тот же файл, что и события C++/OpenGL, поэтому экспорт логов не теряет
// границу между native и Android-частями приложения.
internal object UpdateDiagnostics {
    fun write(context: Context, message: String) {
        NativeRenderer.log(context.filesDir, CATEGORY, message)
    }

    private const val CATEGORY = "SelfUpdate"
}