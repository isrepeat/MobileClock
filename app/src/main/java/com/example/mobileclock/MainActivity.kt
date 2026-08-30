package com.example.mobileclock

import android.content.ClipData
import android.content.Intent
import android.os.Bundle
import android.view.Gravity
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.FileProvider
import java.io.File
import java.io.OutputStream

class MainActivity : ComponentActivity(), SurfaceHolder.Callback {
    private val logFile by lazy { File(filesDir, "logs/mobileclock.log") }
    private val logFiles: List<File>
        get() = listOf(
            File(logFile.parentFile, "mobileclock.2.log"),
            File(logFile.parentFile, "mobileclock.1.log"),
            logFile,
        ).filter(File::isFile)

    private fun writeLogSnapshot(output: OutputStream): Boolean {
        nativeFlushLogs()
        val filesToExport = logFiles
        if (filesToExport.isEmpty()) return false
        filesToExport.forEach { file ->
            output.write("\n--- ${file.name} ---\n".toByteArray(Charsets.UTF_8))
            file.inputStream().use { input -> input.copyTo(output) }
        }
        return true
    }

    private val exportLogs = registerForActivityResult(
        ActivityResultContracts.CreateDocument("text/plain"),
    ) { destination ->
        if (destination == null) return@registerForActivityResult
        val exported = runCatching {
            contentResolver.openOutputStream(destination)?.use { output ->
                check(writeLogSnapshot(output)) { "Лог ещё не создан" }
            } ?: error("Не удалось открыть выбранный файл")
        }.isSuccess
        Toast.makeText(
            this,
            if (exported) "Лог экспортирован" else "Не удалось экспортировать лог",
            Toast.LENGTH_SHORT,
        ).show()
    }

    private fun shareLogs() {
        val snapshot = File(cacheDir, "shared-logs/MobileClock-${System.currentTimeMillis()}.txt")
        val snapshotCreated = runCatching {
            snapshot.parentFile?.mkdirs()
            snapshot.outputStream().use { output ->
                check(writeLogSnapshot(output)) { "Лог ещё не создан" }
            }
        }.isSuccess
        if (!snapshotCreated) {
            snapshot.delete()
            Toast.makeText(this, "Не удалось подготовить лог", Toast.LENGTH_SHORT).show()
            return
        }
        val uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", snapshot)
        val shareIntent = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_STREAM, uri)
            clipData = ClipData.newRawUri("MobileClock log", uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivity(Intent.createChooser(shareIntent, "Поделиться логом"))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        System.loadLibrary("mobileclock")
        logFile.parentFile?.mkdirs()
        nativeSetLogFile(logFile.absolutePath)
        // Передаём доступ к app/src/main/assets нативному TTF-рендереру.
        nativeSetAssetManager(assets)

        val surfaceView = SurfaceView(this).apply {
            holder.addCallback(this@MainActivity)
            setOnTouchListener { _, event ->
                nativeTouch(event.actionMasked, event.x, event.y)
                true
            }
        }
        setContentView(FrameLayout(this).apply {
            addView(surfaceView)
            addView(LinearLayout(context).apply {
                orientation = LinearLayout.VERTICAL
                addView(Button(context).apply {
                    text = "Поделиться логом"
                    setOnClickListener { shareLogs() }
                })
                addView(Button(context).apply {
                    text = "Экспортировать логи"
                    setOnClickListener {
                        exportLogs.launch("MobileClock-${System.currentTimeMillis()}.txt")
                    }
                })
            }, FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM or Gravity.END,
            ).apply {
                setMargins(0, 0, 24, 24)
            })
        })
    }

    override fun surfaceCreated(holder: SurfaceHolder) = Unit

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int,
    ) {
        nativeSurfaceChanged(holder.surface, width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        nativeSurfaceDestroyed()
    }

    private external fun nativeSurfaceChanged(surface: Surface, width: Int, height: Int)
    private external fun nativeSetAssetManager(assetManager: android.content.res.AssetManager)
    private external fun nativeSetLogFile(path: String)
    private external fun nativeFlushLogs()
    private external fun nativeSurfaceDestroyed()
    private external fun nativeTouch(action: Int, x: Float, y: Float)
}