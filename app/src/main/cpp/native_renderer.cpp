#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>

#include <stdint.h>

namespace {

EGLDisplay display = EGL_NO_DISPLAY;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;
ANativeWindow* window = nullptr;
GLuint program = 0;
GLuint vertexBuffer = 0;

constexpr char kVertexShader[] = R"(#version 300 es
layout (location = 0) in vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }
)";

constexpr char kFragmentShader[] = R"(#version 300 es
precision mediump float;
uniform vec4 textColor;
out vec4 color;
void main() { color = textColor; }
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
}

// A tiny 5x7 bitmap font: each byte is one row, and its lower five bits are pixels.
constexpr uint8_t kGlyphs[9][7] = {
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // H
    {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x1F}, // E
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, // L
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x0E, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0E}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, // D
    {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11}, // X
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, // +
};

int glyphIndex(char character) {
    switch (character) {
        case 'H': return 0; case 'E': return 1; case 'L': return 2;
        case 'O': return 3; case ' ': return 4; case 'C': return 5;
        case 'D': return 6; case 'X': return 7; case '+': return 8;
        default: return 4;
    }
}

void addQuad(float left, float top, float right, float bottom, float* output) {
    // OpenGL coordinates: -1..1 horizontally and vertically.
    const float quad[] = {left, top, right, top, right, bottom,
                          left, top, right, bottom, left, bottom};
    for (int i = 0; i < 12; ++i) output[i] = quad[i];
}

void drawText() {
    // "HELLO C++" using square pixels, centred in the surface.
    constexpr char text[] = "HELLO C++";
    constexpr float pixel = 0.025f;
    constexpr float glyphWidth = 6.0f * pixel;
    constexpr float textWidth = 9.0f * glyphWidth;
    float x = -textWidth / 2.0f;
    constexpr float y = 0.0875f;

    glUseProgram(program);
    glUniform4f(glGetUniformLocation(program, "textColor"), 1.0f, 0.92f, 0.23f, 1.0f);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    float vertices[12]{};
    for (char character : text) {
        const auto& glyph = kGlyphs[glyphIndex(character)];
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1 << (4 - column))) == 0) continue;
                const float left = x + column * pixel;
                const float top = y - row * pixel;
                addQuad(left, top, left + pixel, top - pixel, vertices);
                glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
        x += glyphWidth;
    }
}

void destroyRenderer() {
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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
    vertexBuffer = 0;
}

} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSurfaceChanged(
    JNIEnv* env, jobject, jobject androidSurface, jint width, jint height) {
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

    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    surface = eglCreateWindowSurface(display, config, window, nullptr);
    eglMakeCurrent(display, surface, surface, context);

    glViewport(0, 0, width, height);
    makeProgram();
    glClearColor(0.40f, 0.40f, 0.40f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawText();
    eglSwapBuffers(display, surface);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    destroyRenderer();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeTouch(JNIEnv*, jobject, jint, jfloat, jfloat) {
    // The first skeleton forwards touches; interactive controls will be added here.
}
