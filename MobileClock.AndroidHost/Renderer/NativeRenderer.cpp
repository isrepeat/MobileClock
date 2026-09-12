#include <Helpers.Logging/Logging.h>
#include <ESRenderer/OpenGlRenderer.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/input.h>
#include <EGL/egl.h>

#include "MobileClock.Presentation/Registrations.h"
#include "Renderer/AndroidCommandDispatcher.h"
#include "UI/AppSessionController.h"
#include "NativeRenderer.h"
#include "AssetsManager.h"

#include <filesystem>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

namespace mobileclock::renderer {
    struct NativeRenderer::State {
        State()
            : appSessionController(storage) {
            this->appSessionController.SetHostEventHandler([this](
                mobileclock::ui::AppSessionSignal signal,
                const mobileclock::ui::AppSessionSignalData& data) {
                this->commandDispatcher.Dispatch(signal, data);
            });
        }

        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
        ANativeWindow* window = nullptr;
        std::unique_ptr<AssetsManager> assetsManager;
        AndroidCommandDispatcher commandDispatcher;
        mobileclock::ui::ApplicationStorage storage;
        mobileclock::ui::AppSessionController appSessionController;
        std::unique_ptr<es_renderer::OpenGlRenderer> renderer;
        bool isSessionInitialized = false;
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
    //    │              │  └─ AnimationController::Update() обновляет поля состояния эффекта хоста
    //    │              ├─ PageManager::Render()
    //    │              │  └─ MainPageViewModel::Render()
    //    │              │     └─ xaml::Render()
    //    │              │        └─ xaml::_details::RenderElement()
    //    │              │           └─ RendererRegistry::Render()
    //    │              │              └─ RenderWaveOutline()
    //    │              │                 ├─ context.RenderDefaultElement()
    //    │              │                 │  ├─ RenderChrome()
    //    │              │                 │  └─ DrawText()
    //    │              │                 ├─ mobileclock::resources::effects::RenderWave()
    //    │              │                 └─ DrawRoundedRectOutline() для дополнительной обводки
    //    │              └─ eglSwapBuffers() показывает завершённый кадр.
    //    └─ Choreographer.postFrameCallback() планирует следующий VSync.
    void DrawPage(NativeRenderer::State& state) {
        if (state.renderer == nullptr) {
            return;
        }
        state.renderer->BeginFrame();
        state.appSessionController.Session().Update();
        state.appSessionController.Session().Render(*state.renderer);
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

}

namespace mobileclock::renderer {
    NativeRenderer::NativeRenderer()
        : state(std::make_unique<State>()) {
    }

    NativeRenderer::~NativeRenderer() {
        this->SurfaceDestroyed();
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
        this->state->commandDispatcher.SetDispatcher(env, javaDispatcher);
    }

    void NativeRenderer::DispatchSessionSignal(
        JNIEnv* env,
        jint javaSignal,
        jstring javaValue,
        jstring javaAdditionalValue) {
        const char* value = env->GetStringUTFChars(javaValue, nullptr);
        if (value == nullptr) {
            return;
        }
        const char* additionalValue = env->GetStringUTFChars(javaAdditionalValue, nullptr);
        if (additionalValue == nullptr) {
            env->ReleaseStringUTFChars(javaValue, value);
            return;
        }
        this->state->appSessionController.Dispatch(
            static_cast<mobileclock::ui::AppSessionSignal>(javaSignal),
            {value, additionalValue});
        env->ReleaseStringUTFChars(javaAdditionalValue, additionalValue);
        env->ReleaseStringUTFChars(javaValue, value);
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

        const xaml::Size availableSize{
            static_cast<float>(width),
            static_cast<float>(height),
        };
        if (state.isSessionInitialized) {
            state.appSessionController.Session().Resize(availableSize);
        } else {
            state.appSessionController.Session().Initialize(availableSize);
            state.isSessionInitialized = true;
        }
        const std::vector<unsigned char> regularFontData = state.assetsManager->ReadBytes("Roboto-Regular.ttf");
        const std::vector<unsigned char> boldFontData = state.assetsManager->ReadBytes("Roboto-Bold.ttf");
        const std::vector<unsigned char> blackFontData = state.assetsManager->ReadBytes("Roboto-Black.ttf");
        state.renderer = std::make_unique<es_renderer::OpenGlRenderer>(
            width,
            height,
            regularFontData.data(),
            regularFontData.size(),
            boldFontData.data(),
            boldFontData.size(),
            blackFontData.data(),
            blackFontData.size(),
            mobileclock::presentation::CreateShaderPrograms(),
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
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            LOG_DEBUG("MobileClock.Touch", "Touch down received: point=({}, {})", x, y);
            this->state->appSessionController.Session().PointerDown(x, y);
            return;
        }
        if (action == AMOTION_EVENT_ACTION_CANCEL) {
            LOG_DEBUG("MobileClock.Touch", "Touch cancelled");
            this->state->appSessionController.Session().CancelPointer();
            return;
        }
        if (action == AMOTION_EVENT_ACTION_MOVE) {
            this->state->appSessionController.Session().PointerMove(x, y);
            return;
        }
        if (action != AMOTION_EVENT_ACTION_UP) {
            return;
        }
        LOG_DEBUG("MobileClock.Touch", "Touch up received: point=({}, {})", x, y);
        this->state->appSessionController.Session().PointerUp(x, y);
    }

    void NativeRenderer::Render() {
        if (this->state->display == EGL_NO_DISPLAY
            || this->state->surface == EGL_NO_SURFACE) {
            return;
        }
        _details::DrawPage(*this->state);
    }
}