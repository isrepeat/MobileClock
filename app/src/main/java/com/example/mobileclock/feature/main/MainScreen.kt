package com.example.mobileclock.feature.main

import android.content.Context
import android.view.Gravity
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import com.example.mobileclock.native.NativeRenderSurfaceView

class MainScreen(private val context: Context) {
    // Экран формирует только Android Views и передаёт действия наверх как callbacks.
    fun createView(onShareLogs: () -> Unit, onExportLogs: () -> Unit): FrameLayout =
        FrameLayout(context).apply {
            addView(NativeRenderSurfaceView(context))
            addView(createLogActions(onShareLogs, onExportLogs), logActionsLayoutParams())
        }

    private fun createLogActions(
        onShareLogs: () -> Unit,
        onExportLogs: () -> Unit,
    ): LinearLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        addView(Button(context).apply {
            text = "Поделиться логом"
            setOnClickListener { onShareLogs() }
        })
        addView(Button(context).apply {
            text = "Экспортировать логи"
            setOnClickListener { onExportLogs() }
        })
    }

    private fun logActionsLayoutParams() = FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.WRAP_CONTENT,
        FrameLayout.LayoutParams.WRAP_CONTENT,
        Gravity.BOTTOM or Gravity.END,
    ).apply {
        setMargins(0, 0, 24, 24)
    }
}