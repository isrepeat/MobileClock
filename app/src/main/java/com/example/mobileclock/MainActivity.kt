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
import com.example.mobileclock.native.NativeRenderer
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()
    private lateinit var logExportCoordinator: LogExportCoordinator

    private val exportLogs = registerForActivityResult(
        ActivityResultContracts.CreateDocument("text/plain"),
    ) { destination ->
        if (destination == null) return@registerForActivityResult
        val exported = logExportCoordinator.export(destination)
        Toast.makeText(
            this,
            if (exported) "Лог экспортирован" else "Не удалось экспортировать лог",
            Toast.LENGTH_SHORT,
        ).show()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        NativeRenderer.initialize(filesDir, assets)
        logExportCoordinator = LogExportCoordinator(this, NativeRenderer::flushLogs)
        setContentView(
            MainScreen(this).createView(
                onShareLogs = viewModel::onShareLogsClick,
                onExportLogs = viewModel::onExportLogsClick,
            ),
        )
        observeUiEvents()
    }

    private fun observeUiEvents() {
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) {
                viewModel.events.collect { event ->
                    when (event) {
                        MainUiEvent.ShareLogs -> shareLogs()
                        MainUiEvent.ExportLogs -> exportLogs.launch(logExportCoordinator.newExportFileName())
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
}