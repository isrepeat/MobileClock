# Отрисовка и анимации XAML

Читайте руководство последовательно: сначала стандартное оформление без кода,
затем статические renderer, собственный шейдер и только после этого анимации.
По мере усложнения примеров появляются регистрация, структуры состояния и C++.
В конце находятся справочник XAML и подробности runtime.

| Задача | Достаточный уровень |
|---|---|
| Изменить цвет, рамку или скругление | Свойства XAML |
| Собрать оформление из элементов | Контейнеры и дочерние элементы XAML |
| Добавить графику поверх стандартной кнопки | Renderer с RenderDefault() |
| Полностью заменить изображение | Renderer, рисующий нужные части самостоятельно |
| Хранить собственные параметры оформления | Типизированное состояние renderer |
| Вычислять изображение на GPU | Собственный шейдер и вызывающий его renderer |
| Изменять изображение со временем | Анимация свойств или полей состояния |

Примеры XAML — фрагменты внутри страницы с `xmlns="urn:mobileclock:xaml"`.
Новые renderer из примеров нужно реализовать и зарегистрировать; одно имя
в XAML не создаёт обработчик. Сборки для чтения этого руководства не нужны.

## Отрисовка без анимации

Renderer не обязан иметь анимацию. Он может каждый раз рисовать одно и то же
изображение из свойств элемента и постоянных настроек. Для таких примеров
не нужны AnimationRegistry, AnimationController, Storyboard или вызов Update().

### Уровень 1. Стандартное оформление: только XAML

```xml
<Button text="Сохранить"
        command="save"
        background="#303128"
        foreground="#FFFFFF"
        borderBrush="#E6CA69"
        borderThickness="1"
        cornerRadius="12"
        padding="20 12"
        fontSize="18"/>
```

Этого достаточно, чтобы стандартный renderer нарисовал фон, рамку и текст.
`command` нужен для обработки нажатия, а не для рисования.
Без назначенной анимации внешний вид при нажатии сам по себе не меняется.

| Свойство | Назначение |
|---|---|
| background | Цвет фона |
| foreground | Цвет текста |
| borderBrush / borderThickness | Цвет и толщина рамки |
| cornerRadius | Радиус скругления |
| padding | Внутренние отступы, учитываемые layout |
| fontSize / fontWeight | Размер и начертание текста |
| opacity | Общая прозрачность элемента и содержимого |
| width / height / margin | Размеры и внешние отступы layout |

Шейдеры фона и текста уже встроены в OpenGlRenderer. Не нужно писать GLSL,
загружать файлы или указывать renderer. В обычной кнопке волна не рисуется,
пока её waveOpacity равно нулю.

В существующем приложении достаточно поменять XAML. Если дерево создаётся
в собственном хосте, стандартный проход выглядит так:

```cpp
// root — созданное дерево Element; backend — готовый IRenderBackend.
xaml::layout(root, availableSize);
xaml::Render(root, backend);
```

layout вычисляет размеры и координаты. Render обходит дерево и передаёт
графические команды backend. Эти вызовы не создают окно, OpenGL-контекст
или backend: их подготавливает хост.

### Уровень 2. Составное оформление без C++

Если оформление можно собрать из существующих элементов, свой renderer ещё
не нужен. Например, внешний контейнер образует цветную рамку вокруг кнопки:

```xml
<Border background="#E6CA69"
        cornerRadius="18"
        padding="3">
    <Button text="Сохранить"
            command="save"
            background="#303128"
            foreground="#FFFFFF"
            cornerRadius="15"
            padding="20 12"/>
</Border>
```

Border рисует свой фон, а Button — свой фон и текст. Каждый элемент проходит
через стандартную отрисовку. Это именно два элемента: внутреннюю кнопку
обрабатывает input, а внешний Border сам по себе команду не выполняет.

Так же можно составлять содержимое из StackPanel, TextBlock и других элементов.
Расположение задаёт layout, а не renderer. Пользовательский C++ не требуется.

### Уровень 3. Добавить собственную графику поверх кнопки

Теперь нужен renderer, который сохраняет стандартное изображение и добавляет
вторую декоративную обводку. Собственного изменяемого состояния нет, поэтому
используем встроенный тип `xaml::EmptyState`.

XAML:

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererAccent"
        background="#303128"
        foreground="#E6CA69"
        cornerRadius="12"/>
```

В C++-файле renderer подключаем заголовок и объявляем обычную функцию:

```cpp
#include <XamlRuntime/RenderEngine.h>

namespace mobileclock::renderer::_details {
    bool RenderAccent(
        const xaml::Element& element,
        xaml::RenderContext<xaml::EmptyState>& context) {
        context.RenderDefault();

        auto color = element.Foreground();
        color.alpha *= context.Opacity();

        context.Backend().DrawRoundedRectOutline(
            context.Bounds(),
            color,
            element.CornerRadius(),
            2.0f);
        return true;
    }
}
```

При инициализации добавляем функцию в тот реестр, который используется при рисовании:

```cpp
xaml::RendererRegistry renderers;
renderers.Register<xaml::EmptyState>(
    "rendererAccent",
    mobileclock::renderer::_details::RenderAccent);

// При отрисовке:
xaml::Render(root, backend, renderers);
```

В MobileClock нужно дополнить существующий RegisterAnimationRenderers —
несмотря на его название, он может регистрировать и статические renderer.
Создание отдельного реестра не поможет, если он не передан в Render.

Что происходит:

1. Render находит `rendererAccent` по свойству renderer.
2. Подготавливает EmptyState и создаёт контекст текущего элемента.
3. RenderAccent вызывает RenderDefault: фон, текст и содержимое рисуются штатно.
4. DrawRoundedRectOutline добавляет обводку поверх результата.
5. Возвращённое true означает, что обработчик выполнил отрисовку.

`EmptyState` уже зарегистрирован движком; повторная регистрация типа не нужна.
В обработчике нет таймера и нет анимации.

### Уровень 4. Полностью заменить отрисовку кнопки

Чтобы самостоятельно определить изображение, не вызываем RenderDefault.
Тогда ответственность за фон, текст и содержимое переходит к обработчику.

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererFlat"
        background="#164A60"
        foreground="#FFFFFF"
        cornerRadius="8"
        fontSize="18"/>
```

Полный пример функции:

```cpp
namespace mobileclock::renderer::_details {
    bool RenderFlat(
        const xaml::Element& element,
        xaml::RenderContext<xaml::EmptyState>& context) {
        if (element.Type() != xaml::ElementType::button) {
            return false;
        }

        auto background = element.Background();
        background.alpha *= context.Opacity();

        context.Backend().DrawRoundedRect(
            context.Bounds(),
            background,
            element.CornerRadius());

        auto textColor = element.Foreground();
        textColor.alpha *= context.Opacity();

        context.Backend().DrawText(
            context.Bounds(),
            element.Text(),
            textColor,
            element.FontSize(),
            element.FontWeight(),
            xaml::attr::Alignment::center);

        context.RenderChildren();
        return true;
    }
}
```

Регистрация:

```cpp
renderers.Register<xaml::EmptyState>(
    "rendererFlat",
    mobileclock::renderer::_details::RenderFlat);
```

В этом примере текст центрирован во всей области кнопки. Если нужны особые
отступы текста, обработчик должен вычислить отдельный прямоугольник для DrawText.
Стандартная рамка и стандартная волна не рисуются: RenderDefault не вызывался.

`RenderChildren()` рисует вложенные элементы, но не заменяет DrawText для
собственного свойства text кнопки. Дочерние элементы сохраняют свои renderer.

Важные правила для собственных обработчиков:

| API | Смысл |
|---|---|
| context.Bounds() | Область рисования с уже применённым смещением элемента и родителей |
| context.Opacity() | Общая прозрачность, которую надо учитывать в собственных цветах или шейдере |
| context.RenderDefault() | Стандартное изображение вместе с дочерними элементами |
| context.RenderChildren() | Только дочерние элементы |
| return true | Собственная отрисовка обработана |
| return false | Движок должен выполнить стандартную отрисовку |

Возвращайте false до рисования, иначе можно получить собственные команды вместе
со стандартным изображением. Неизвестное имя renderer тоже использует стандартную
отрисовку. Повторный вызов RenderDefault или RenderChildren через один контекст
не должен дублировать соответствующее содержимое.

Кастомный renderer не меняет автоматически размеры, layout или hit-test.
Если нарисовать круглую кнопку, область ввода сама по себе круглой не станет.

### Уровень 5. Статический renderer со своими настройками в C++

Собственная структура нужна не только для анимации. В ней можно хранить
постоянные параметры оформления конкретного элемента.

В заголовке, общем для кода регистрации и настройки элемента:

```cpp
namespace mobileclock::renderer {
    struct AccentStyle {
        float thickness = 3.0f;
        float intensity = 0.6f;
    };
}
```

Обработчик в C++-файле:

```cpp
namespace mobileclock::renderer::_details {
    bool RenderStyledAccent(
        const xaml::Element& element,
        xaml::RenderContext<AccentStyle>& context) {
        context.RenderDefault();

        const auto& style = context.State();
        auto color = element.Foreground();
        color.alpha *= style.intensity * context.Opacity();

        context.Backend().DrawRoundedRectOutline(
            context.Bounds(),
            color,
            element.CornerRadius(),
            style.thickness);
        return true;
    }
}
```

Объявление состояния и регистрация renderer:

```cpp
xaml::StateRegistry states;
states.Register<mobileclock::renderer::AccentStyle>();

xaml::RendererRegistry renderers(states);
renderers.Register<mobileclock::renderer::AccentStyle>(
    "rendererStyledAccent",
    mobileclock::renderer::_details::RenderStyledAccent);
```

XAML выбирает renderer:

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererStyledAccent"
        foreground="#E6CA69"/>
```

По умолчанию используются значения из AccentStyle. При необходимости код хоста
может настроить отдельный элемент до рисования:

```cpp
// button — Element с renderer="rendererStyledAccent".
renderers.Prepare(button);
auto& style = button.States().Get<mobileclock::renderer::AccentStyle>();
style.thickness = 4.0f;
style.intensity = 0.9f;
```

У другой кнопки будет свой экземпляр AccentStyle. Изменение не затронет её.
Render автоматически подготавливает отсутствующие состояния, но перед ручным
Get нужен явный Prepare. Хост должен запросить перерисовку после изменения,
если его цикл рисования работает только по событиям.

Важно: произвольные атрибуты вроде `accentIntensity="0.9"` или блок
`Button.Renderer.Parameters` сейчас не поддерживаются.
Схема Option относится к настройкам зарегистрированной Animation, а не к renderer.
Для статического renderer используйте обычные свойства Element или настройку
типизированного состояния из C++, как показано выше.

### Уровень 6. Полностью собственный статический шейдер

Если примитивов backend недостаточно, renderer может вызвать свою GLSL-программу.
Ни собственная анимация, ни структура состояния для этого не обязательны.

Ниже шейдер рисует скруглённый фон с постоянным вертикальным градиентом.
Сначала задаём исходники, например в AnimationShaders.cpp, в именованном namespace:

```cpp
namespace mobileclock::renderer::_details {
    constexpr char GradientVertex[] = R"(#version 300 es
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 localPosition;
        out vec2 local;
        void main() {
            local = localPosition;
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    constexpr char GradientFragment[] = R"(#version 300 es
        precision mediump float;
        in vec2 local;
        uniform vec2 size;
        uniform float cornerRadius;
        uniform vec4 baseColor;
        uniform float opacity;
        out vec4 color;
        void main() {
            vec2 halfSize = size * 0.5;
            float radius = clamp(cornerRadius, 0.0, min(halfSize.x, halfSize.y));
            vec2 q = abs(local * size - halfSize) - halfSize + radius;
            float distanceToEdge =
                length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
            if (distanceToEdge > 0.0) {
                discard;
            }
            float brightness = mix(1.0, 0.55, local.y);
            color = vec4(baseColor.rgb * brightness, baseColor.a * opacity);
        }
    )";
}
```

В существующей CreateShaderPrograms заменяем пустой результат регистрацией
своей программы. Шейдеры имеют статическое время жизни, поэтому string_view
в ShaderProgramSource не ссылаются на уничтоженные локальные строки:

```cpp
namespace mobileclock::renderer {
    es_renderer::OpenGlRenderer::ShaderProgramSources CreateShaderPrograms() {
        return {
            {"shaderGradient", {
                _details::GradientVertex,
                _details::GradientFragment,
            }},
        };
    }
}
```

MobileClock уже передаёт результат этой функции в конструктор OpenGlRenderer.
Встроенные программы при этом сохраняются. Ключ shaderGradient принадлежит
приложению; стандартный renderer движка о нём ничего не знает.

Теперь обработчик, который вызывает программу и рисует текст:

```cpp
namespace mobileclock::renderer::_details {
    bool RenderGradient(
        const xaml::Element& element,
        xaml::RenderContext<xaml::EmptyState>& context) {
        if (element.Type() != xaml::ElementType::button) {
            return false;
        }

        const auto background = element.Background();
        context.Backend().DrawShader(
            "shaderGradient",
            context.Bounds(),
            {
                {"cornerRadius", {element.CornerRadius()}, 1},
                {"baseColor", {
                    background.red,
                    background.green,
                    background.blue,
                    background.alpha,
                }, 4},
                {"opacity", {context.Opacity()}, 1},
            });

        auto foreground = element.Foreground();
        foreground.alpha *= context.Opacity();
        context.Backend().DrawText(
            context.Bounds(),
            element.Text(),
            foreground,
            element.FontSize(),
            element.FontWeight(),
            xaml::attr::Alignment::center);

        context.RenderChildren();
        return true;
    }
}
```

Регистрация renderer и XAML:

```cpp
renderers.Register<xaml::EmptyState>(
    "rendererGradient",
    mobileclock::renderer::_details::RenderGradient);
```

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererGradient"
        background="#26708A"
        foreground="#FFFFFF"
        cornerRadius="16"
        width="240"
        height="64"/>
```

Для GLSL используются следующие данные:

| Данные | Кто задаёт |
|---|---|
| position, location 0 | Backend: координаты вершин для OpenGL |
| localPosition, location 1 | Backend: локальные координаты 0..1 |
| size | Backend: размеры области DrawShader |
| cornerRadius, baseColor, opacity | Наш renderer через список uniforms |

Число после массива значений в ShaderUniform — количество компонентов:
1 для float, 2 для vec2, 3 для vec3, 4 для vec4.
В примере общая прозрачность применяется в шейдере ровно один раз.

Два независимых ключа связывают разные уровни:

```text
XAML renderer="rendererGradient"
    → RendererRegistry вызывает RenderGradient
    → RenderGradient вызывает DrawShader("shaderGradient", ...)
    → OpenGlRenderer выполняет зарегистрированную GLSL-программу
```

Нет времени, прогресса и Update(): градиент статический. Программа компилируется
при создании backend, а её вызов происходит при рисовании.
Чтобы тот же градиент менялся со временем, нужно добавить изменяемое поле
состояния и анимацию этого поля — следующие разделы показывают такой переход.

В приложении и предпросмотре разные экземпляры backend и реестров. Регистрация
shaderGradient только в MobileClock не добавляет его в XamlPreviewer: для
предпросмотра своего эффекта нужно подключить и программу, и renderer к его хосту.

## Добавляем анимации: от готовой волны до собственного эффекта

Здесь последовательно показаны варианты от простого к сложному.
Встроенные возможности уже работают; новые типы и функции из примеров Dim и
Highlight нужно добавить в приложение и зарегистрировать. Примеры регистрации
независимы: в реальном приложении регистрации собираются в общие реестры до Attach.

| Уровень | За что отвечает |
|---|---|
| Анимация | Меняет свойства элемента или поля структуры со временем |
| Renderer | Определяет, что рисовать, используя текущие значения |
| Шейдер | Вычисляет изображение на GPU по команде renderer |

### Шаг 1. Стандартная кнопка со встроенной анимацией

```xml
<Button text="Сохранить"
        command="save"
        background="#303128"
        foreground="#FFFFFF">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationSoftPulse"
                       duration="650"/>
            <FloatAnimation property="waveOpacity"
                            from="0"
                            to="1"
                            duration="0"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <FloatAnimation property="waveOpacity"
                            from="Current"
                            to="0"
                            duration="200"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

При PointerDown обработчик animationSoftPulse запускает изменение waveProgress,
а FloatAnimation сразу делает волну видимой. Стандартный renderer использует
прогресс при рисовании встроенным шейдером. При отпускании waveOpacity уменьшается
до нуля. Кастомный renderer здесь не нужен.

Настройки встроенной волны можно менять непосредственно в XAML:

```xml
<Animation name="animationSoftPulse"
           duration="400"
           intensity="0.7"
           spread="0.4"
           fadeExponent="2"
           easing="Linear"/>
```

Чтобы явно подавить назначенную штатную анимацию событий:

```xml
<Button.Storyboards>
    <Storyboard trigger="PointerDown"/>
    <Storyboard trigger="PointerUp"/>
</Button.Storyboards>
```

### Шаг 2. Изменить только шейдер встроенной волны

XAML предыдущего шага остаётся прежним. Стандартный renderer вызывает
`xaml::BuiltinShaders::buttonWave` — публичный ключ только программы волны,
а не всей кнопки. Фон и текст используют другие встроенные программы.

OpenGlRenderer уже содержит исходники волны. Чтобы заменить её изображение,
приложение передаёт свои GLSL-исходники под тем же публичным ключом:

```cpp
es_renderer::OpenGlRenderer::ShaderProgramSources programs;
programs.emplace(
    xaml::BuiltinShaders::buttonWave,
    es_renderer::OpenGlRenderer::ShaderProgramSource{
        vertexSource,
        fragmentSource,
    });
```

vertexSource и fragmentSource — строки GLSL, которые должны оставаться живыми
до завершения конструктора OpenGlRenderer, принимающего programs.
В MobileClock точка расширения — CreateShaderPrograms() в AnimationShaders.cpp.
Сейчас она возвращает пустой набор: backend использует встроенную волну.

Подмена сохраняет интерфейс: vertex attributes position (location 0) и
localPosition (location 1), uniforms size, cornerRadius, progress, spread,
rippleColor. Например, fragment shader может вместо мягкого пятна рисовать
кольцо, радиус которого зависит от progress.

```text
PointerDown
    → animationSoftPulse обновляет прогресс
    → стандартный renderer вызывает BuiltinShaders::buttonWave
    → выполняется переданная приложением GLSL-программа
```

Логика и длительность анимации не меняются. Подмена затрагивает все стандартные
волны этого экземпляра backend. Атрибута shader для отдельной кнопки пока нет:
для отдельной программы нужен собственный renderer, как в шаге 6.

### Шаг 3. Своя анимация со стандартным renderer

Например, при нажатии хотим уменьшать прозрачность всей кнопки.
Объявляем настройки и обработчик:

```cpp
struct Dim {
    float targetOpacity = 0.5f;
    int duration = 150;
};

bool AnimateDim(xaml::AnimationContext<Dim>& context) {
    const auto& settings = context.State();
    context.AnimateProperty(
        xaml::AnimatedProperty::opacity,
        context.Target().Opacity(),
        settings.targetOpacity,
        std::chrono::milliseconds(settings.duration));
    return true;
}
```

Здесь изменяется существующее свойство Element::Opacity. Стандартный renderer
уже умеет его учитывать. Регистрируем тип и анимацию:

```cpp
xaml::StateRegistry states;
states.Register<Dim>();

xaml::AnimationRegistry animations(states);
animations.Register<Dim>("animationDim", {
    xaml::Option("opacity", &Dim::targetOpacity),
    xaml::Option("duration", &Dim::duration),
}, AnimateDim);
```

```xml
<Button text="Сохранить"
        command="save">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationDim"
                       opacity="0.5"
                       duration="150"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <Animation name="animationDim"
                       opacity="1"
                       duration="250"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

Для такого простого случая достаточно и FloatAnimation в XAML.
Свой обработчик полезен, когда нужны вычисления, условия или несколько
согласованных треков. К схеме регистрации можно добавить валидаторы значений,
как показано в разделе про типизированное состояние.

### Шаг 4. Кастомная отрисовка поверх готовой анимации

В приложении уже зарегистрирован rendererWaveOutline:

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererWaveOutline">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationSoftPulse"/>
            <FloatAnimation property="waveOpacity"
                            from="0"
                            to="1"
                            duration="0"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <FloatAnimation property="waveOpacity"
                            from="Current"
                            to="0"
                            duration="200"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

Этот renderer сначала вызывает стандартную отрисовку, затем читает прогресс
волны и рисует дополнительную затухающую обводку. Без атрибута renderer останется
обычная кнопка с волной. Анимация в обоих случаях та же самая.

### Шаг 5. Своя анимация и свой renderer с общим состоянием

Создадим новую пару animationHighlight / rendererHighlight.
Она устроена аналогично уже встроенным animationGlow / rendererGlow.

Одна структура содержит текущее значение и настройки запуска:

```cpp
struct Highlight {
    float intensity = 0.0f;
    float targetIntensity = 0.8f;
    int duration = 300;
};
```

Анимация меняет поле этой структуры:

```cpp
bool AnimateHighlight(xaml::AnimationContext<Highlight>& context) {
    const auto& state = context.State();
    context.Animate(
        &Highlight::intensity,
        state.targetIntensity,
        std::chrono::milliseconds(state.duration));
    return true;
}
```

Renderer читает это же поле и добавляет обводку к стандартной кнопке:

```cpp
bool RenderHighlight(
    const xaml::Element& element,
    xaml::RenderContext<Highlight>& context) {
    context.RenderDefault();

    auto color = element.Foreground();
    color.alpha *= context.State().intensity * context.Opacity();

    context.Backend().DrawRoundedRectOutline(
        context.Bounds(), color, element.CornerRadius(), 3.0f);
    return true;
}
```

Регистрация использует обычные функции без захватов:

```cpp
xaml::StateRegistry states;
states.Register<Highlight>();

xaml::AnimationRegistry animations(states);
animations.Register<Highlight>("animationHighlight", {
    xaml::Option("intensity", &Highlight::targetIntensity),
    xaml::Option("duration", &Highlight::duration),
}, AnimateHighlight);

xaml::RendererRegistry renderers(states);
renderers.Register<Highlight>("rendererHighlight", RenderHighlight);
```

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererHighlight">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationHighlight"
                       intensity="0.8"
                       duration="300"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <Animation name="animationHighlight"
                       intensity="0"
                       duration="150"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

Обработчики связаны типом Highlight, а не совпадением строковых имён.
Для каждой кнопки создаётся отдельный экземпляр состояния.

```text
PointerDown
    → XAML-настройки записываются в targetIntensity и duration
    → AnimateHighlight создаёт трек поля intensity
    → контроллер обновляет intensity
    → RenderHighlight читает intensity и рисует обводку
```

При отпускании новый трек продолжает с текущего значения до нуля.
Встроенная пара animationGlow / rendererGlow уже работает таким способом,
поэтому для неё дополнительные регистрации не нужны.

### Шаг 6. Полностью заменить отрисовку, включая шейдер

В предыдущем renderer RenderDefault() сохранял стандартное изображение кнопки.
Для полной замены не вызываем этот метод и рисуем нужные части самостоятельно:

```cpp
bool RenderShaderHighlight(
    const xaml::Element& element,
    xaml::RenderContext<Highlight>& context) {
    context.Backend().DrawShader(
        "shaderHighlight",
        context.Bounds(),
        {
            {"intensity", {context.State().intensity}, 1},
            {"opacity", {context.Opacity()}, 1},
            {"cornerRadius", {element.CornerRadius()}, 1},
        });

    auto textColor = element.Foreground();
    textColor.alpha *= context.Opacity();

    context.Backend().DrawText(
        context.Bounds(),
        element.Text(),
        textColor,
        element.FontSize(),
        element.FontWeight(),
        xaml::attr::Alignment::center);

    context.RenderChildren();
    return true;
}
```

Шейдер рисует собственный фон, текст кнопки рисуется явно,
а RenderChildren() отображает вложенные элементы при их наличии.
Renderer регистрируется для того же состояния Highlight:

```cpp
renderers.Register<Highlight>(
    "rendererShaderHighlight",
    RenderShaderHighlight);
```

Отдельную GLSL-программу нужно написать и добавить в набор программ backend:

```cpp
programs.emplace(
    "shaderHighlight",
    es_renderer::OpenGlRenderer::ShaderProgramSource{
        customVertexSource,
        customFragmentSource,
    });
```

shaderHighlight — имя из этого примера, такой программы в проекте пока нет.
Она должна соответствовать vertex interface DrawShader и использовать
переданные uniforms. Приложение определяет её fragment shader самостоятельно.
Добавление своего ключа не отключает встроенную волну.

В XAML меняется только renderer, а анимация остаётся прежней:

```xml
<Button text="Сохранить"
        command="save"
        renderer="rendererShaderHighlight">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationHighlight"
                       intensity="0.8"
                       duration="300"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <Animation name="animationHighlight"
                       intensity="0"
                       duration="150"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

### Подключение примеров к приложению

Реестры должны использоваться тем деревом и проходом рисования, где находится
кнопка. После регистрации:

```cpp
controller.Attach(root, animations);

// При событии ввода:
controller.Start(button, xaml::AnimationTrigger::pointerDown);

// При обновлении и отрисовке:
controller.Update();
xaml::Render(root, backend, renderers);
```

В MobileClock эти места уже существуют. Нужно дополнить реестры приложения,
а не создавать независимый реестр, который никто не передаёт контроллеру.
Программы шейдеров отдельно передаются при создании OpenGlRenderer.

| Что меняем | Что нужно написать |
|---|---|
| Длительность готовой волны | Настройки Animation в XAML |
| Простое изменение свойства | FloatAnimation в XAML |
| Только изображение общей волны | Подмену программы BuiltinShaders::buttonWave |
| Логику изменения стандартного свойства | Свой обработчик анимации |
| Дополнительную графику поверх кнопки | Renderer с RenderDefault() |
| Собственное визуальное поведение | Общую структуру, анимацию и renderer |
| Полностью своё изображение кнопки | Renderer без RenderDefault(), при необходимости свой шейдер |

## Справочник XAML: анимации

Внутри `Storyboard` есть два вида записей:

- `Animation` — вызвать анимацию по зарегистрированному имени.
- `FloatAnimation` — напрямую описать изменение числового свойства.

Ниже — минимальные примеры текущих возможностей. Это фрагменты внутри страницы с `xmlns="urn:mobileclock:xaml"`. Длительности задаются в миллисекундах.

### 1. Появление и исчезновение

Самая короткая запись:

```xml
<Border>
    <Border.Storyboards>
        <Storyboard trigger="Show">
            <Animation name="animationFade"/>
        </Storyboard>
        <Storyboard trigger="Hide">
            <Animation name="animationFade"/>
        </Storyboard>
    </Border.Storyboards>
</Border>
```

`animationFade` меняет прозрачность. По умолчанию длительность — `180` мс.

Для `Page`, `StackPanel`, `Button` и других элементов форма такая же: меняется только имя перед `.Storyboards`.

### 2. Разные анимации и параметры для Show и Hide

```xml
<Border>
    <Border.Storyboards>
        <Storyboard trigger="Show">
            <Animation name="animationSlideFade"
                       duration="320"
                       distance="24"/>
        </Storyboard>
        <Storyboard trigger="Hide">
            <Animation name="animationFade"
                       duration="120"/>
        </Storyboard>
    </Border.Storyboards>
</Border>
```

`animationSlideFade` сочетает прозрачность с вертикальным смещением. Его настройки по умолчанию: `duration="180"`, `distance="24"`.

Отрицательный `distance` задаёт смещение вверх. У встроенных `animationFade` и `animationSlideFade` сейчас фиксированное сглаживание `CubicOut`.

### 3. Анимация конкретного свойства

Например, горизонтальное появление без изменения прозрачности:

```xml
<Border>
    <Border.Storyboards>
        <Storyboard trigger="Show">
            <FloatAnimation property="renderOffsetX"
                            from="-40"
                            to="0"
                            duration="250"
                            easing="CubicOut"/>
        </Storyboard>
    </Border.Storyboards>
</Border>
```

У `FloatAnimation` обязательны `property`, `from`, `to`, `duration`.

Полный список поддерживаемых свойств:

| `property` | Что меняется |
|---|---|
| `opacity` | Прозрачность элемента |
| `renderOffsetX` | Горизонтальное смещение отрисовки |
| `pressProgress` | Прогресс визуального состояния нажатия |
| `toggleProgress` | Прогресс переключения |
| `waveProgress` | Прогресс волны |
| `waveOpacity` | Прозрачность волны |

Свойства состояний дают видимый результат, если renderer элемента их использует.

### 4. Нажатие и отпускание — от текущего значения

```xml
<Button text="Нажми"
        command="save">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <FloatAnimation property="pressProgress"
                            from="Current"
                            to="1"
                            duration="100"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <FloatAnimation property="pressProgress"
                            from="Current"
                            to="0"
                            duration="200"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

`from="Current"` начинает с текущего значения свойства. Это позволяет продолжить движение без скачка при смене события.

### 5. Переключение в зависимости от состояния

```xml
<ToggleSwitch command="toggle">
    <ToggleSwitch.Storyboards>
        <Storyboard trigger="Toggled">
            <FloatAnimation property="toggleProgress"
                            from="Current"
                            to="ToggleState"
                            duration="180"/>
        </Storyboard>
    </ToggleSwitch.Storyboards>
</ToggleSwitch>
```

`to="ToggleState"` означает `1` для включённого состояния и `0` для выключенного.

Все доступные события:

| `trigger` | Событие |
|---|---|
| `Show` | Появление |
| `Hide` | Исчезновение |
| `PointerDown` | Нажатие |
| `PointerUp` | Отпускание |
| `Toggled` | Переключение состояния |

### 6. Существующая волна и смешивание видов анимации

```xml
<Button text="Нажми"
        command="save">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationSoftPulse"
                       duration="650"/>
            <FloatAnimation property="waveOpacity"
                            from="0"
                            to="1"
                            duration="0"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <FloatAnimation property="waveOpacity"
                            from="Current"
                            to="0"
                            duration="200"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

Здесь именованная анимация запускает волну, а числовые треки управляют её прозрачностью. `duration="0"` присваивает конечное значение сразу.

Полная запись настроек волны внутри `Storyboard`:

```xml
<Animation name="animationSoftPulse"
           from="0"
           to="1"
           duration="650"
           easing="CubicOut"
           intensity="0.75"
           spread="0.36"
           fadeExponent="1.6"/>
```

Также принимается имя `animationRippleWave`. Сейчас оба имени используют один обработчик; различие результата задаётся параметрами.

Для волны поддерживается `from="Current"`. Значения по умолчанию:

| Параметр | Значение |
|---|---|
| `from` / `to` | `0` / `1` |
| `duration` | `650` |
| `easing` | `CubicOut` |
| `intensity` | `0.45` |
| `spread` | `0.28` |
| `fadeExponent` | `2` |

Эти три настройки волны также можно задавать у `FloatAnimation property="waveProgress"`.

### 7. Несколько одновременных анимаций

```xml
<Border>
    <Border.Storyboards>
        <Storyboard trigger="Show">
            <Animation name="animationFade"
                       duration="200"/>
            <FloatAnimation property="renderOffsetX"
                            from="-40"
                            to="0"
                            duration="400"
                            easing="Linear"/>
        </Storyboard>
    </Border.Storyboards>
</Border>
```

Обе записи запускаются одновременно. Так же можно разместить несколько `Animation` или несколько `FloatAnimation`.

Если записи управляют одним и тем же каналом, последняя заменяет предыдущую. Несколько `Storyboard` с одинаковым событием тоже обрабатываются в порядке объявления.

Для `FloatAnimation` и встроенной волны доступны `Linear` и `CubicOut`; если `easing` пропущен, используется `CubicOut`.

### 8. Кастомная анимация

```xml
<Border>
    <Border.Storyboards>
        <Storyboard trigger="Show">
            <Animation name="animationMyReveal"
                       duration="300"
                       amplitude="12"/>
        </Storyboard>
        <Storyboard trigger="Hide">
            <Animation name="animationMyReveal"
                       duration="100"
                       amplitude="4"/>
        </Storyboard>
    </Border.Storyboards>
</Border>
```

`animationMyReveal` — условное имя: его нужно зарегистрировать. Атрибуты после `name` передаются как настройки; их смысл определяет обработчик.

В приложении уже зарегистрированы `animationPageTransition` и `animationSettingsReveal`. Их можно выбирать тем же способом:

```xml
<Page.Storyboards>
    <Storyboard trigger="Show">
        <Animation name="animationSettingsReveal"
                   duration="320"/>
    </Storyboard>
    <Storyboard trigger="Hide">
        <Animation name="animationSettingsReveal"
                   duration="120"/>
    </Storyboard>
</Page.Storyboards>
```

### 9. Независимые анимации родителя и ребёнка

```xml
<StackPanel>
    <StackPanel.Storyboards>
        <Storyboard trigger="Hide">
            <Animation name="animationFade"
                       duration="200"/>
        </Storyboard>
    </StackPanel.Storyboards>

    <Border>
        <Border.Storyboards>
            <Storyboard trigger="Hide">
                <Animation name="animationSlideFade"
                           duration="400"/>
            </Storyboard>
        </Border.Storyboards>
    </Border>
</StackPanel>
```

При скрытии родителя исчезновение запускается и у отображаемого ребёнка. Поддерево сохраняется до завершения исчезновения.

### 10. Явно отключить анимацию события

```xml
<Border>
    <Border.Storyboards>
        <Storyboard trigger="Hide"/>
    </Border.Storyboards>
</Border>
```

Пустой `Storyboard` подавляет штатную анимацию этого события. Отсутствие `Storyboard` оставляет возможность выполнить штатную анимацию.

### 11. Анимация вместе с кастомной отрисовкой

```xml
<Button text="Нажми"
        command="save"
        renderer="rendererWaveOutline">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationSoftPulse"/>
            <FloatAnimation property="waveOpacity"
                            from="0"
                            to="1"
                            duration="0"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

`renderer` выбирает отрисовку независимо от анимаций. Здесь используется уже зарегистрированный `rendererWaveOutline`.

### Текущие ограничения

Через XAML не задаются задержки, последовательности, повторы, `AutoReverse`, ключевые кадры, ссылки на общие анимации через ресурсы и binding параметров внутри `Animation`. Атрибуты `mode="Append"` / `mode="Replace"` также отсутствуют.

Старые `animation="..."`, `<Element.Animation>`, `RendererAnimation` и синтаксис `effect` больше не поддерживаются.

Примеры в отдельных файлах: [PageTransitions.xaml](Examples/Pages/PageTransitions.xaml) и [CustomPageAnimation.xaml](Examples/Pages/CustomPageAnimation.xaml).

## Типизированное состояние и регистрация

Состояние объявляется структурой C++. В ней находятся и текущие анимируемые
значения, и настройки запуска. Одна структура используется анимацией и renderer,
но у каждого элемента свой экземпляр. Совпадение строковых имён регистраций
не связывает обработчики: связь задаётся типом состояния.

Встроенный пример — пара `animationGlow` / `rendererGlow`:

```cpp
struct Glow {
    float intensity = 0.0f;
    float targetIntensity = 0.8f;
    int duration = 300;
};
```

Этот тип уже объявлен в `VisualState.h` и зарегистрирован конструктором
`StateRegistry`. Повторно регистрировать встроенные типы не нужно.
Пример разметки: [ButtonGlow.xaml](Examples/Buttons/ButtonGlow.xaml).

Для собственного состояния регистрация выглядит так:

```cpp
struct CustomGlow {
    float intensity = 0.0f;
    float targetIntensity = 0.8f;
    int duration = 300;
};

bool ValidDuration(const int& value) {
    return value >= 0;
}

bool AnimateCustomGlow(xaml::AnimationContext<CustomGlow>& context) {
    const auto& state = context.State();
    context.Animate(
        &CustomGlow::intensity,
        state.targetIntensity,
        std::chrono::milliseconds(state.duration));
    return true;
}

bool RenderCustomGlow(
    const xaml::Element& element,
    xaml::RenderContext<CustomGlow>& context) {
    context.RenderDefault();
    auto color = element.Foreground();
    color.alpha *= context.State().intensity * context.Opacity();
    context.Backend().DrawRoundedRectOutline(
        context.Bounds(), color, element.CornerRadius(), 3.0f);
    return true;
}

xaml::StateRegistry states;
states.Register<CustomGlow>();

xaml::AnimationRegistry animations(states);
animations.Register<CustomGlow>("animationCustomGlow", {
    xaml::Option("intensity", &CustomGlow::targetIntensity),
    xaml::Option("duration", &CustomGlow::duration, ValidDuration),
}, AnimateCustomGlow);

xaml::RendererRegistry renderers(states);
renderers.Register<CustomGlow>("rendererCustomGlow", RenderCustomGlow);
```

Обработчики — обычные указатели на функции. Захват состояния в лямбдах не нужен.
Внутри реестров используются адаптеры для хранения регистраций разных типов.

`Option` сохраняет указатель на поле класса, а не адрес отдельного значения.
Компилятор проверяет принадлежность поля типу состояния, поддерживаемый тип
настройки и сигнатуру обработчика. Поддерживаются настройки `float`, `int`,
`bool` и `std::string`. Анимировать методом `Animate` можно поля `float`.

Имена регистраций, имена настроек и типы состояний не могут дублироваться в
соответствующем реестре: повторная регистрация даёт ошибку. Для другого поведения
нужно зарегистрировать другое имя. Состояние необходимо объявить до создания
реестров, которые его используют: они сохраняют копию объявлений.

В XAML указываются только объявленные настройки:

```xml
<Button text="Подсветка"
        command="glow"
        renderer="rendererCustomGlow">
    <Button.Storyboards>
        <Storyboard trigger="PointerDown">
            <Animation name="animationCustomGlow"
                       intensity="0.8"
                       duration="300"/>
        </Storyboard>
        <Storyboard trigger="PointerUp">
            <Animation name="animationCustomGlow"
                       intensity="0"
                       duration="150"/>
        </Storyboard>
    </Button.Storyboards>
</Button>
```

При подключении дерева настройки известных анимаций проверяются по схеме.
Неизвестный атрибут, неверный тип, повторное имя настройки или нарушение
валидатора — ошибка. Строки не преобразуются с игнорированием лишних символов;
числа должны быть конечными. `name` выбирает регистрацию и не входит в настройки.

Перед событием движок копирует только объявленные поля настроек в состояние.
Пропущенные настройки получают значения из новой структуры по умолчанию.
Текущие анимируемые поля при этом сохраняются. Передавая одно поле одновременно
как настройку и как анимируемое значение, автор намеренно задаёт ему новое
начальное значение при событии; для плавного движения от текущего значения
следует использовать отдельное поле цели, как `targetIntensity`.

`Animate` сохраняет текущие from/to/duration в треке. Последующее изменение
настроек не меняет уже выполняющийся трек. Новый трек для того же поля заменяет
старый и начинает с текущего значения. Нулевая длительность присваивает цель сразу.

Строковые `AnimationValue`, `SetValue` и `AnimateTo` удалены.
Renderer читает `context.State().intensity`; код вне обработчика может читать
`element.State<CustomGlow>()` после подготовки состояния.
Обращение к неподготовленному типу даёт ошибку, а не значение по умолчанию.

## Подключение, обновление и отрисовка

```cpp
xaml::layout(root, availableSize);
controller.Attach(root, animations, true);
renderers.Prepare(root);

controller.Start(button, xaml::AnimationTrigger::pointerDown);
controller.Update();
xaml::Render(root, backend, renderers);
```

`Attach` копирует регистрации анимаций и подготавливает объявленные состояния.
`true` запускает первое появление; `false` принимает текущую видимость без
анимации. Повторный `Attach` очищает треки и состояния поддерева.

`RendererRegistry::Prepare` подготавливает состояния выбранных renderer.
`Render` вызывает его автоматически, поэтому явный вызов нужен только для
предварительной подготовки. Уже существующее состояние не пересоздаётся:
анимация и renderer используют один объект типа на элемент.

Один `AnimationController` обновляет и поля структур, и обычные
`FloatAnimation`. Отдельного цикла для кастомных анимаций нет. Контроллер
учитывает скорость воспроизведения и не обновляет дочерний элемент дважды,
если он уже входит в отслеживаемое дерево.
`AnimationController::Update(root, elapsed)` использует тот же проход
с явно заданным временем для детерминированного хоста или тестов.

Контекст анимации предоставляет `Target()`, `Trigger()`,
`IsStartingFromHidden()`, `Parameters()` и типизированный `State()`.
`AnimateProperty` остаётся способом менять обычные свойства элемента.
Контекст renderer предоставляет состояние только для чтения.

`ContainerAnimation` содержит объявленные каналы `opacity`, `offsetX`,
`offsetY` и настройки контейнерных переходов. Стандартная отрисовка применяет
эти каналы к элементу и его содержимому, не меняя базовые свойства layout.
Другие структуры, например `Glow`, интерпретирует соответствующий renderer.

## Штатное поведение и завершение

Записи подходящих Storyboard запускаются одновременно в порядке объявления.
Если ни одна запись не обработала событие, используется `SetDefaultAnimation`.
Пустой Storyboard явно подавляет штатную анимацию.

Обработчик анимации возвращает `true`, когда обработал событие. Для дополнения
штатной анимации он вызывает `context.StartDefaultAnimation()` перед созданием
своих треков. Штатный обработчик получает собственные настройки по умолчанию.
При `false` изменения собственного типизированного состояния обработчика
и созданные им треки откатываются. Прямые изменения свойств Element не являются
транзакционными: отказывающийся обработчик должен вернуть `false` до таких изменений.

`RenderDefault()` рисует стандартный элемент и содержимое,
`RenderChildren()` — только содержимое. Совместный вызов не дублирует детей.
Renderer возвращает `true` для собственной отрисовки; `false` или неизвестное
имя передаёт рисование стандартному renderer.

После завершения трек удаляется, но конечное значение поля сохраняется.
Например, `Glow::intensity` остаётся `0.8`, пока следующее событие не изменит его.

## Параметры события

`SetVisibility` остаётся обычным setter. Провайдер дополнительных данных
назначается при инициализации:

```cpp
page.SetAnimationParametersProvider([this]() {
    return xaml::AnimationParameters::Create(this->navigation.CurrentTransition());
});
```

Каждое новое событие сохраняет снимок данных. Обработчик читает его через
`context.Parameters().TryGet<PageTransitionData>()`. Это не настройки XAML
и не состояние визуального эффекта: это данные события, например страницы
отправления и назначения. Снимок не перечитывается каждый кадр.

## Видимость и удаление

При скрытии родителя запускаются Hide видимых потомков без изменения их
собственного Visibility. Исчезающие элементы сразу перестают получать ввод
и занимают место до завершения своих треков исчезновения и исчезновения потомков.
Затем Collapsed освобождает место, а Hidden сохраняет его.
Треки нажатия не задерживают завершение Hide.

`RemoveChild` сохраняет элемент до конца исчезновения. Контроллер хранит
слабые lifetime-токены и не обращается к уничтоженным элементам.
Прямое удаление через mutable Children или уничтожение корня обходит этот
механизм; корень нужно сохранять до конца перехода.

## MobileClock и предпросмотр

PageManager регистрирует `animationPageTransition` и `animationSettingsReveal`,
назначает провайдеры навигационных данных и меняет видимость страниц.
В runtime нет специальных событий Forward/Backward.

`animationPageTransition` сохраняет горизонтальный переход 240 мс и fade 180 мс.
`animationSettingsReveal` использует отдельную логику Show/Hide и настройку
`duration`; в примере заданы 320 и 120 мс соответственно.

XamlPreviewer использует тот же runtime и проверку схем. В нём доступны встроенные
анимации, `rendererGlow` и `rendererWaveOutline`. Обработчики приложения,
не зарегистрированные в хосте предпросмотра, используют штатный fallback.

## Встроенные шейдеры, подмена и эффекты приложения

OpenGlRenderer сам содержит и компилирует шейдеры стандартной геометрии, текста,
изображений и волны кнопки. MobileClock и XamlPreviewer не загружают Ripple.vert /
Ripple.frag из ресурсов. Пустой ShaderProgramSources означает использование
встроенных программ. Функция MobileClock CreateShaderPrograms() сейчас возвращает
пустой набор и остаётся местом для подключения программ приложения.

Стандартный renderer волны обращается к публичному идентификатору
`xaml::BuiltinShaders::buttonWave`. Это ключ только программы волны, а не всей
кнопки. Приложению не нужно повторять его строковое значение.

Для подмены волны передайте новую пару GLSL-исходников под этим ключом:

```cpp
es_renderer::OpenGlRenderer::ShaderProgramSources programs;
programs.emplace(
    xaml::BuiltinShaders::buttonWave,
    es_renderer::OpenGlRenderer::ShaderProgramSource{
        vertexSource,
        fragmentSource,
    });
```

Здесь vertexSource и fragmentSource — исходники GLSL, а не имена файлов.
Их память должна оставаться действительной до завершения конструктора
OpenGlRenderer, которому передаётся programs. Встроенные исходники имеют
статическое время жизни. Подмена применяется при создании backend; это не API
горячего обновления шейдера.

При наличии пользовательской программы встроенная версия этого ключа не
компилируется. Ошибочная или пустая подмена вызывает ошибку, а не молчаливый
возврат к встроенной версии.

XAML остаётся обычным: Animation с name="animationSoftPulse" изменяет прогресс,
а FloatAnimation с property="waveOpacity" управляет прозрачностью. Подмена
шейдера влияет на все стандартные волны данного экземпляра backend.
Выбора шейдера отдельной кнопки атрибутом shader пока нет.

Совместимая подмена волны использует vertex attributes position (location 0)
и localPosition (location 1); uniforms size, cornerRadius, progress, spread,
rippleColor. Backend передаёт size, стандартный renderer — остальные uniforms.

Полностью собственный эффект регистрирует программу под собственным ключом:

```cpp
programs.emplace(
    "shaderMyEffect",
    es_renderer::OpenGlRenderer::ShaderProgramSource{
        customVertexSource,
        customFragmentSource,
    });
```

Его renderer вызывает программу через обычный API:

```cpp
context.Backend().DrawShader(
    "shaderMyEffect",
    context.Bounds(),
    {
        {"intensity", {context.State().intensity}, 1},
        {"opacity", {context.Opacity()}, 1},
    });
```

Это фрагмент обработчика renderer со структурой состояния, содержащей intensity.
Схема uniforms определяется самим эффектом. Renderer должен учитывать общую
прозрачность и при необходимости рисовать стандартный элемент или его содержимое.

Программы с собственными ключами добавляются к встроенным, а не отключают их.
Движок ничего не знает об имени shaderMyEffect: его выбирает только renderer
приложения. Таким образом, встроенная волна, её явная подмена и независимые
эффекты приложения могут использоваться одновременно.

## Проверки

Исходники проверок находятся в `Native/Tests/PageTransitions`.
Они покрывают привязку типизированных полей, ошибки схем, отдельные состояния
элементов, renderer + animationGlow, прерывание, видимость, удаление и навигацию.
Сборки и выполнение проверок запускаются отдельно по запросу.