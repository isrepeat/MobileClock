package com.example.mobileclock.feature.logs

import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.core.content.FileProvider
import java.io.File
import java.io.OutputStream

class LogExportCoordinator(
    private val context: Context,
    private val flushLogs: () -> Unit,
) {
    // Работа с FileProvider и ContentResolver изолирована от ViewModel.
    private val logFile = File(context.filesDir, "logs/mobileclock.log")

    fun newExportFileName() = "MobileClock-${System.currentTimeMillis()}.txt"

    fun export(destination: Uri): Boolean = runCatching {
        context.contentResolver.openOutputStream(destination)?.use { output ->
            check(writeSnapshot(output)) { "Лог ещё не создан" }
        } ?: error("Не удалось открыть выбранный файл")
    }.isSuccess

    fun share(): Boolean {
        val snapshot = File(context.cacheDir, "shared-logs/${newExportFileName()}")
        val snapshotCreated = runCatching {
            snapshot.parentFile?.mkdirs()
            snapshot.outputStream().use { output ->
                check(writeSnapshot(output)) { "Лог ещё не создан" }
            }
        }.isSuccess
        if (!snapshotCreated) {
            snapshot.delete()
            return false
        }
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", snapshot)
        context.startActivity(Intent.createChooser(createShareIntent(uri), "Поделиться логом"))
        return true
    }

    private fun writeSnapshot(output: OutputStream): Boolean {
        flushLogs()
        val files = listOf(
            File(logFile.parentFile, "mobileclock.2.log"),
            File(logFile.parentFile, "mobileclock.1.log"),
            logFile,
        ).filter(File::isFile)
        if (files.isEmpty()) return false
        files.forEach { file ->
            output.write("\n--- ${file.name} ---\n".toByteArray(Charsets.UTF_8))
            file.inputStream().use { input -> input.copyTo(output) }
        }
        return true
    }

    private fun createShareIntent(uri: Uri) = Intent(Intent.ACTION_SEND).apply {
        type = "text/plain"
        putExtra(Intent.EXTRA_STREAM, uri)
        clipData = ClipData.newRawUri("MobileClock log", uri)
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }
}