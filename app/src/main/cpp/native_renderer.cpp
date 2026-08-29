// Android NDK: доступ к нативному окну, которое Kotlin передаёт через JNI.
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>

#include <stdint.h>

namespace {

// Ресурсы EGL/OpenGL существуют только пока жива Android Surface.  Это первый
// минимальный рендерер с одним экраном, поэтому состояние пока хранится здесь.
EGLDisplay display = EGL_NO_DISPLAY;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;
ANativeWindow* window = nullptr;
GLuint program = 0;
GLuint vertexBuffer = 0;

// Вершинный шейдер получает уже готовую позицию вершины в диапазоне -1..1.
// Он не применяет матрицы: для первой версии интерфейса все координаты считаем
// непосредственно в пространстве экрана OpenGL.
constexpr char kVertexShader[] = R"(#version 300 es
layout (location = 0) in vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }
)";

// Фрагментный шейдер красит каждый пиксель фигуры единым цветом textColor.
// Позже сюда можно добавить текстуры, прозрачность, градиенты и эффекты.
constexpr char kFragmentShader[] = R"(#version 300 es
precision mediump float;
uniform vec4 textColor;
out vec4 color;
void main() { color = textColor; }
)";

GLuint compileShader(GLenum type, const char* source) {
    // OpenGL сначала компилирует два независимых шейдера, а затем соединяет их
    // в program. Для короткого демо ошибки компиляции пока не выводятся в log.
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

// Миниатюрный шрифт 5x7. Один элемент — строка символа; младшие пять битов
// определяют, какие квадратные пиксели нужно нарисовать. Он нужен только для
// демо, чтобы написать текст без подключения TTF-движка и текстурного атласа.
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
    // Таблица хранит не все ASCII-символы, а только буквы из текущей строки.
    // Неизвестный символ заменяем пробелом.
    switch (character) {
        case 'H': return 0; case 'E': return 1; case 'L': return 2;
        case 'O': return 3; case ' ': return 4; case 'C': return 5;
        case 'D': return 6; case 'X': return 7; case '+': return 8;
        default: return 4;
    }
}

void addQuad(float left, float top, float right, float bottom, float* output) {
    // В OpenGL видимая область — от -1 до 1 по обеим осям. Квадрат собираем
    // из двух треугольников: именно треугольники являются базовой примитивной
    // формой для glDrawArrays.
    const float quad[] = {left, top, right, top, right, bottom,
                          left, top, right, bottom, left, bottom};
    for (int i = 0; i < 12; ++i) output[i] = quad[i];
}

void drawText() {
    // Рисуем "HELLO C++" квадратными пикселями и центрируем по горизонтали.
    // Размер pixel задан в OpenGL-координатах, поэтому сейчас не зависит от dp.
    // Для настоящего UI здесь появятся layout в dp и масштабирование экрана.
    constexpr char text[] = "HELLO C++";
    constexpr float pixel = 0.025f;
    constexpr float glyphWidth = 6.0f * pixel;
    constexpr float textWidth = 9.0f * glyphWidth;
    float x = -textWidth / 2.0f;
    constexpr float y = 0.0875f;

    // Выбираем шейдерную программу и передаём ей цвет текста — тёплый жёлтый.
    glUseProgram(program);
    glUniform4f(glGetUniformLocation(program, "textColor"), 1.0f, 0.92f, 0.23f, 1.0f);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Временный буфер шести вершин (по две координаты) для одного пикселя.
    // В дальнейшем выгоднее собрать все символы в один VBO и вызвать draw
    // один раз, но для ясности демо рисует квадрат сразу.
    float vertices[12]{};
    for (char character : text) {
        const auto& glyph = kGlyphs[glyphIndex(character)];
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                // Проверяем соответствующий бит карты символа: 0 — прозрачный
                // фон, 1 — рисуем квадратный пиксель.
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
    // Surface может исчезнуть при сворачивании приложения, повороте экрана или
    // закрытии Activity. Нельзя использовать EGL/OpenGL после этого события.
    // Сначала отсоединяем context от потока, затем освобождаем ресурсы.
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

// Kotlin вызывает эту функцию из SurfaceHolder.surfaceChanged(). JNI передаёт
// Android Surface, а ANativeWindow_fromSurface делает из неё NDK-объект для EGL.
extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSurfaceChanged(
    JNIEnv* env, jobject, jobject androidSurface, jint width, jint height) {
    // При смене размера Surface пересоздаём всё состояние, чтобы не держать
    // ссылку на старое окно.
    destroyRenderer();
    window = ANativeWindow_fromSurface(env, androidSurface);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    // Запрашиваем оконную поверхность с 8 битами на каждый RGB-канал и
    // поддержкой OpenGL ES 3.0.
    const EGLint configAttributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint count;
    eglChooseConfig(display, configAttributes, &config, 1, &count);

    // EGL создаёт context OpenGL ES 3. Затем привязываем его к Surface, чтобы
    // все последующие gl* вызовы рисовали именно в окно приложения.
    const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    surface = eglCreateWindowSurface(display, config, window, nullptr);
    eglMakeCurrent(display, surface, surface, context);

    // Один кадр первого экрана: фон, надпись, затем показ через swap buffers.
    glViewport(0, 0, width, height);
    makeProgram();
    glClearColor(0.40f, 0.40f, 0.40f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawText();
    eglSwapBuffers(display, surface);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    // Kotlin сообщает, что Surface больше нельзя использовать.
    destroyRenderer();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mobileclock_MainActivity_nativeTouch(JNIEnv*, jobject, jint, jfloat, jfloat) {
    // Kotlin уже передаёт сюда action/x/y каждого касания. В следующем этапе
    // здесь будет hit-test UI-элементов (кнопок, циферблата и жестов).
}
