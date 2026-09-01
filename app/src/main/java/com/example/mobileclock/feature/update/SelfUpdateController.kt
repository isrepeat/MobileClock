package com.example.mobileclock.feature.update

import android.content.Intent
import androidx.activity.ComponentActivity
import androidx.activity.result.IntentSenderRequest
import androidx.lifecycle.lifecycleScope
import com.google.android.gms.auth.api.identity.AuthorizationRequest
import com.google.android.gms.auth.api.identity.AuthorizationResult
import com.google.android.gms.auth.api.identity.Identity
import com.google.android.gms.common.api.Scope
import kotlinx.coroutines.launch

// Управляет пользовательской частью обновления: защитой от двойного нажатия,
// Google OAuth и сообщениями на экране. Работа с сетью вынесена в coordinator.
class SelfUpdateController(
    private val activity: ComponentActivity,
    private val onAuthorizationRequired: (IntentSenderRequest) -> Unit,
    private val onProgress: (String) -> Unit,
    private val onCompleted: (String) -> Unit,
) {
    private val coordinator = SelfUpdateCoordinator(activity)
    private var isRunning = false

    fun start() {
        UpdateDiagnostics.write(activity, "Update button pressed; isRunning=$isRunning")
        if (isRunning) {
            UpdateDiagnostics.write(activity, "Duplicate update request rejected")
            onCompleted("Обновление уже выполняется. Подождите завершения текущего процесса")
            return
        }
        isRunning = true
        // Drive API выдаёт short-lived токен через Google Identity; приложение не
        // хранит пароль или refresh token на устройстве.
        reportProgress("Проверяем доступ к Google Drive…")
        val request = AuthorizationRequest.builder()
            .setRequestedScopes(listOf(Scope(DRIVE_SCOPE)))
            .build()
        Identity.getAuthorizationClient(activity)
            .authorize(request)
            .addOnSuccessListener { result ->
                UpdateDiagnostics.write(activity, "Google Drive authorization request completed")
                handleAuthorizationResult(result)
            }
            .addOnFailureListener { exception ->
                UpdateDiagnostics.write(activity, "Google Drive authorization failed: ${exception.message}")
                finish("Не удалось открыть Google Drive: ${exception.message}")
            }
    }

    fun completeAuthorization(intent: Intent?) {
        // Этот метод вызывается MainActivity после возврата из Google UI.
        UpdateDiagnostics.write(activity, "Google authorization UI returned; hasIntent=${intent != null}")
        try {
            handleAuthorizationResult(Identity.getAuthorizationClient(activity).getAuthorizationResultFromIntent(intent))
        } catch (exception: Exception) {
            finish("Доступ к Google Drive не предоставлен: ${exception.message}")
        }
    }

    fun completeExternalUpdater(message: String) {
        // При отмене установки updater возвращает MobileClock на передний план.
        finish(message)
    }

    private fun handleAuthorizationResult(result: AuthorizationResult) {
        if (result.hasResolution()) {
            UpdateDiagnostics.write(activity, "Google authorization requires user resolution")
            reportProgress("Ожидание авторизации Google…")
            val pendingIntent = result.pendingIntent ?: run {
                finish("Google не вернул экран авторизации.")
                return
            }
            onAuthorizationRequired(IntentSenderRequest.Builder(pendingIntent.intentSender).build())
            return
        }
        val accessToken = result.accessToken ?: run {
            UpdateDiagnostics.write(activity, "Google authorization returned without access token")
            finish("Google не вернул токен доступа к Drive.")
            return
        }
        UpdateDiagnostics.write(activity, "Google Drive access token received")

        activity.lifecycleScope.launch {
            // Coroutine привязана к Activity: при закрытии экрана она не оставит
            // в памяти фоновую операцию со ссылкой на старый UI.
            when (val result = coordinator.downloadAndInstall(accessToken, ::reportProgress)) {
                SelfUpdateResult.NoApk -> finish("В папке APKs нет APK-файлов")
                SelfUpdateResult.NoUpdate -> finish("На Google Drive есть только более старая версия")
                is SelfUpdateResult.InstallationStarted -> installationStarted(result.fileName)
                is SelfUpdateResult.Failed -> finish("Не удалось обновить приложение: ${result.exception.message}")
            }
        }
    }

    private fun finish(message: String) {
        UpdateDiagnostics.write(activity, "Update flow finished: $message")
        isRunning = false
        reportProgress(message)
        onCompleted(message)
    }

    private fun installationStarted(fileName: String) {
        UpdateDiagnostics.write(activity, "Installation flow started for $fileName")
        val message = "$fileName передан MobileClock Updater. Ожидаем установку…"
        reportProgress(message)
        onCompleted(message)
    }

    private fun reportProgress(message: String) {
        activity.runOnUiThread { onProgress(message) }
    }

    private companion object {
        const val DRIVE_SCOPE = "https://www.googleapis.com/auth/drive"
    }
}