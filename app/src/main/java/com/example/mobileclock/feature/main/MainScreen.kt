package com.example.mobileclock.feature.main

import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import com.example.mobileclock.native.NativeRenderSurfaceView

class MainScreen(private val context: Context) {
    private var updateStatus: TextView? = null

    // Экран формирует только Android Views и передаёт действия наверх как callbacks.
    fun createView(
        onShareLogs: () -> Unit,
        onExportLogs: () -> Unit,
        onUploadScreenshotToGoogleDrive: () -> Unit,
        onUpdateApplication: () -> Unit,
    ): FrameLayout =
        FrameLayout(context).apply {
            addView(NativeRenderSurfaceView(context))
            addView(
                createActions(
                    onShareLogs,
                    onExportLogs,
                    onUploadScreenshotToGoogleDrive,
                    onUpdateApplication,
                ),
                actionsLayoutParams(),
            )
        }

    fun showUpdateStatus(message: String) {
        // Контроллер обновления передаёт сюда прогресс без знания о структуре UI.
        updateStatus?.text = message
    }

    private fun createActions(
        onShareLogs: () -> Unit,
        onExportLogs: () -> Unit,
        onUploadScreenshotToGoogleDrive: () -> Unit,
        onUpdateApplication: () -> Unit,
    ): LinearLayout = LinearLayout(context).apply {
        orientation = LinearLayout.VERTICAL
        // Кнопка остаётся активной визуально, но повторный запрос блокируется
        // SelfUpdateController, чтобы не создавать две install-session.
        addView(Button(context).apply {
            text = "ОБНОВИТЬ"
            textSize = 20.0f
            minimumWidth = dp(260)
            minimumHeight = dp(72)
            setOnClickListener { onUpdateApplication() }
        })
        val status = TextView(context).apply {
            setTextColor(Color.WHITE)
            setPadding(0, dp(8), 0, dp(12))
            text = "Готово к проверке обновлений"
        }
        updateStatus = status
        addView(status)
        addView(Button(context).apply {
            text = "Поделиться логом"
            setOnClickListener { onShareLogs() }
        })
        addView(Button(context).apply {
            text = "Экспортировать логи"
            setOnClickListener { onExportLogs() }
        })
        addView(Button(context).apply {
            text = "Скриншот в Google Drive"
            setOnClickListener { onUploadScreenshotToGoogleDrive() }
        })
    }

    private fun actionsLayoutParams() = FrameLayout.LayoutParams(
        FrameLayout.LayoutParams.WRAP_CONTENT,
        FrameLayout.LayoutParams.WRAP_CONTENT,
        Gravity.BOTTOM or Gravity.END,
    ).apply {
        setMargins(0, 0, 24, 24)
    }

    private fun dp(value: Int): Int = (value * context.resources.displayMetrics.density).toInt()
}