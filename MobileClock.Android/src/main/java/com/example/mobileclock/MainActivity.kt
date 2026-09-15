package com.example.mobileclock

import android.app.Activity
import android.content.Intent
import android.media.RingtoneManager
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.util.AtomicFile
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import com.example.mobileclock.feature.logs.LogExportCoordinator
import com.example.mobileclock.feature.screenshot.GoogleDriveUploadCoordinator
import com.example.mobileclock.feature.update.SelfUpdateController
import com.example.mobileclock.feature.update.UpdateDiagnostics
import com.example.mobileclock.native.NativeRenderer
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

class MainActivity : ComponentActivity() {
    private data class AlarmMelody(val name: String, val uri: String)

    private lateinit var logExportCoordinator: LogExportCoordinator
    private lateinit var googleDriveUploadCoordinator: GoogleDriveUploadCoordinator
    private lateinit var selfUpdateController: SelfUpdateController
    private val authorizeGoogleDriveUpdate = registerForActivityResult(
        ActivityResultContracts.StartIntentSenderForResult(),
    ) { result ->
        selfUpdateController.completeAuthorization(result.data)
    }

    private val authorizeGoogleDrive = registerForActivityResult(
        ActivityResultContracts.StartIntentSenderForResult(),
    ) { result ->
        googleDriveUploadCoordinator.completeAuthorization(result.data)
    }

    private val chooseAlarmMelody = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) { result ->
        if (result.resultCode != Activity.RESULT_OK) {
            return@registerForActivityResult
        }
        val melodyUri: Uri = result.data?.getParcelableExtra(
            RingtoneManager.EXTRA_RINGTONE_PICKED_URI,
        ) ?: return@registerForActivityResult
        val melodyName = RingtoneManager.getRingtone(this, melodyUri)?.getTitle(this)
            ?: "Выбранная мелодия"
        saveAlarmMelody(melodyName, melodyUri.toString())
        NativeRenderer.dispatch(
            NativeRenderer.AppSessionSignal.ALARM_MELODY_SELECTED,
            melodyName,
            melodyUri.toString(),
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        UpdateDiagnostics.write(this, "MainActivity.onCreate action=${intent.action}")
        handleUpdateCompletion(intent)
        NativeRenderer.initialize(filesDir, assets)
        loadAlarmMelodies().forEach { melody ->
            NativeRenderer.dispatch(
                NativeRenderer.AppSessionSignal.RESTORE_ALARM_MELODY,
                melody.name,
                melody.uri,
            )
        }
        logExportCoordinator = LogExportCoordinator(this, NativeRenderer::flushLogs)
        googleDriveUploadCoordinator = GoogleDriveUploadCoordinator(
            activity = this,
            createLogSnapshot = logExportCoordinator::createSnapshot,
            onAuthorizationRequired = authorizeGoogleDrive::launch,
            onCompleted = ::showNativeStatus,
        )
        selfUpdateController = SelfUpdateController(
            activity = this,
            onAuthorizationRequired = authorizeGoogleDriveUpdate::launch,
            onProgress = { message ->
                NativeRenderer.dispatch(NativeRenderer.AppSessionSignal.SET_STATUS, message)
            },
            onCompleted = ::showNativeStatus,
        )
        NativeRenderer.setCommandHandler(::handleNativeEvent)
        setContentView(com.example.mobileclock.native.NativeRenderSurfaceView(this))
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        UpdateDiagnostics.write(this, "MainActivity.onNewIntent action=${intent.action}")
        setIntent(intent)
        handleUpdateCompletion(intent)
        if (intent.action == ACTION_UPDATE_FAILED && ::selfUpdateController.isInitialized) {
            selfUpdateController.completeExternalUpdater(
                intent.getStringExtra(EXTRA_UPDATE_ERROR) ?: "Установка обновления отменена",
            )
        }
    }

    private fun handleUpdateCompletion(intent: android.content.Intent) {
        val updaterTrace = intent.getStringExtra(EXTRA_UPDATER_TRACE)
        if (!updaterTrace.isNullOrBlank()) {
            UpdateDiagnostics.write(this, "External updater trace:\n$updaterTrace")
        }
        when (intent.action) {
            ACTION_UPDATE_COMPLETED -> UpdateDiagnostics.write(
                this,
                "External updater returned focus after successful installation; " +
                    "session=${intent.getIntExtra(EXTRA_INSTALL_SESSION_ID, -1)}",
            )
            ACTION_UPDATE_FAILED -> UpdateDiagnostics.write(
                this,
                "External updater returned after failure: ${intent.getStringExtra(EXTRA_UPDATE_ERROR)}",
            )
        }
    }

    override fun onStart() {
        super.onStart()
        UpdateDiagnostics.write(this, "MainActivity.onStart")
    }

    override fun onResume() {
        super.onResume()
        UpdateDiagnostics.write(this, "MainActivity.onResume")
    }

    override fun onPause() {
        UpdateDiagnostics.write(this, "MainActivity.onPause")
        super.onPause()
    }

    override fun onStop() {
        UpdateDiagnostics.write(this, "MainActivity.onStop")
        super.onStop()
    }

    override fun onDestroy() {
        UpdateDiagnostics.write(this, "MainActivity.onDestroy changingConfigurations=$isChangingConfigurations")
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        UpdateDiagnostics.write(this, "MainActivity.onWindowFocusChanged hasFocus=$hasFocus")
    }

    private fun handleNativeEvent(signal: Int, value: String, additionalValue: String) {
        when (NativeRenderer.AppSessionSignal.fromValue(signal)) {
            NativeRenderer.AppSessionSignal.REQUEST_ALARM_MELODY -> chooseAlarmMelody()
            NativeRenderer.AppSessionSignal.RESET_ALARM_MELODY_SELECTION -> resetAlarmMelodySelection()
            NativeRenderer.AppSessionSignal.SHARE_LOGS -> shareLogs()
            NativeRenderer.AppSessionSignal.EXPORT_LOGS -> googleDriveUploadCoordinator.startLogUpload()
            NativeRenderer.AppSessionSignal.UPLOAD_SCREENSHOT -> googleDriveUploadCoordinator.startScreenshotUpload()
            NativeRenderer.AppSessionSignal.UPDATE_APPLICATION -> selfUpdateController.start()
            else -> Unit
        }
    }

    private fun shareLogs() {
        if (!logExportCoordinator.share()) {
            showNativeStatus("Не удалось подготовить лог")
        }
    }

    private fun chooseAlarmMelody() {
        chooseAlarmMelody.launch(
            Intent(RingtoneManager.ACTION_RINGTONE_PICKER)
                .putExtra(RingtoneManager.EXTRA_RINGTONE_TYPE, RingtoneManager.TYPE_ALARM)
                .putExtra(RingtoneManager.EXTRA_RINGTONE_SHOW_SILENT, false)
                .putExtra(RingtoneManager.EXTRA_RINGTONE_SHOW_DEFAULT, true),
        )
    }

    private fun resetAlarmMelodySelection() {
        // Legacy: удаляем сохранённые мелодии, созданные версиями до JSON-хранилища.
        getSharedPreferences(ALARM_MELODIES_PREFERENCES, MODE_PRIVATE)
            .edit()
            .remove(ALARM_MELODIES_KEY)
            .apply()
        AtomicFile(alarmMelodiesFile()).delete()
        showNativeStatus("Сохранённые мелодии удалены")
        runCatching {
            startActivity(Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS))
        }.onFailure {
            showNativeStatus("Не удалось открыть настройки приложений")
        }
    }

    private fun loadAlarmMelodies(): List<AlarmMelody> {
        val file = alarmMelodiesFile()
        if (!file.exists()) {
            val legacyMelodies = loadLegacyAlarmMelodies()
            if (legacyMelodies.isNotEmpty() && saveAlarmMelodies(legacyMelodies)) {
                getSharedPreferences(ALARM_MELODIES_PREFERENCES, MODE_PRIVATE)
                    .edit()
                    .remove(ALARM_MELODIES_KEY)
                    .apply()
            }
            return legacyMelodies
        }
        return runCatching {
            AtomicFile(file).openRead().bufferedReader().use { reader ->
                parseAlarmMelodies(reader.readText())
            }
        }.getOrDefault(emptyList())
    }

    private fun saveAlarmMelody(name: String, uri: String) {
        val melodies = loadAlarmMelodies().filterNot { melody -> melody.uri == uri }.toMutableList()
        melodies.add(0, AlarmMelody(name, uri))
        if (!saveAlarmMelodies(melodies)) {
            showNativeStatus("Не удалось сохранить мелодию")
        }
    }

    private fun loadLegacyAlarmMelodies(): List<AlarmMelody> {
        val serialized = getSharedPreferences(ALARM_MELODIES_PREFERENCES, MODE_PRIVATE)
            .getString(ALARM_MELODIES_KEY, null) ?: return emptyList()
        return runCatching {
            parseAlarmMelodies(serialized)
        }.getOrDefault(emptyList())
    }

    private fun parseAlarmMelodies(serialized: String): List<AlarmMelody> {
        val items = JSONArray(serialized)
        return List(items.length()) { index ->
            val item = items.getJSONObject(index)
            AlarmMelody(item.getString("name"), item.getString("uri"))
        }
    }

    private fun saveAlarmMelodies(melodies: List<AlarmMelody>): Boolean = runCatching {
        val serialized = JSONArray().apply {
            melodies.forEach { melody ->
                put(JSONObject().apply {
                    put("name", melody.name)
                    put("uri", melody.uri)
                })
            }
        }
        val file = AtomicFile(alarmMelodiesFile())
        val stream = file.startWrite()
        try {
            stream.write(serialized.toString().toByteArray(Charsets.UTF_8))
            file.finishWrite(stream)
        } catch (error: Exception) {
            file.failWrite(stream)
            throw error
        }
    }.isSuccess

    private fun alarmMelodiesFile(): File = File(filesDir, MOBILECLOCK_STORAGE_FILENAME)

    private fun showNativeStatus(message: String) {
        NativeRenderer.dispatch(NativeRenderer.AppSessionSignal.SET_STATUS, message)
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    private companion object {
        const val ACTION_UPDATE_COMPLETED = "com.example.mobileclock.action.UPDATE_COMPLETED"
        const val ACTION_UPDATE_FAILED = "com.example.mobileclock.action.UPDATE_FAILED"
        const val EXTRA_INSTALL_SESSION_ID = "install_session_id"
        const val EXTRA_UPDATER_TRACE = "updater_trace"
        const val EXTRA_UPDATE_ERROR = "update_error"
        const val ALARM_MELODIES_PREFERENCES = "alarm_melodies"
        const val ALARM_MELODIES_KEY = "items"
        const val MOBILECLOCK_STORAGE_FILENAME = "mobileclock-storage.json"
    }
}