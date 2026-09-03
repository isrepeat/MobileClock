#include <Helpers.Logging/Logging.h>

#include "NativeApplication.h"

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
    nativeApplication().SetLogFile(env, javaLogFilePath);
    LOG_FUNCTION_SCOPE("MobileClock", "nativeSetLogFile");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeFlushLogs(JNIEnv*, jobject) {
    LOG_FUNCTION_SCOPE("MobileClock", "nativeFlushLogs");
    nativeApplication().FlushLogs();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeLog(
    JNIEnv* env, jobject, jstring javaCategory, jstring javaMessage) {
    nativeApplication().Log(env, javaCategory, javaMessage);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSetAssetManager(
    JNIEnv* env, jobject, jobject javaAssetManager) {
    LOG_FUNCTION_SCOPE("MobileClock", "nativeSetAssetManager");
    nativeApplication().SetAssetManager(env, javaAssetManager);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSurfaceChanged(
    JNIEnv* env, jobject, jobject androidSurface, jint width, jint height) {
    LOG_FUNCTION_SCOPE("MobileClock", "nativeSurfaceChanged: {}x{}", width, height);
    nativeApplication().SurfaceChanged(env, androidSurface, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    LOG_FUNCTION_SCOPE("MobileClock", "nativeSurfaceDestroyed");
    nativeApplication().SurfaceDestroyed();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeTouch(
    JNIEnv*, jobject, jint action, jfloat x, jfloat y) {
    LOG_FUNCTION_SCOPE("MobileClock", "nativeTouch: action={}, x={}, y={}", action, x, y);
    nativeApplication().Touch(action, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_native_NativeRenderer_nativeRender(JNIEnv*, jobject) {
    nativeApplication().Render();
}