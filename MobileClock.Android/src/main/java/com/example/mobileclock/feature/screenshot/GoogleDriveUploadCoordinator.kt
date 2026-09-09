package com.example.mobileclock.feature.screenshot

import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.PixelCopy
import android.view.View
import android.view.ViewGroup
import androidx.activity.ComponentActivity
import androidx.activity.result.IntentSenderRequest
import androidx.lifecycle.lifecycleScope
import com.example.mobileclock.native.NativeRenderSurfaceView
import com.example.mobileclock.native.NativeRenderer
import com.google.android.gms.auth.api.identity.AuthorizationRequest
import com.google.android.gms.auth.api.identity.AuthorizationResult
import com.google.android.gms.auth.api.identity.Identity
import com.google.android.gms.common.api.Scope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.coroutines.resume

// Один OAuth-сеанс обслуживает две операции: загрузку готового лога и снимка
// экрана. Это не смешивается с авторизацией, используемой обновлением APK.
class GoogleDriveUploadCoordinator(
    private val activity: ComponentActivity,
    private val createLogSnapshot: () -> File?,
    private val onAuthorizationRequired: (IntentSenderRequest) -> Unit,
    private val onCompleted: (String) -> Unit,
) {
    private var pendingUpload: PendingUpload? = null
    private var isRunning = false

    fun startScreenshotUpload() {
        authorize(PendingUpload.Screenshot)
    }

    fun startLogUpload() {
        authorize(PendingUpload.Logs)
    }

    fun completeAuthorization(intent: android.content.Intent?) {
        try {
            handleAuthorizationResult(
                Identity.getAuthorizationClient(activity).getAuthorizationResultFromIntent(intent),
            )
        } catch (exception: Exception) {
            finish("Доступ к Google Drive не предоставлен: ${exception.message}")
        }
    }

    private fun authorize(upload: PendingUpload) {
        if (isRunning) {
            onCompleted("Загрузка в Google Drive уже выполняется")
            return
        }
        log("Drive upload requested: $upload")
        isRunning = true
        // Сохраняем выбранную операцию, пока Google при необходимости показывает
        // собственный экран выдачи разрешения.
        pendingUpload = upload
        val request = AuthorizationRequest.builder()
            .setRequestedScopes(listOf(Scope(DRIVE_SCOPE)))
            .build()
        Identity.getAuthorizationClient(activity)
            .authorize(request)
            .addOnSuccessListener(::handleAuthorizationResult)
            .addOnFailureListener { exception ->
                finish("Не удалось авторизоваться в Google: ${exception.message}")
            }
    }

    private fun handleAuthorizationResult(result: AuthorizationResult) {
        if (result.hasResolution()) {
            val pendingIntent = result.pendingIntent ?: run {
                finish("Google не вернул экран авторизации.")
                return
            }
            onAuthorizationRequired(IntentSenderRequest.Builder(pendingIntent.intentSender).build())
            return
        }
        val accessToken = result.accessToken ?: run {
            finish("Google не вернул токен доступа к Drive.")
            return
        }
        when (pendingUpload) {
            PendingUpload.Screenshot -> uploadScreenshot(accessToken)
            PendingUpload.Logs -> uploadLogs(accessToken)
            null -> finish("Не выбрана операция загрузки в Google Drive")
        }
    }

    private fun uploadScreenshot(accessToken: String) {
        activity.lifecycleScope.launch {
            try {
                log("Capturing screenshot for Drive upload")
                val screenshot = captureScreenshot()
                val file = withContext(Dispatchers.IO) { saveScreenshot(screenshot) }
                screenshot.recycle()
                withContext(Dispatchers.IO) { upload(file, SCREENSHOT_MIME_TYPE, accessToken) }
                file.delete()
                finish("Скриншот загружен в Google Drive")
            } catch (exception: Exception) {
                finish("Не удалось загрузить скриншот: ${exception.message}")
            }
        }
    }

    private fun uploadLogs(accessToken: String) {
        activity.lifecycleScope.launch {
            try {
                log("Creating unified native/Kotlin log snapshot for Drive upload")
                val file = withContext(Dispatchers.IO) {
                    createLogSnapshot() ?: error("Лог ещё не создан")
                }
                withContext(Dispatchers.IO) { upload(file, LOG_MIME_TYPE, accessToken) }
                file.delete()
                finish("Логи загружены в папку Screens на Google Drive")
            } catch (exception: Exception) {
                finish("Не удалось загрузить логи: ${exception.message}")
            }
        }
    }

    private suspend fun captureScreenshot(): Bitmap = suspendCancellableCoroutine { continuation ->
        val rootView = activity.window.decorView.rootView
        val bitmap = Bitmap.createBitmap(rootView.width, rootView.height, Bitmap.Config.ARGB_8888)
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // На старых версиях Android PixelCopy ещё недоступен.
            rootView.draw(Canvas(bitmap))
            continuation.resume(bitmap)
            return@suspendCancellableCoroutine
        }

        // Canvas не считывает содержимое отдельной OpenGL Surface. PixelCopy
        // снимает сам Surface, а Android-элементы накладываются вторым слоем.
        val nativeSurface = findNativeRenderSurfaceView(rootView)
        if (nativeSurface == null || !nativeSurface.holder.surface.isValid) {
            bitmap.recycle()
            continuation.resumeWith(Result.failure(IllegalStateException("OpenGL surface недоступен.")))
            return@suspendCancellableCoroutine
        }
        val nativeBitmap = Bitmap.createBitmap(
            nativeSurface.width,
            nativeSurface.height,
            Bitmap.Config.ARGB_8888,
        )
        val overlayBitmap = Bitmap.createBitmap(
            rootView.width,
            rootView.height,
            Bitmap.Config.ARGB_8888,
        )
        rootView.draw(Canvas(overlayBitmap))

        PixelCopy.request(nativeSurface.holder.surface, nativeBitmap, { result ->
            if (result == PixelCopy.SUCCESS) {
                val rootLocation = IntArray(2)
                val surfaceLocation = IntArray(2)
                rootView.getLocationInWindow(rootLocation)
                nativeSurface.getLocationInWindow(surfaceLocation)
                nativeBitmap.setHasAlpha(false)
                Canvas(bitmap).apply {
                    drawBitmap(
                        nativeBitmap,
                        (surfaceLocation[0] - rootLocation[0]).toFloat(),
                        (surfaceLocation[1] - rootLocation[1]).toFloat(),
                        null,
                    )
                    drawBitmap(overlayBitmap, 0.0f, 0.0f, null)
                }
                overlayBitmap.recycle()
                nativeBitmap.recycle()
                continuation.resume(bitmap)
            } else {
                overlayBitmap.recycle()
                nativeBitmap.recycle()
                bitmap.recycle()
                continuation.resumeWith(Result.failure(IllegalStateException("Ошибка PixelCopy: $result")))
            }
        }, Handler(Looper.getMainLooper()))
    }

    private fun findNativeRenderSurfaceView(view: View): NativeRenderSurfaceView? {
        if (view is NativeRenderSurfaceView) {
            return view
        }
        if (view !is ViewGroup) {
            return null
        }
        for (index in 0 until view.childCount) {
            val nativeSurface = findNativeRenderSurfaceView(view.getChildAt(index))
            if (nativeSurface != null) {
                return nativeSurface
            }
        }
        return null
    }

    private fun saveScreenshot(bitmap: Bitmap): File {
        val directory = File(activity.cacheDir, "google-drive-screenshots").apply { mkdirs() }
        val name = "MobileClock_${newFileTimeFormatter().format(Date())}.png"
        return File(directory, name).also { file ->
            FileOutputStream(file).use { output ->
                check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, output)) { "Не удалось сохранить PNG." }
            }
        }
    }

    private fun upload(file: File, mimeType: String, accessToken: String) {
        // Google Drive принимает файл вместе с именем и ID папки одним multipart
        // запросом; папка Screens используется и для скриншотов, и для логов.
        val boundary = "MobileClock${System.currentTimeMillis()}"
        val connection = (URL(UPLOAD_URL).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            setChunkedStreamingMode(0)
            setRequestProperty("Authorization", "Bearer $accessToken")
            setRequestProperty("Content-Type", "multipart/related; boundary=$boundary")
        }
        connection.outputStream.buffered().use { output ->
            output.write("--$boundary\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n".toByteArray())
            output.write(
                "{\"name\":\"${file.name}\",\"mimeType\":\"$mimeType\",\"parents\":[\"$DRIVE_FOLDER_ID\"]}"
                    .toByteArray(),
            )
            output.write("\r\n--$boundary\r\nContent-Type: $mimeType\r\n\r\n".toByteArray())
            file.inputStream().use { input -> input.copyTo(output) }
            output.write("\r\n--$boundary--\r\n".toByteArray())
        }
        check(connection.responseCode in 200..299) {
            "Google Drive вернул HTTP ${connection.responseCode}: ${connection.errorStream?.bufferedReader()?.use { it.readText() }}"
        }
    }

    private fun finish(message: String) {
        // Сброс флага важен после любой ошибки: пользователь сразу может
        // повторить ту же операцию без перезапуска приложения.
        log(message)
        pendingUpload = null
        isRunning = false
        onCompleted(message)
    }

    private fun log(message: String) {
        NativeRenderer.log(activity.filesDir, LOG_CATEGORY, message)
    }

    private enum class PendingUpload {
        Screenshot,
        Logs,
    }

    private companion object {
        const val DRIVE_FOLDER_ID = "1215FwAl5PlQn99508-jUkRgHyPWgygoD"
        const val DRIVE_SCOPE = "https://www.googleapis.com/auth/drive"
        const val LOG_MIME_TYPE = "text/plain"
        const val LOG_CATEGORY = "GoogleDrive"
        const val SCREENSHOT_MIME_TYPE = "image/png"
        const val UPLOAD_URL = "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart"

        fun newFileTimeFormatter() = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US)
    }
}