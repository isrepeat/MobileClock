#include "NativeApplication.h"
#include "Renderer/NativeRenderer.h"

#include <Helpers/platform/Android/Logging.h>

namespace mobileclock::native {
    NativeApplication::NativeApplication()
        : _renderer(std::make_unique<mobileclock::renderer::NativeRenderer>()) {
        // Не логируем конструктор: он вызывается до nativeSetLogFile и должен
        // позволить настроить файл логов до первого обращения к логгеру.
    }

    NativeApplication::~NativeApplication() = default;

    void NativeApplication::setLogFile(JNIEnv* env, jstring javaLogFilePath) {
        _renderer->setLogFile(env, javaLogFilePath);
    }

    void NativeApplication::flushLogs() {
        LOG_FUNCTION_SCOPE("NativeApplication::flushLogs");
        _renderer->flushLogs();
    }

    void NativeApplication::setAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("NativeApplication::setAssetManager");
        _renderer->setAssetManager(env, javaAssetManager);
    }

    void NativeApplication::surfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height) {
        LOG_FUNCTION_SCOPE("NativeApplication::surfaceChanged: {}x{}", width, height);
        _renderer->surfaceChanged(env, androidSurface, width, height);
    }

    void NativeApplication::surfaceDestroyed() {
        LOG_FUNCTION_SCOPE("NativeApplication::surfaceDestroyed");
        _renderer->surfaceDestroyed();
    }

    void NativeApplication::touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("NativeApplication::touch: action={}, x={}, y={}", action, x, y);
        _renderer->touch(action, x, y);
    }
}