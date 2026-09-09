package com.example.mobileclock.feature.update

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import java.io.File

// Граница между основным приложением и отдельным updater-пакетом. Основной
// процесс только скачивает APK, а установкой и перезапуском управляет updater.
internal object ExternalUpdaterLauncher {
    fun launch(context: Context, apk: File) {
        // FileProvider выдаёт updater-у временный read-only content URI вместо
        // небезопасной передачи абсолютного пути к APK.
        val apkUri = FileProvider.getUriForFile(
            context,
            "${context.packageName}.fileprovider",
            apk,
        )
        val intent = Intent(ACTION_INSTALL_UPDATE)
            // Явный component исключает перехват intent другим приложением.
            .setClassName(UPDATER_PACKAGE_NAME, UPDATER_ACTIVITY_NAME)
            .setDataAndType(apkUri, APK_MIME_TYPE)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_GRANT_READ_URI_PERMISSION)
            .putExtra(EXTRA_TARGET_PACKAGE, context.packageName)
        try {
            context.startActivity(intent)
            UpdateDiagnostics.write(context, "APK handed to external updater: uri=$apkUri")
        } catch (exception: ActivityNotFoundException) {
            // Updater устанавливается один раз отдельно и не имеет иконки в launcher.
            throw IllegalStateException(
                "MobileClock Updater не установлен. Установите MobileClockUpdater.apk один раз.",
                exception,
            )
        }
    }

    private const val ACTION_INSTALL_UPDATE = "com.example.mobileclock.updater.action.INSTALL_UPDATE"
    private const val APK_MIME_TYPE = "application/vnd.android.package-archive"
    private const val EXTRA_TARGET_PACKAGE = "target_package"
    private const val UPDATER_ACTIVITY_NAME = "com.example.mobileclock.updater.UpdaterActivity"
    private const val UPDATER_PACKAGE_NAME = "com.example.mobileclock.updater"
}