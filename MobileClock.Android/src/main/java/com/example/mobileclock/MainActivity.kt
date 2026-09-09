package com.example.mobileclock

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import com.example.mobileclock.feature.logs.LogExportCoordinator
import com.example.mobileclock.feature.screenshot.GoogleDriveUploadCoordinator
import com.example.mobileclock.feature.update.SelfUpdateController
import com.example.mobileclock.feature.update.UpdateDiagnostics
import com.example.mobileclock.native.NativeRenderer

class MainActivity : ComponentActivity() {
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        UpdateDiagnostics.write(this, "MainActivity.onCreate action=${intent.action}")
        handleUpdateCompletion(intent)
        NativeRenderer.initialize(filesDir, assets)
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
    }
}