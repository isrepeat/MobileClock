package com.example.mobileclock

import android.app.Activity
import android.content.Intent
import android.media.RingtoneManager
import android.net.Uri
import android.os.Bundle
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
        NativeRenderer.setAlarmMelody(melodyName, melodyUri.toString())
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        UpdateDiagnostics.write(this, "MainActivity.onCreate action=${intent.action}")
        handleUpdateCompletion(intent)
        NativeRenderer.initialize(filesDir, assets)
        loadAlarmMelodies().forEach { melody ->
            NativeRenderer.addAlarmMelody(melody.name, melody.uri)
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
            onProgress = NativeRenderer::setStatus,
            onCompleted = ::showNativeStatus,
        )
        NativeRenderer.setCommandHandler(::handleNativeCommand)
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

    private fun handleNativeCommand(command: String) {
        when (command) {
            "chooseAlarmMelody" -> chooseAlarmMelody()
            "shareLogs" -> shareLogs()
            "exportLogs" -> googleDriveUploadCoordinator.startLogUpload()
            "uploadScreenshot" -> googleDriveUploadCoordinator.startScreenshotUpload()
            "updateApplication" -> selfUpdateController.start()
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

    private fun loadAlarmMelodies(): List<AlarmMelody> {
        val serialized = getSharedPreferences(ALARM_MELODIES_PREFERENCES, MODE_PRIVATE)
            .getString(ALARM_MELODIES_KEY, null) ?: return emptyList()
        return runCatching {
            val items = JSONArray(serialized)
            List(items.length()) { index ->
                val item = items.getJSONObject(index)
                AlarmMelody(item.getString("name"), item.getString("uri"))
            }
        }.getOrDefault(emptyList())
    }

    private fun saveAlarmMelody(name: String, uri: String) {
        val melodies = loadAlarmMelodies().filterNot { melody -> melody.uri == uri }.toMutableList()
        melodies.add(0, AlarmMelody(name, uri))
        val serialized = JSONArray().apply {
            melodies.forEach { melody ->
                put(JSONObject().apply {
                    put("name", melody.name)
                    put("uri", melody.uri)
                })
            }
        }
        getSharedPreferences(ALARM_MELODIES_PREFERENCES, MODE_PRIVATE)
            .edit()
            .putString(ALARM_MELODIES_KEY, serialized.toString())
            .apply()
    }

    private fun showNativeStatus(message: String) {
        NativeRenderer.setStatus(message)
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
    }
}