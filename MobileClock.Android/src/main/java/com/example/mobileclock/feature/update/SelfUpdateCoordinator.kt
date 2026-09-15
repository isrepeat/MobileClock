package com.example.mobileclock.feature.update

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder

// Выполняет сетевую часть обновления: находит APK в Drive, скачивает его и
// проверяет до того, как отдельный updater начнёт системную установку.
class SelfUpdateCoordinator(private val context: Context) {
    suspend fun downloadAndInstall(
        accessToken: String,
        onProgress: (String) -> Unit,
    ): SelfUpdateResult = withContext(Dispatchers.IO) {
        try {
            onProgress("Ищем последнюю сборку в Google Drive…")
            val driveFile = findLatestApk(accessToken) ?: return@withContext SelfUpdateResult.NoApk
            UpdateDiagnostics.write(context, "Latest Drive APK selected: ${driveFile.name}; id=${driveFile.id}")
            onProgress("Найдена ${driveFile.name}. Скачивание: 0%")
            val apk = downloadApk(driveFile, accessToken, onProgress)
            UpdateDiagnostics.write(context, "APK downloaded: path=${apk.absolutePath}; bytes=${apk.length()}")
            onProgress("Проверяем скачанный APK…")
            val archive = context.packageManager.getPackageArchiveInfo(apk.absolutePath, 0)
                ?: error("Google Drive вернул некорректный APK.")
            // Нельзя передавать updater-у APK с другим package name, даже если
            // файл оказался в той же папке Drive.
            if (archive.packageName != context.packageName) {
                apk.delete()
                error("APK предназначен для пакета ${archive.packageName}, а не ${context.packageName}.")
            }
            val installedVersionCode = installedVersionCode()
            UpdateDiagnostics.write(
                context,
                "APK validated: package=${archive.packageName}; candidateVersion=${archive.versionCode}; installedVersion=$installedVersionCode",
            )
            if (archive.versionCode.toLong() < installedVersionCode) {
                apk.delete()
                return@withContext SelfUpdateResult.NoUpdate
            }

            onProgress("Запускаем MobileClock Updater…")
            // startActivity должен выполняться на main thread. После передачи
            // URI updater владеет install-session и переживает замену MobileClock.
            withContext(Dispatchers.Main) {
                ExternalUpdaterLauncher.launch(context, apk)
            }
            SelfUpdateResult.InstallationStarted(archive.versionCode.toLong(), driveFile.name)
        } catch (exception: Exception) {
            UpdateDiagnostics.write(
                context,
                "Update coordinator failed: ${exception::class.java.simpleName}: ${exception.message}",
            )
            SelfUpdateResult.Failed(exception)
        }
    }

    private fun findLatestApk(accessToken: String): DriveFile? {
        // В папке находится также MobileClockUpdater.apk. Регулярное выражение
        // отбирает только versioned APK основного приложения.
        val query = "'$DRIVE_FOLDER_ID' in parents and trashed = false"
        val fields = "files(id,name,modifiedTime)"
        val url = URL(
            "$DRIVE_FILES_URL?q=${encode(query)}&orderBy=modifiedTime%20desc&pageSize=100&fields=${encode(fields)}",
        )
        val connection = openConnection(url, accessToken)
        val response = connection.inputStream.bufferedReader().use { it.readText() }
        val files = JSONObject(response).getJSONArray("files")
        val apkFiles = mutableListOf<DriveFile>()
        for (index in 0 until files.length()) {
            val file = files.getJSONObject(index)
            val name = file.getString("name")
            if (VERSIONED_APK_PATTERN.matches(name)) {
                apkFiles += DriveFile(file.getString("id"), name)
            }
        }
        return apkFiles.maxByOrNull { file -> publishedVersionCode(file.name) ?: Long.MIN_VALUE }
    }

    private fun downloadApk(
        driveFile: DriveFile,
        accessToken: String,
        onProgress: (String) -> Unit,
    ): File {
        val directory = File(context.cacheDir, "self-updates").apply { mkdirs() }
        val apk = File(directory, "update.apk")
        val connection = openConnection(URL("$DRIVE_FILES_URL/${driveFile.id}?alt=media"), accessToken)
        val totalBytes = connection.contentLengthLong
        var downloadedBytes = 0L
        var reportedPercent = 0
        connection.inputStream.use { input ->
            apk.outputStream().use { output ->
                val buffer = ByteArray(DOWNLOAD_BUFFER_SIZE)
                while (true) {
                    val readBytes = input.read(buffer)
                    if (readBytes < 0) {
                        break
                    }
                    output.write(buffer, 0, readBytes)
                    downloadedBytes += readBytes
                    if (totalBytes > 0L) {
                        // UI обновляется с шагом 1%, а не после каждого буфера.
                        val percent = (downloadedBytes * 100L / totalBytes).toInt()
                        if (percent >= reportedPercent + PROGRESS_STEP_PERCENT) {
                            reportedPercent = percent
                            onProgress("Скачивание: $percent%")
                        }
                    }
                }
            }
        }
        if (totalBytes > 0L) {
            onProgress("Скачивание: 100%")
        }
        return apk
    }

    private fun installedVersionCode(): Long = context.packageManager
        .getPackageInfo(context.packageName, 0)
        .versionCode
        .toLong()

    private fun openConnection(url: URL, accessToken: String): HttpURLConnection =
        // Drive закрыт OAuth-токеном: ссылки на APK не вшиваются в приложение.
        (url.openConnection() as HttpURLConnection).apply {
            connectTimeout = CONNECTION_TIMEOUT_MILLIS
            readTimeout = READ_TIMEOUT_MILLIS
            instanceFollowRedirects = true
            requestMethod = "GET"
            setRequestProperty("Authorization", "Bearer $accessToken")
            connect()
            check(responseCode in 200..299) {
                "Google Drive вернул HTTP $responseCode: ${errorStream?.bufferedReader()?.use { it.readText() }}"
            }
        }

    private fun encode(value: String): String = URLEncoder.encode(value, Charsets.UTF_8.name())

    private fun publishedVersionCode(fileName: String): Long? = VERSIONED_APK_PATTERN
        .matchEntire(fileName)
        ?.groupValues
        ?.get(1)
        ?.toLongOrNull()

    private data class DriveFile(val id: String, val name: String)

    private companion object {
        const val CONNECTION_TIMEOUT_MILLIS = 15_000
        const val DOWNLOAD_BUFFER_SIZE = 64 * 1024
        const val DRIVE_FILES_URL = "https://www.googleapis.com/drive/v3/files"
        const val DRIVE_FOLDER_ID = "1w7RHJCRjhIpHU2uUL2lB6qrofxbjWPV1"
        const val PROGRESS_STEP_PERCENT = 1
        const val READ_TIMEOUT_MILLIS = 120_000
        val VERSIONED_APK_PATTERN = Regex("MobileClock-(\\d+)-.+\\.apk", RegexOption.IGNORE_CASE)
    }
}

sealed interface SelfUpdateResult {
    data object NoApk : SelfUpdateResult
    data object NoUpdate : SelfUpdateResult
    data class InstallationStarted(val versionCode: Long, val fileName: String) : SelfUpdateResult
    data class Failed(val exception: Exception) : SelfUpdateResult
}