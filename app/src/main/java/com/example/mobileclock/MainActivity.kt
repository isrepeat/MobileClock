package com.example.mobileclock

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.example.mobileclock.feature.logs.LogExportCoordinator
import com.example.mobileclock.feature.main.MainScreen
import com.example.mobileclock.feature.main.MainUiEvent
import com.example.mobileclock.feature.main.MainViewModel
import com.example.mobileclock.feature.screenshot.GoogleDriveUploadCoordinator
import com.example.mobileclock.feature.update.SelfUpdateController
import com.example.mobileclock.feature.update.UpdateDiagnostics
import com.example.mobileclock.native.NativeRenderer
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()
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
            onCompleted = { message -> Toast.makeText(this, message, Toast.LENGTH_SHORT).show() },
        )
        val mainScreen = MainScreen(this)
        selfUpdateController = SelfUpdateController(
            activity = this,
            onAuthorizationRequired = authorizeGoogleDriveUpdate::launch,
            onProgress = mainScreen::showUpdateStatus,
            onCompleted = { message -> Toast.makeText(this, message, Toast.LENGTH_SHORT).show() },
        )
        setContentView(
            mainScreen.createView(
                onShareLogs = viewModel::onShareLogsClick,
                onExportLogs = viewModel::onExportLogsClick,
                onUploadScreenshotToGoogleDrive = viewModel::onUploadScreenshotToGoogleDriveClick,
                onUpdateApplication = viewModel::onUpdateApplicationClick,
            ),
        )
        observeUiEvents()
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

    private fun observeUiEvents() {
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.events.collect { event ->
                    when (event) {
                        MainUiEvent.ShareLogs -> shareLogs()
                        MainUiEvent.ExportLogs -> googleDriveUploadCoordinator.startLogUpload()
                        MainUiEvent.UploadScreenshotToGoogleDrive -> googleDriveUploadCoordinator.startScreenshotUpload()
                        MainUiEvent.UpdateApplication -> selfUpdateController.start()
                    }
                }
            }
        }
    }

    private fun shareLogs() {
        if (!logExportCoordinator.share()) {
            Toast.makeText(this, "Не удалось подготовить лог", Toast.LENGTH_SHORT).show()
        }
    }

    private companion object {
        const val ACTION_UPDATE_COMPLETED = "com.example.mobileclock.action.UPDATE_COMPLETED"
        const val ACTION_UPDATE_FAILED = "com.example.mobileclock.action.UPDATE_FAILED"
        const val EXTRA_INSTALL_SESSION_ID = "install_session_id"
        const val EXTRA_UPDATER_TRACE = "updater_trace"
        const val EXTRA_UPDATE_ERROR = "update_error"
    }
}