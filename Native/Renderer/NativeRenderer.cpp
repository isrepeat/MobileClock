#include <Helpers.Logging/Logging.h>
#include <ESRenderer/OpenGlRenderer.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/input.h>
#include <EGL/egl.h>

#include "../UI/PageManager.h"
#include "AnimationRenderers.h"
#include "AnimationShaders.h"
#include "AssetsManager.h"
#include "NativeRenderer.h"

#include <filesystem>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

namespace mobileclock::renderer {
    struct NativeRenderer::State {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
        ANativeWindow* window = nullptr;
        std::unique_ptr<AssetsManager> assetsManager;
        JavaVM* javaVm = nullptr;
        jobject commandDispatcher = nullptr;
        jmethodID dispatchCommand = nullptr;
        mobileclock::ui::PageManager pageManager;
        xaml::RendererRegistry renderers;
        std::unique_ptr<es_renderer::OpenGlRenderer> renderer;
    };
}

namespace mobileclock::renderer::_details {
    // Схема кадра для Button с renderer="wave-outline":
    // Choreographer.doFrame()
    // └─ NativeRenderSurfaceView.doFrame()
    //    ├─ NativeRenderer.render() [Kotlin]
    //    │  └─ nativeRender() [JNI]
    //    │     └─ NativeApplication::Render()
    //    │        └─ NativeRenderer::Render()
    //    │           └─ DrawPage()
    //    │              ├─ PageManager::UpdateClock()
    //    │              │  └─ AnimationController::Update() обновляет WaveProgress и WaveOpacity
    //    │              ├─ PageManager::Render()
    //    │              │  └─ MainPageViewModel::Render()
    //    │              │     └─ xaml::Render()
    //    │              │        └─ xaml::_details::RenderElement()
    //    │              │           └─ RendererRegistry::Render()
    //    │              │              └─ RenderWaveOutline()
    //    │              │                 ├─ context.RenderDefaultElement()
    //    │              │                 │  ├─ RenderChrome()
    //    │              │                 │  ├─ RenderButtonWave()
    //    │              │                 │  └─ DrawText()
    //    │              │                 └─ DrawRoundedRectOutline() для дополнительной обводки
    //    │              └─ eglSwapBuffers() показывает завершённый кадр.
    //    └─ Choreographer.postFrameCallback() планирует следующий VSync.
    void DrawPage(NativeRenderer::State& state) {
        if (state.renderer == nullptr) {
            return;
        }
        state.renderer->BeginFrame();
        state.pageManager.UpdateClock();
        state.pageManager.Render(*state.renderer, state.renderers);
        eglSwapBuffers(state.display, state.surface);
    }

    void DestroyRenderer(NativeRenderer::State& state) {
        if (state.display != EGL_NO_DISPLAY) {
            if (state.context != EGL_NO_CONTEXT && state.surface != EGL_NO_SURFACE) {
                eglMakeCurrent(state.display, state.surface, state.surface, state.context);
            }
            state.renderer.reset();
            eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (state.context != EGL_NO_CONTEXT) {
                eglDestroyContext(state.display, state.context);
            }
            if (state.surface != EGL_NO_SURFACE) {
                eglDestroySurface(state.display, state.surface);
            }
            eglTerminate(state.display);
        }
        else {
            state.renderer.reset();
        }
        if (state.window != nullptr) {
            ANativeWindow_release(state.window);
        }
        state.display = EGL_NO_DISPLAY;
        state.surface = EGL_NO_SURFACE;
        state.context = EGL_NO_CONTEXT;
        state.window = nullptr;
    }

    void DispatchCommand(NativeRenderer::State& state, const std::string& command) {
        if (state.javaVm == nullptr || state.commandDispatcher == nullptr || state.dispatchCommand == nullptr) {
            return;
        }
        JNIEnv* env = nullptr;
        bool isAttached = false;
        if (state.javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (state.javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                return;
            }
            isAttached = true;
        }
        jstring javaCommand = env->NewStringUTF(command.c_str());
        if (javaCommand != nullptr) {
            env->CallVoidMethod(state.commandDispatcher, state.dispatchCommand, javaCommand);
            env->DeleteLocalRef(javaCommand);
        }
        if (isAttached) {
            state.javaVm->DetachCurrentThread();
        }
    }
}

namespace mobileclock::renderer {
    NativeRenderer::NativeRenderer()
        : state(std::make_unique<State>()) {
        RegisterAnimationRenderers(this->state->renderers);
        this->state->pageManager.SetCommandHandler([this](const std::string& command) {
            _details::DispatchCommand(*this->state, command);
        });
    }

    NativeRenderer::~NativeRenderer() {
        this->SurfaceDestroyed();
        if (this->state->commandDispatcher != nullptr && this->state->javaVm != nullptr) {
            JNIEnv* env = nullptr;
            if (this->state->javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
                env->DeleteGlobalRef(this->state->commandDispatcher);
            }
        }
    }

    //
    // API
    //
    void NativeRenderer::SetLogFile(JNIEnv* env, jstring javaLogFilePath) {
        const char* utf8Path = env->GetStringUTFChars(javaLogFilePath, nullptr);
        if (utf8Path == nullptr) {
            return;
        }
        utility_helpers::logging::Configure({
            std::filesystem::path(utf8Path),
        });
        utility_helpers::logging::Initialize("MobileClock");
        env->ReleaseStringUTFChars(javaLogFilePath, utf8Path);
    }

    void NativeRenderer::FlushLogs() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::FlushLogs");
        utility_helpers::logging::Flush();
    }

    void NativeRenderer::SetAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SetAssetManager");
        utility_helpers::logging::Initialize("MobileClock");
        this->state->assetsManager = std::make_unique<AssetsManager>(env, javaAssetManager);
        LOG_INFO("MobileClock", "Android AssetManager connected");
    }

    void NativeRenderer::SetCommandDispatcher(JNIEnv* env, jobject javaDispatcher) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SetCommandDispatcher");
        State& state = *this->state;
        env->GetJavaVM(&state.javaVm);
        if (state.commandDispatcher != nullptr) {
            env->DeleteGlobalRef(state.commandDispatcher);
        }
        state.commandDispatcher = env->NewGlobalRef(javaDispatcher);
        const jclass dispatcherClass = env->GetObjectClass(javaDispatcher);
        state.dispatchCommand = env->GetMethodID(dispatcherClass, "dispatch", "(Ljava/lang/String;)V");
        env->DeleteLocalRef(dispatcherClass);
    }

    void NativeRenderer::SetStatus(JNIEnv* env, jstring javaStatus) {
        const char* status = env->GetStringUTFChars(javaStatus, nullptr);
        if (status == nullptr) {
            return;
        }
        this->state->pageManager.SetStatus(status);
        env->ReleaseStringUTFChars(javaStatus, status);
    }

    void NativeRenderer::SurfaceChanged(
        JNIEnv* env,
        jobject androidSurface,
        jint width,
        jint height) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SurfaceChanged: {}x{}", width, height);
        utility_helpers::logging::Initialize("MobileClock");
        State& state = *this->state;
        if (state.assetsManager == nullptr) {
            throw std::logic_error("AssetsManager must be set before creating a surface");
        }
        _details::DestroyRenderer(state);
        state.window = ANativeWindow_fromSurface(env, androidSurface);
        state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(state.display, nullptr, nullptr);

        const EGLint configurationAttributes[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_NONE,
        };
        EGLConfig configuration = nullptr;
        EGLint configurationCount = 0;
        eglChooseConfig(state.display, configurationAttributes, &configuration, 1, &configurationCount);
        const EGLint contextAttributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE,
        };
        state.context = eglCreateContext(
            state.display,
            configuration,
            EGL_NO_CONTEXT,
            contextAttributes);
        state.surface = eglCreateWindowSurface(
            state.display,
            configuration,
            state.window,
            nullptr);
        eglMakeCurrent(state.display, state.surface, state.surface, state.context);

        state.pageManager.Initialize({
            static_cast<float>(width),
            static_cast<float>(height),
        });
        const std::vector<unsigned char> regularFontData = state.assetsManager->ReadBytes("Roboto-Regular.ttf");
        const std::vector<unsigned char> boldFontData = state.assetsManager->ReadBytes("Roboto-Bold.ttf");
        const std::vector<unsigned char> blackFontData = state.assetsManager->ReadBytes("Roboto-Black.ttf");
        const std::vector<unsigned char> rippleVertexShader = state.assetsManager->ReadBytes("Shaders/Ripple.vert");
        const std::vector<unsigned char> rippleFragmentShader = state.assetsManager->ReadBytes("Shaders/Ripple.frag");
        state.renderer = std::make_unique<es_renderer::OpenGlRenderer>(
            width,
            height,
            regularFontData.data(),
            regularFontData.size(),
            boldFontData.data(),
            boldFontData.size(),
            blackFontData.data(),
            blackFontData.size(),
            CreateShaderPrograms(rippleVertexShader, rippleFragmentShader),
            [&assetsManager = *state.assetsManager](std::string_view source) {
                return assetsManager.ReadBytes(source);
            });
        _details::DrawPage(state);
    }

    void NativeRenderer::SurfaceDestroyed() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SurfaceDestroyed");
        _details::DestroyRenderer(*this->state);
    }

    void NativeRenderer::Touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::Touch: action={}, x={}, y={}", action, x, y);
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            this->state->pageManager.HandleTouchDown(x, y);
            return;
        }
        if (action == AMOTION_EVENT_ACTION_CANCEL) {
            this->state->pageManager.CancelTouch();
            return;
        }
        if (action != AMOTION_EVENT_ACTION_UP) {
            return;
        }
        if (!this->state->pageManager.HandleTouchUp(x, y)) {
            return;
        }
        _details::DrawPage(*this->state);
    }

    void NativeRenderer::Render() {
        if (this->state->display == EGL_NO_DISPLAY
            || this->state->surface == EGL_NO_SURFACE) {
            return;
        }
        _details::DrawPage(*this->state);
    }
}