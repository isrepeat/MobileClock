#pragma once
#include <jni.h>

#include <memory>

namespace mobileclock::renderer {
    // Владелец EGL/OpenGL ES-ресурсов и нативной UI-модели одного Surface.
    // Его жизненным циклом управляет NativeApplication, а не JNI-код.
    class NativeRenderer final {
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
        void SetCommandDispatcher(JNIEnv* env, jobject javaDispatcher);
        void SetStatus(JNIEnv* env, jstring javaStatus);
        void AddAlarmMelody(JNIEnv* env, jstring javaName, jstring javaUri);
        void SetAlarmMelody(JNIEnv* env, jstring javaName, jstring javaUri);
        void SurfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height);
        void SurfaceDestroyed();
        void Touch(jint action, jfloat x, jfloat y);
        void Render();

    private:
        std::unique_ptr<State> state;
    };
}