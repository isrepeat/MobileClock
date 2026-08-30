package com.example.mobileclock.feature.main

sealed interface MainUiEvent {
    data object ShareLogs : MainUiEvent
    data object ExportLogs : MainUiEvent
}