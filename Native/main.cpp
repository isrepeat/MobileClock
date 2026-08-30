#include "NativeApplication.h"

#include <Helpers/platform/Android/Logging.h>

namespace {
    mobileclock::native::NativeApplication& nativeApplication() {
        static mobileclock::native::NativeApplication application;
        return application;
    }
}

// Единственная JNI-точка входа: она только переводит типы JVM в API приложения.
// Состояние Activity, OpenGL ES и XAML UI здесь намеренно не хранятся.
extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSetLogFile(
    JNIEnv* env, jobject, jstring javaLogFilePath) {
    nativeApplication().setLogFile(env, javaLogFilePath);
    LOG_FUNCTION_SCOPE("nativeSetLogFile");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeFlushLogs(JNIEnv*, jobject) {
    LOG_FUNCTION_SCOPE("nativeFlushLogs");
    nativeApplication().flushLogs();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSetAssetManager(
    JNIEnv* env, jobject, jobject javaAssetManager) {
    LOG_FUNCTION_SCOPE("nativeSetAssetManager");
    nativeApplication().setAssetManager(env, javaAssetManager);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSurfaceChanged(
    JNIEnv* env, jobject, jobject androidSurface, jint width, jint height) {
    LOG_FUNCTION_SCOPE("nativeSurfaceChanged: {}x{}", width, height);
    nativeApplication().surfaceChanged(env, androidSurface, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    LOG_FUNCTION_SCOPE("nativeSurfaceDestroyed");
    nativeApplication().surfaceDestroyed();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeTouch(
    JNIEnv*, jobject, jint action, jfloat x, jfloat y) {
    LOG_FUNCTION_SCOPE("nativeTouch: action={}, x={}, y={}", action, x, y);
    nativeApplication().touch(action, x, y);
}