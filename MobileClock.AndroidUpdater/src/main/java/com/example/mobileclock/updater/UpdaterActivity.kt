package com.example.mobileclock.updater

import android.app.Activity
import android.app.ActivityOptions
import android.app.PendingIntent
import android.content.Intent
import android.content.pm.PackageInstaller
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.widget.Toast

// Отдельный пакет сохраняет этот Activity живым, пока Android заменяет APK
// основного MobileClock. Поэтому именно updater получает финальный результат
// PackageInstaller и может вернуть пользователю фокус приложения.
class UpdaterActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleIntent(intent)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_UNKNOWN_APP_SOURCES) {
            return
        }
        // После системного экрана разрешений исходный APK и пакет берём из
        // стартового Intent: в result этих данных Android не возвращает.
        val apkUri = intent.data
        val targetPackage = intent.getStringExtra(EXTRA_TARGET_PACKAGE)
        if (apkUri == null || targetPackage == null) {
            fail("Не удалось восстановить запрос установки")
            return
        }
        if (!canInstallPackages()) {
            fail("Разрешение на установку APK не предоставлено")
            return
        }
        install(apkUri, targetPackage)
    }

    private fun handleIntent(intent: Intent) {
        // Новый запрос от MobileClock начинает самостоятельную диагностическую
        // цепочку. Результаты PackageInstaller продолжают эту же цепочку.
        if (intent.action == ACTION_INSTALL_UPDATE) {
            clearTrace()
        }
        log("Activity received action=${intent.action}")
        when (intent.action) {
            ACTION_INSTALL_UPDATE -> prepareInstallation(intent)
            ACTION_INSTALL_RESULT -> handleInstallationResult(intent)
            else -> fail("Неизвестная команда updater’а")
        }
    }

    private fun prepareInstallation(intent: Intent) {
        val apkUri = intent.data
        val targetPackage = intent.getStringExtra(EXTRA_TARGET_PACKAGE)
        if (apkUri == null || targetPackage.isNullOrBlank()) {
            fail("MobileClock не передал APK для установки")
            return
        }
        if (!canInstallPackages()) {
            // Разрешение принадлежит updater-пакету, а не MobileClock, и
            // запрашивается один раз через системный экран Android.
            log("REQUEST_INSTALL_PACKAGES access is missing; opening settings")
            Toast.makeText(
                this,
                "Один раз разрешите установку обновлений для MobileClock Updater",
                Toast.LENGTH_LONG,
            ).show()
            startActivityForResult(
                Intent(
                    Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:$packageName"),
                ),
                REQUEST_UNKNOWN_APP_SOURCES,
            )
            return
        }
        install(apkUri, targetPackage)
    }

    private fun install(apkUri: Uri, targetPackage: String) {
        val installer = packageManager.packageInstaller
        val parameters = PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL).apply {
            setAppPackageName(targetPackage)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_REQUIRED)
            }
        }
        val sessionId = installer.createSession(parameters)
        log("Install session created: session=$sessionId; target=$targetPackage; uri=$apkUri")
        try {
            installer.openSession(sessionId).use { session ->
                // APK уже полностью скачан MobileClock. Updater только копирует
                // его в системную install-session и больше не зависит от файла.
                contentResolver.openInputStream(apkUri)?.use { input ->
                    session.openWrite("base.apk", 0, -1).use { output ->
                        input.copyTo(output, COPY_BUFFER_SIZE)
                        session.fsync(output)
                    }
                } ?: error("Не удалось открыть переданный APK")
                session.commit(installResultPendingIntent(sessionId, targetPackage).intentSender)
                log("Install session committed: session=$sessionId")
            }
        } catch (exception: Exception) {
            installer.abandonSession(sessionId)
            fail("Не удалось передать APK Android: ${exception.message}")
        }
    }

    private fun handleInstallationResult(intent: Intent) {
        val status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE)
        val sessionId = intent.getIntExtra(PackageInstaller.EXTRA_SESSION_ID, INVALID_SESSION_ID)
        val targetPackage = intent.getStringExtra(EXTRA_TARGET_PACKAGE) ?: TARGET_PACKAGE_NAME
        val message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE)
        log("Install result: status=$status; session=$sessionId; message=$message")
        when (status) {
            // Android присылает это событие до показа собственного диалога.
            PackageInstaller.STATUS_PENDING_USER_ACTION -> launchConfirmation(intent)
            // После SUCCESS MobileClock уже заменён, но updater всё ещё жив.
            PackageInstaller.STATUS_SUCCESS -> launchUpdatedApplication(targetPackage, sessionId)
            else -> fail(message ?: "Android не установил обновление (код $status)")
        }
    }

    private fun launchConfirmation(intent: Intent) {
        val confirmation = pendingUserAction(intent)
        if (confirmation == null) {
            fail("Android не вернул окно подтверждения установки")
            return
        }
        log("Starting system installation confirmation")
        // Собственный интерфейс установки не рисуем: это системный диалог,
        // который подтверждает источник APK и подпись пакета.
        startActivity(confirmation)
    }

    private fun launchUpdatedApplication(targetPackage: String, sessionId: Int) {
        val launchIntent = packageManager.getLaunchIntentForPackage(targetPackage)
        if (launchIntent == null) {
            fail("Обновление установлено, но MobileClock не найден")
            return
        }
        log("Installation succeeded; launching $targetPackage; session=$sessionId")
        launchIntent
            .setAction(ACTION_UPDATE_COMPLETED)
            .addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK or
                    Intent.FLAG_ACTIVITY_CLEAR_TOP or
                    Intent.FLAG_ACTIVITY_SINGLE_TOP,
            )
            .putExtra(EXTRA_INSTALL_SESSION_ID, sessionId)
            // Основной пакет добавляет этот след в единый native/Kotlin-лог.
            .putExtra(EXTRA_UPDATER_TRACE, readTrace())
        startActivity(launchIntent)
        finishAndRemoveTask()
    }

    private fun installResultPendingIntent(sessionId: Int, targetPackage: String): PendingIntent =
        // PackageInstaller вызывает этот PendingIntent как системный компонент,
        // поэтому updater можно безопасно вернуть на передний план после установки.
        PendingIntent.getActivity(
            this,
            sessionId,
            Intent(this, UpdaterActivity::class.java)
                .setAction(ACTION_INSTALL_RESULT)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
                .putExtra(EXTRA_TARGET_PACKAGE, targetPackage),
            PendingIntent.FLAG_UPDATE_CURRENT or mutableFlag(),
            creatorActivityOptions(),
        )

    private fun canInstallPackages(): Boolean =
        // До Android 8 отдельное разрешение на установку из неизвестного источника
        // задавалось глобально, а не для конкретного приложения.
        Build.VERSION.SDK_INT < Build.VERSION_CODES.O || packageManager.canRequestPackageInstalls()

    @Suppress("DEPRECATION")
    private fun pendingUserAction(intent: Intent): Intent? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            intent.getParcelableExtra(Intent.EXTRA_INTENT, Intent::class.java)
        } else {
            intent.getParcelableExtra(Intent.EXTRA_INTENT)
        }

    private fun creatorActivityOptions(): Bundle? =
        // Начиная с Android 15 запуск Activity из PendingIntent требует явного
        // разрешения creator-а. Это нужно для возврата MobileClock после SUCCESS.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ActivityOptions.makeBasic()
                .setPendingIntentCreatorBackgroundActivityStartMode(
                    if (Build.VERSION.SDK_INT >= 36) {
                        ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOW_ALWAYS
                    } else {
                        ActivityOptions.MODE_BACKGROUND_ACTIVITY_START_ALLOWED
                    },
                )
                .toBundle()
        } else {
            null
        }

    private fun mutableFlag(): Int =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            PendingIntent.FLAG_MUTABLE
        } else {
            0
        }

    private fun fail(message: String) {
        log("Failure: $message")
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
        packageManager.getLaunchIntentForPackage(TARGET_PACKAGE_NAME)?.let { launchIntent ->
            // При отмене или ошибке возвращаемся в прежний MobileClock, чтобы
            // кнопка обновления разблокировалась и причина попала в его лог.
            launchIntent
                .setAction(ACTION_UPDATE_FAILED)
                .addFlags(
                    Intent.FLAG_ACTIVITY_NEW_TASK or
                        Intent.FLAG_ACTIVITY_CLEAR_TOP or
                        Intent.FLAG_ACTIVITY_SINGLE_TOP,
                )
                .putExtra(EXTRA_UPDATER_TRACE, readTrace())
                .putExtra(EXTRA_UPDATE_ERROR, message)
            startActivity(launchIntent)
        }
        finishAndRemoveTask()
    }

    private fun log(message: String) {
        Log.i(LOG_TAG, message)
        // Журнал живёт в sandbox updater-а до финального статуса, затем целиком
        // передаётся MobileClock в EXTRA_UPDATER_TRACE.
        val trace = readTrace()
        val entry = "${System.currentTimeMillis()}: $message"
        preferences().edit()
            .putString(KEY_TRACE, (trace + "\n" + entry).takeLast(MAX_TRACE_LENGTH))
            .apply()
    }

    private fun clearTrace() {
        preferences().edit().remove(KEY_TRACE).apply()
    }

    private fun readTrace(): String = preferences().getString(KEY_TRACE, "").orEmpty().trim()

    private fun preferences() = getSharedPreferences(PREFERENCES_NAME, MODE_PRIVATE)

    private companion object {
        const val ACTION_INSTALL_RESULT = "com.example.mobileclock.updater.action.INSTALL_RESULT"
        const val ACTION_INSTALL_UPDATE = "com.example.mobileclock.updater.action.INSTALL_UPDATE"
        const val ACTION_UPDATE_COMPLETED = "com.example.mobileclock.action.UPDATE_COMPLETED"
        const val ACTION_UPDATE_FAILED = "com.example.mobileclock.action.UPDATE_FAILED"
        const val COPY_BUFFER_SIZE = 64 * 1024
        const val EXTRA_INSTALL_SESSION_ID = "install_session_id"
        const val EXTRA_TARGET_PACKAGE = "target_package"
        const val EXTRA_UPDATER_TRACE = "updater_trace"
        const val EXTRA_UPDATE_ERROR = "update_error"
        const val INVALID_SESSION_ID = -1
        const val KEY_TRACE = "trace"
        const val LOG_TAG = "MobileClockUpdater"
        const val MAX_TRACE_LENGTH = 16_384
        const val PREFERENCES_NAME = "updater_diagnostics"
        const val REQUEST_UNKNOWN_APP_SOURCES = 1001
        const val TARGET_PACKAGE_NAME = "com.example.mobileclock"
    }
}