#pragma once

#include <jni.h>
#include <memory>

namespace mobileclock::renderer {
    // Владелец EGL/OpenGL ES-ресурсов и нативной UI-модели одного Surface.
    // Его жизненным циклом управляет NativeApplication, а не JNI-код.
    class NativeRenderer {
    public:
        NativeRenderer();
        ~NativeRenderer();

        NativeRenderer(const NativeRenderer&) = delete;
        NativeRenderer& operator=(const NativeRenderer&) = delete;

        void setLogFile(JNIEnv* env, jstring javaLogFilePath);
        void flushLogs();
        void setAssetManager(JNIEnv* env, jobject javaAssetManager);
        void surfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height);
        void surfaceDestroyed();
        void touch(jint action, jfloat x, jfloat y);

        // Публичен только для реализации в .cpp: скрывает EGL и GL-типы из .h.
        struct State;

    private:
        std::unique_ptr<State> _state;
    };
}