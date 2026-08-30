// Android NDK: доступ к Surface, EGL/OpenGL ES и файлам из папки assets.
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>

#include <cstdlib>

#include <Helpers/platform/Android/Logging.h>
#include "XamlRuntime/XamlLayout.h"

// stb_truetype создаёт обычную текстуру-атлас глифов; рисование делает OpenGL ES.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

namespace {
    constexpr int kFirstGlyph = 32; // ASCII-пробел.
    constexpr int kGlyphCount = 96; // Символы ASCII от 32 до 127.
    constexpr int kAtlasWidth = 512;
    constexpr int kAtlasHeight = 512;

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    ANativeWindow* window = nullptr;
    AAssetManager* assetManager = nullptr;
    GLuint program = 0;
    GLuint solidProgram = 0;
    GLuint vertexBuffer = 0;
    GLuint fontTexture = 0;
    stbtt_bakedchar glyphs[kGlyphCount]{};
    std::unique_ptr<mobileclock::ui::Element> mainPage;
    int renderWidth = 0;
    int renderHeight = 0;
    bool helloButtonIsBlue = false;

    // Вершина хранит позицию на экране и координату в текстурном атласе.
    constexpr char kVertexShader[] = R"(#version 300 es
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 textureCoordinate;
        out vec2 uv;
        void main() {
            uv = textureCoordinate;
            gl_Position = vec4(position, 0.0, 1.0);
        }
        )";

    // В красном канале текстуры находится непрозрачность символа.
    constexpr char kFragmentShader[] = R"(#version 300 es
        precision mediump float;
        in vec2 uv;
        uniform sampler2D fontAtlas;
        uniform vec4 textColor;
        out vec4 color;
        void main() {
            float alpha = texture(fontAtlas, uv).r;
            color = vec4(textColor.rgb, textColor.a * alpha);
        }
        )";

    constexpr char kSolidVertexShader[] = R"(#version 300 es
        layout (location = 0) in vec2 position;
        void main() { gl_Position = vec4(position, 0.0, 1.0); }
        )";

    constexpr char kSolidFragmentShader[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 color;
        out vec4 fragmentColor;
        void main() { fragmentColor = color; }
        )";

    GLuint compileShader(GLenum type, const char* source) {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }

    void makeProgram() {
        const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
        const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
        program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glGenBuffers(1, &vertexBuffer);

        const GLuint solidVertex = compileShader(GL_VERTEX_SHADER, kSolidVertexShader);
        const GLuint solidFragment = compileShader(GL_FRAGMENT_SHADER, kSolidFragmentShader);
        solidProgram = glCreateProgram();
        glAttachShader(solidProgram, solidVertex);
        glAttachShader(solidProgram, solidFragment);
        glLinkProgram(solidProgram);
        glDeleteShader(solidVertex);
        glDeleteShader(solidFragment);
    }

    bool makeFontAtlas() {
        if (assetManager == nullptr) {
        LOG_ERROR("AssetManager was not passed from Kotlin");
            return false;
        }

        // TTF упакован в APK из app/src/main/assets. NDK читает его без файлового пути.
        AAsset* fontAsset = AAssetManager_open(assetManager, "Roboto-Regular.ttf", AASSET_MODE_BUFFER);
        if (fontAsset == nullptr) {
        LOG_ERROR("Cannot open Roboto-Regular.ttf from assets");
            return false;
        }

        const auto fontSize = static_cast<size_t>(AAsset_getLength(fontAsset));
        auto* fontData = static_cast<unsigned char*>(std::malloc(fontSize));
        if (fontData == nullptr) {
            AAsset_close(fontAsset);
            return false;
        }
        const int bytesRead = AAsset_read(fontAsset, fontData, fontSize);
        AAsset_close(fontAsset);
        if (bytesRead != static_cast<int>(fontSize)) {
            std::free(fontData);
        LOG_ERROR("Cannot read Roboto-Regular.ttf");
            return false;
        }

        // Растеризуем ASCII в bitmap. Для Unicode и нескольких размеров атлас далее
        // должен пополняться динамически, но принцип отрисовки останется тем же.
        auto* atlas = static_cast<unsigned char*>(std::malloc(kAtlasWidth * kAtlasHeight));
        if (atlas == nullptr) {
            std::free(fontData);
            return false;
        }
        const int bakeResult = stbtt_BakeFontBitmap(
            fontData, 0, 64.0f, atlas, kAtlasWidth, kAtlasHeight,
            kFirstGlyph, kGlyphCount, glyphs);
        std::free(fontData);
        if (bakeResult <= 0) {
            std::free(atlas);
        LOG_ERROR("Font atlas is too small");
            return false;
        }

        glGenTextures(1, &fontTexture);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasWidth, kAtlasHeight, 0,
            GL_RED, GL_UNSIGNED_BYTE, atlas);
        std::free(atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    void appendVertex(float* vertices, int& offset, float pixelX, float pixelY,
        float textureX, float textureY, int width, int height) {
        // stb использует пиксели от левого верхнего угла, OpenGL — NDC снизу слева.
        vertices[offset++] = pixelX * 2.0f / width - 1.0f;
        vertices[offset++] = 1.0f - pixelY * 2.0f / height;
        vertices[offset++] = textureX;
        vertices[offset++] = textureY;
    }

    void appendQuad(float* vertices, int& offset, const stbtt_aligned_quad& quad,
        int width, int height) {
        appendVertex(vertices, offset, quad.x0, quad.y0, quad.s0, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y0, quad.s1, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y1, quad.s1, quad.t1, width, height);
        appendVertex(vertices, offset, quad.x0, quad.y0, quad.s0, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y1, quad.s1, quad.t1, width, height);
        appendVertex(vertices, offset, quad.x0, quad.y1, quad.s0, quad.t1, width, height);
    }

    void drawText(const char* text, int width, int height,
        mobileclock::ui::Color color) {
        // Сначала измеряем строку, чтобы центрировать её. Эта минимальная версия
        // поддерживает ASCII; для кириллицы и emoji нужен Unicode shaping-движок.
        float textWidth = 0.0f;
        for (const char* character = text; *character != '\0'; ++character) {
            const int index = static_cast<unsigned char>(*character) - kFirstGlyph;
            if (index >= 0 && index < kGlyphCount) textWidth += glyphs[index].xadvance;
        }

        float cursorX = (width - textWidth) * 0.5f;
        float cursorY = height * 0.5f + 22.0f; // Baseline текста.
        // До 256 ASCII-символов по 6 вершин × 4 числа; для демо этого достаточно.
        float vertices[256 * 6 * 4]{};
        int vertexDataSize = 0;
        for (const char* character = text; *character != '\0'; ++character) {
            const int index = static_cast<unsigned char>(*character) - kFirstGlyph;
            if (index < 0 || index >= kGlyphCount) continue;
            stbtt_aligned_quad quad{};
            stbtt_GetBakedQuad(glyphs, kAtlasWidth, kAtlasHeight, index,
                &cursorX, &cursorY, &quad, 1);
            appendQuad(vertices, vertexDataSize, quad, width, height);
        }

        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glUniform1i(glGetUniformLocation(program, "fontAtlas"), 0);
        glUniform4f(glGetUniformLocation(program, "textColor"),
            color.red, color.green, color.blue, color.alpha);

        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexDataSize * sizeof(float), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
            reinterpret_cast<void*>(2 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, vertexDataSize / 4);
    }

    void drawButtonOutline(const mobileclock::ui::Rect& bounds,
        mobileclock::ui::Color color) {
        const float left = bounds.x * 2.0f / renderWidth - 1.0f;
        const float right = (bounds.x + bounds.width) * 2.0f / renderWidth - 1.0f;
        const float top = 1.0f - bounds.y * 2.0f / renderHeight;
        const float bottom = 1.0f - (bounds.y + bounds.height) * 2.0f / renderHeight;
        const float vertices[] = {left, top, right, top, right, bottom, left, bottom};

        glUseProgram(solidProgram);
        glUniform4f(glGetUniformLocation(solidProgram, "color"),
            color.red, color.green, color.blue, color.alpha);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    void drawPage() {
        glClear(GL_COLOR_BUFFER_BIT);
        if (mainPage == nullptr || mainPage->children().empty() || fontTexture == 0) {
            return;
        }

        const auto& button = mainPage->children().front();
        drawButtonOutline(button->bounds(), button->foreground());
        drawText(button->text().c_str(), renderWidth, renderHeight, button->foreground());
        eglSwapBuffers(display, surface);
    }

    void destroyRenderer() {
        // Удаляем OpenGL-ресурсы, пока context ещё привязан к потоку.
        if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT) {
            eglMakeCurrent(display, surface, surface, context);
            if (fontTexture != 0) glDeleteTextures(1, &fontTexture);
            if (vertexBuffer != 0) glDeleteBuffers(1, &vertexBuffer);
            if (program != 0) glDeleteProgram(program);
            if (solidProgram != 0) glDeleteProgram(solidProgram);
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        if (display != EGL_NO_DISPLAY) {
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
            eglTerminate(display);
        }
        if (window != nullptr) ANativeWindow_release(window);
        display = EGL_NO_DISPLAY;
        surface = EGL_NO_SURFACE;
        context = EGL_NO_CONTEXT;
        window = nullptr;
        program = 0;
        solidProgram = 0;
        vertexBuffer = 0;
        fontTexture = 0;
    }

} // namespace

// Kotlin передаёт приватный путь files/logs/mobileclock.log до первого лога.
extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSetLogFile(
    JNIEnv* env, jobject, jstring javaLogFilePath) {
    const char* utf8Path = env->GetStringUTFChars(javaLogFilePath, nullptr);
    if (utf8Path == nullptr) return;
    utility_helpers::android::configureLogFile(utf8Path);
    env->ReleaseStringUTFChars(javaLogFilePath, utf8Path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeFlushLogs(JNIEnv*, jobject) {
    utility_helpers::android::flushLogging();
}

// Kotlin передаёт Android AssetManager один раз при старте Activity.
extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSetAssetManager(
    JNIEnv* env, jobject, jobject javaAssetManager) {
    utility_helpers::android::initializeLogging("MobileClock");
    assetManager = AAssetManager_fromJava(env, javaAssetManager);
    LOG_INFO("Android AssetManager connected");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSurfaceChanged(
    JNIEnv* env, jobject, jobject androidSurface, jint width, jint height) {
    utility_helpers::android::initializeLogging("MobileClock");
    LOG_INFO("Surface changed: {}x{}", width, height);
    destroyRenderer();
    window = ANativeWindow_fromSurface(env, androidSurface);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint count;
    eglChooseConfig(display, configAttributes, &config, 1, &count);

    const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    surface = eglCreateWindowSurface(display, config, window, nullptr);
    eglMakeCurrent(display, surface, surface, context);

    glViewport(0, 0, width, height);
    renderWidth = width;
    renderHeight = height;
    makeProgram();
    // Текстурный атлас хранит форму букв в alpha-канале. Без blending OpenGL
    // всё равно записывает RGB даже при alpha = 0 — это выглядело бы как
    // заполненные прямоугольники вокруг глифов.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.40f, 0.40f, 0.40f, 1.0f);
    mainPage = mobileclock::ui::createMainPage();
    mobileclock::ui::layout(*mainPage, {static_cast<float>(width), static_cast<float>(height)});
    makeFontAtlas();
    drawPage();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    LOG_INFO("Surface destroyed");
    destroyRenderer();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeTouch(JNIEnv*, jobject, jint action, jfloat x, jfloat y) {
    // MotionEvent.ACTION_UP: меняем состояние только после завершённого тапа.
    if (action != 1 || mainPage == nullptr || mainPage->children().empty()) {
        return;
    }

    auto& button = mainPage->children().front();
    const mobileclock::ui::Rect bounds = button->bounds();
    const bool tapped = x >= bounds.x && x <= bounds.x + bounds.width
        && y >= bounds.y && y <= bounds.y + bounds.height;
    if (!tapped) {
        return;
    }

    helloButtonIsBlue = !helloButtonIsBlue;
    button->setForeground(helloButtonIsBlue
        ? mobileclock::ui::Color{0.2f, 0.65f, 1.0f, 1.0f}
        : mobileclock::ui::Color{1.0f, 0.91f, 0.23f, 1.0f});
    drawPage();
}