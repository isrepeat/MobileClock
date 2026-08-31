#pragma once

#include <jni.h>
#include <memory>

namespace mobileclock::renderer {
    // Владелец EGL/OpenGL ES-ресурсов и нативной UI-модели одного Surface.
    // Его жизненным циклом управляет NativeApplication, а не JNI-код.
    class NativeRenderer {
    public:
        // Публичен только для реализации в .cpp: скрывает EGL и GL-типы из .h.
        struct State;

        NativeRenderer();
        ~NativeRenderer();

        NativeRenderer(const NativeRenderer&) = delete;
        NativeRenderer& operator=(const NativeRenderer&) = delete;

        void SetLogFile(JNIEnv* env, jstring javaLogFilePath);
        void FlushLogs();
        void SetAssetManager(JNIEnv* env, jobject javaAssetManager);
        void SurfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height);
        void SurfaceDestroyed();
        void Touch(jint action, jfloat x, jfloat y);

    private:
        std::unique_ptr<State> state;
    };
}