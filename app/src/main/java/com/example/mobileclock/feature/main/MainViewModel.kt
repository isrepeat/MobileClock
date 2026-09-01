package com.example.mobileclock.feature.main

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow

class MainViewModel : ViewModel() {
    // Одноразовые события оставляют системные Intent-операции в Activity.
    private val _events = MutableSharedFlow<MainUiEvent>(
        extraBufferCapacity = 1,
        onBufferOverflow = BufferOverflow.DROP_OLDEST,
    )
    val events: SharedFlow<MainUiEvent> = _events

    fun onShareLogsClick() {
        _events.tryEmit(MainUiEvent.ShareLogs)
    }

    fun onExportLogsClick() {
        _events.tryEmit(MainUiEvent.ExportLogs)
    }

    fun onUploadScreenshotToGoogleDriveClick() {
        _events.tryEmit(MainUiEvent.UploadScreenshotToGoogleDrive)
    }

    fun onUpdateApplicationClick() {
        _events.tryEmit(MainUiEvent.UpdateApplication)
    }
}