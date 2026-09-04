package com.example.mobileclock.native

import android.content.res.AssetManager
import android.view.Surface
import java.io.File

object NativeRenderer {
    private var isLogFileConfigured = false

    init {
        // Загружает libmobileclock.so из APK. После этого ART может вызвать
        // экспортированные JNI-функции из Native/main.cpp.
        System.loadLibrary("mobileclock")
    }

    fun initialize(filesDirectory: File, assetManager: AssetManager) {
        // Kotlin подготавливает Android-зависимые объекты до первого GL-кадра.
        configureLogFile(filesDirectory)
        nativeSetAssetManager(assetManager)
        nativeSetCommandDispatcher(NativeCommandDispatcher)
    }

    fun setCommandHandler(handler: (String) -> Unit) {
        NativeCommandDispatcher.handler = handler
    }

    fun setStatus(message: String) {
        nativeSetStatus(message)
    }

    fun log(filesDirectory: File, category: String, message: String) {
        configureLogFile(filesDirectory)
        nativeLog(category, message)
    }

    fun onSurfaceChanged(surface: Surface, width: Int, height: Int) {
        nativeSurfaceChanged(surface, width, height)
    }

    fun onSurfaceDestroyed() {
        nativeSurfaceDestroyed()
    }

    fun onTouch(action: Int, x: Float, y: Float) {
        nativeTouch(action, x, y)
    }

    fun render() {
        nativeRender()
    }

    fun flushLogs() {
        nativeFlushLogs()
    }

    @Synchronized
    private fun configureLogFile(filesDirectory: File) {
        if (isLogFileConfigured) {
            return
        }
        val logFile = File(filesDirectory, "logs/mobileclock.log")
        logFile.parentFile?.mkdirs()
        nativeSetLogFile(logFile.absolutePath)
        isLogFileConfigured = true
    }

    // У external-методов нет Kotlin-тела: вызов переходит в JNI. ART ищет
    // C++-символ Java_com_example_mobileclock_native_NativeRenderer_<имя метода>
    // в libmobileclock.so. Этот символ определён в Native/main.cpp.
    //
    // Примеры преобразования аргументов: Surface/AssetManager -> jobject,
    // Int -> jint, Float -> jfloat, String -> jstring.
    private external fun nativeSurfaceChanged(surface: Surface, width: Int, height: Int)
    private external fun nativeSetAssetManager(assetManager: AssetManager)
    private external fun nativeSetCommandDispatcher(dispatcher: NativeCommandDispatcher)
    private external fun nativeSetStatus(status: String)
    private external fun nativeSetLogFile(path: String)
    private external fun nativeFlushLogs()
    private external fun nativeLog(category: String, message: String)
    private external fun nativeSurfaceDestroyed()
    private external fun nativeTouch(action: Int, x: Float, y: Float)
    private external fun nativeRender()
}

object NativeCommandDispatcher {
    @Volatile
    var handler: ((String) -> Unit)? = null

    // Вызывается C++ только после успешного отпускания touch на native-кнопке.
    fun dispatch(command: String) {
        handler?.invoke(command)
    }
}