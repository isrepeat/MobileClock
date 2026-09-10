# Обработка касаний и жестов

Этот документ описывает путь ввода в MobileClock от события Android до действия
страницы или интерактивного контрола. Руководство построено от простого к
сложному: сначала сырые события, затем tap, прокрутка и кастомный pan.

## 1. Базовые события приходят от платформы

Приложение не генерирует события касания в цикле отрисовки. Android OS передаёт
`MotionEvent` в `SurfaceView` в ответ на реальные действия пользователя.
`NativeRenderSurfaceView` принимает событие и сразу передаёт его в JNI:

```kotlin
setOnTouchListener { _, event ->
    NativeRenderer.onTouch(event.actionMasked, event.x, event.y)
    true
}
```

`actionMasked` — вид базового события, `x` и `y` — координаты в пределах
`SurfaceView`. Kotlin-мост не распознаёт жесты:

```kotlin
fun onTouch(action: Int, x: Float, y: Float) {
    nativeTouch(action, x, y)
}
```

JNI вызывает `NativeApplication::Touch`, а тот — `NativeRenderer::Touch`. Здесь
Android action переводится в нейтральный API сессии:

```cpp
if (action == AMOTION_EVENT_ACTION_DOWN) {
    this->state->session.PointerDown(x, y);
    return;
}
if (action == AMOTION_EVENT_ACTION_MOVE) {
    this->state->session.PointerMove(x, y);
    return;
}
if (action == AMOTION_EVENT_ACTION_CANCEL) {
    this->state->session.CancelPointer();
    return;
}
if (action == AMOTION_EVENT_ACTION_UP) {
    this->state->session.PointerUp(x, y);
}
```

| Android `MotionEvent` | Событие сессии | Значение |
|---|---|---|
| `ACTION_DOWN` | `PointerDown` | Палец коснулся поверхности. |
| `ACTION_MOVE` | `PointerMove` | Координата активного пальца изменилась. |
| `ACTION_UP` | `PointerUp` | Палец отпущен. |
| `ACTION_CANCEL` | `CancelPointer` | Система отобрала последовательность ввода. |

Одна последовательность одного пальца обычно выглядит так:

```text
DOWN(300, 500)
MOVE(301, 504)  // ноль или больше событий
MOVE(303, 520)
UP(303, 520)
```

`DOWN` приходит один раз в начале этой последовательности, `UP` — один раз в
конце. `MOVE` может не прийти вовсе и не привязан к частоте кадров: Android
присылает его при обновлении ввода и может объединять очень частые перемещения.
Текущая реализация предназначена для одного активного pointer. Multi-touch
actions, например `ACTION_POINTER_DOWN`, отдельно не обрабатываются.

XamlPreviewer не использует Android OS, но передаёт мышиные события в тот же
API `ApplicationSession::PointerDown/Move/Up`. Поэтому всё ниже работает на
устройстве и в предпросмотре одинаково.

## 2. Сессия и PageManager только доставляют событие

`ApplicationSession` не интерпретирует координаты:

```cpp
void ApplicationSession::PointerDown(float x, float y) {
    this->pageManager.HandleTouchDown(x, y);
}
```

`PageManager` блокирует ввод только во время перехода между страницами и
направляет остальные события диспетчеру текущей страницы:

```cpp
void PageManager::HandleTouchDown(float x, float y) {
    if (this->isTransitioning) {
        return;
    }
    this->inputDispatcher.PointerDown(
        this->currentPage->Root(),
        x,
        y,
        &this->animations);
}
```

В этот момент ещё невозможно узнать, является ли действие tap или pan: палец
только коснулся экрана и ещё не перемещался.

```text
PageManager → InputDispatcher
```

`InputDispatcher` — первая точка, где из последовательности сырых событий
строится смысловой жест.

## 3. Самый простой результат: tap

На `PointerDown` диспетчер запоминает начальную точку и передаёт событие
`InteractionController` из XamlRuntime:

```cpp
this->touchDownX = x;
this->touchDownY = y;
this->lastTouchY = y;
this->gestureAxis = GestureAxis::none;

this->interactionController.PointerDown(root, *animations, x, y);
```

Если пользователь почти не сдвинул палец и отпустил его, runtime вернёт
`GestureResult` с `kind == tap`:

```cpp
const xaml::GestureResult result =
    this->interactionController.PointerUp(root, animations, x, y);

return result.kind == xaml::GestureKind::tap
    ? result.target
    : nullptr;
```

`PageManager` получает target-элемент и передаёт его странице:

```cpp
xaml::Element* const element = this->inputDispatcher.PointerUp(
    this->currentPage->Root(),
    x,
    y,
    this->animations);
if (element == nullptr) {
    return false;
}
this->currentPage->HandleTap(*element);
```

Поэтому обычной кнопке достаточно иметь стабильный `id`, а странице —
обработать её как обычное действие:

```cpp
void MainPageViewModel::HandleTap(xaml::Element& element) {
    if (element.Id() == "addAlarmButton") {
        this->AddAlarm();
    }
}
```

Tap не требует `IGestureTarget` и не требует отдельного контрола.

## 4. Что такое pan

Pan — это заметное перемещение активного пальца при удержании. Сам по себе pan
не определяет действие: вертикальный pan обычно прокручивает, горизонтальный
pan строки может удалить её, а у другого контрола — раскрыть меню или изменить
значение.

```text
DOWN + почти нет перемещения + UP → tap
DOWN + вертикальное перемещение  → вертикальный pan / scroll
DOWN + горизонтальное перемещение → пользовательский pan
```

Распознавание начинается не на `PointerDown`, а при первом либо последующем
`PointerMove`: только тогда есть с чем сравнивать исходную точку.

## 5. Вертикальный pan превращается в прокрутку

При `PointerDown` `InputDispatcher` пытается найти `ScrollViewer` под касанием.
Сначала используется visual hit-test и подъём к родителю:

```cpp
this->scrollViewer = xaml::HitTestVisual(root, x, y);
while (this->scrollViewer != nullptr
    && this->scrollViewer->Type() != xaml::ElementType::scrollViewer) {
    this->scrollViewer = this->scrollViewer->Parent();
}
```

При движении сравниваются смещения от сохранённой исходной точки:

```cpp
const float horizontalDistance = x - this->touchDownX;
const float verticalDistance = y - this->touchDownY;
constexpr float gestureThreshold = 8.0f;
```

Если направление ещё не выбрано, присутствует `ScrollViewer`, вертикальное
смещение больше горизонтального и оно не меньше 8 px, выбирается прокрутка:

```cpp
if (this->gestureAxis == GestureAxis::none
    && this->scrollViewer != nullptr
    && std::abs(verticalDistance) > std::abs(horizontalDistance)
    && std::abs(verticalDistance) >= gestureThreshold) {
    this->gestureAxis = GestureAxis::vertical;
    this->scrollController.Begin(*this->scrollViewer);
    this->interactionController.Cancel();
}
```

`Cancel()` принципиален: последовательность стала прокруткой и больше не должна
после отпускания случайно превратиться в tap либо горизонтальный pan.

После захвата все дальнейшие `MOVE` изменяют offset выбранного `ScrollViewer`:

```cpp
if (this->gestureAxis == GestureAxis::vertical) {
    const bool wasScrolled =
        this->scrollController.Drag(this->lastTouchY - y);
    this->lastTouchY = y;
    return wasScrolled;
}
```

На `PointerUp` прокрутка завершается и элемент для `HandleTap` не возвращается:

```cpp
if (this->gestureAxis == GestureAxis::vertical) {
    this->scrollController.End();
    this->scrollViewer = nullptr;
    this->gestureAxis = GestureAxis::none;
    return nullptr;
}
```

## 6. Пользовательский pan через IGestureTarget

Если движение не было захвачено вертикальной прокруткой, `InputDispatcher`
сам определяет horizontal pan и направляет каждую его фазу владельцу target-
элемента. Он не знает назначения жеста и не знает контролов MobileClock;
конкретная реакция принадлежит `IGestureTarget`.

Интерфейс отделяет общий маршрут ввода от поведения конкретного контрола и
передаёт все стадии pan:

```cpp
class IGestureTarget {
public:
    virtual bool CanHandlePan(const xaml::Element& element) const = 0;
    virtual xaml::Element* FindScrollViewer(const xaml::Element& element) const = 0;
    virtual void BeginPan(const PanState& state) = 0;
    virtual void UpdatePan(const PanState& state) = 0;
    virtual bool EndPan(
        const PanState& state,
        xaml::AnimationController& animations) = 0;
    virtual void CancelPan(xaml::Element& element) = 0;
    virtual void UpdateGestures(
        xaml::Element& pageRoot,
        xaml::AnimationController& animations) = 0;
};
```

`PanState` содержит корень текущей страницы, target-элемент, координаты down,
предыдущего move и текущего move. Поэтому контрол может реализовать собственную
геометрию и пороги без привязки к другому контролу.

Когда horizontal смещение впервые прошло 8 px, `InputDispatcher` отменяет
стандартный recognizer и вызывает `BeginPan`:

```cpp
this->gestureAxis = GestureAxis::horizontal;
this->interactionController.Cancel();
this->panTarget->BeginPan(state);
```

Каждый последующий `MOVE` направляется в `UpdatePan`:

```cpp
if (this->gestureAxis == GestureAxis::horizontal) {
    this->panTarget->UpdatePan(state);
    return true;
}
```

На `PointerUp` вызывается `EndPan`, а на системном `CANCEL` — `CancelPan`.
`InputDispatcher` не содержит условий о будильниках, удалении или конкретных
страницах. Он только распознаёт и маршрутизирует жестовые стадии.

### Кастомная live-анимация

`InteractiveList` сохраняет прежнее swipe-to-dismiss поведение, но теперь оно
явно реализовано самим контролом. Во время каждого `UpdatePan` карточка следует
за пальцем:

```cpp
void InteractiveList::UpdatePan(const PanState& state) {
    state.target.SetRenderOffsetX(state.currentX - state.downX);
}
```

На `EndPan` список сам выбирает порог 180 px. При меньшем смещении он
анимирует target обратно:

```cpp
if (std::abs(horizontalDistance) < PanCompletionThreshold) {
    animations.Animate(
        state.target,
        xaml::AnimatedProperty::renderOffsetX,
        state.target.RenderOffsetX(),
        0.0f,
        std::chrono::milliseconds(180));
    return false;
}
```

При достаточном расстоянии `InteractiveList` запускает удаление модели и
анимирует карточку за край. Другой `IGestureTarget` может вместо этого раскрыть
действия, ограничить offset, использовать иной порог или вообще не менять
`renderOffsetX`.

## 7. Почему нужен реестр обработчиков

Target жеста обычно является вложенным `Border` или `Grid`, а не самим
`UserControl`. Подъём по `Element::Parent()` через шаблонное XAML-дерево не во
всех случаях приводит к `UserControl`, поэтому интерфейс нельзя надёжно искать
через RTTI и `dynamic_cast`.

При инициализации интерактивный контрол явно регистрируется:

```cpp
void InteractiveList::OnInitialized() {
    this->RegisterGestureTarget();
}
```

Реестр выбирает контрол, который содержит target-элемент и подтверждает
обработку pan:

```cpp
for (IGestureTarget* const target : _details::gestureTargets) {
    if (target->Owns(element)
        && target->CanHandlePan(element)) {
        return target;
    }
}
```

`element` всегда получен hit-test от root текущей страницы, поэтому `Owns` уже
исключает контролы других страниц. Деструктор `IGestureTarget` удаляет контрол
из реестра. При кадровом `Update`, где target-элемента нет, `IsIn(pageRoot)`
по-прежнему отсекает контролы других страниц.

## 8. Пример: InteractiveList и удаление строки

`InteractiveList` разрешает pan только с явно помеченной области строки:

```cpp
bool InteractiveList::CanHandlePan(const xaml::Element& element) const {
    return element.Id() == "interactiveListGestureTarget";
}
```

Когда pan завершён, контрол определяет модель строки. Runtime может передать
`itemIndex`; если индекса нет, контрол ищет target в собственном дереве и
наследует ближайший непустой `DataContext` контейнера строки:

```cpp
const void* const dataContext = list != nullptr
    && gesture.itemIndex >= 0
    && static_cast<size_t>(gesture.itemIndex) < list->Children().size()
        ? list->Children()[gesture.itemIndex]->DataContext()
        : this->FindItemDataContext(*gesture.target);
```

Список не знает тип модели. При создании в него передаётся callback ViewModel:

```cpp
control->removeHandler = [&viewModel](const void* dataContext) {
    return viewModel.RemoveItem(dataContext);
};
```

Сначала `InteractiveList` сохраняет viewport и откладывает действие на 220 ms.
На очередном обновлении он вызывает `removeHandler`, а после успешного удаления
восстанавливает scroll offset и анимирует сдвиг оставшихся строк:

```cpp
if (this->removeHandler && this->removeHandler(dataContext)) {
    this->RestoreViewportAndAnimate(
        state,
        pageRoot,
        animations,
        std::chrono::milliseconds(840));
}
```

Обновление выполняется из обычного кадра приложения:

```cpp
void PageManager::UpdateClock() {
    this->animations.Update();
    this->inputDispatcher.Update(this->currentPage->Root(), this->animations);
    this->pages.ForEach([](IPage& page) {
        page.Update();
    });
}
```

Это не генерирует новые touch-события. Оно только продвигает анимации,
инерцию прокрутки и отложенные UI-действия.

Итого, у удаления строки есть две разные анимации:

```text
MOVE:      InteractiveList::UpdatePan обновляет renderOffsetX выбранной карточки.
UP:        InteractiveList::EndPan анимирует её за край либо обратно.
После UP:  InteractiveList удаляет модель и анимирует renderOffsetY остальных строк.
```

## 9. Как выбрать подходящий уровень

| Требование | Достаточная реализация |
|---|---|
| Нажатие на кнопку | `PageViewModel::HandleTap`. |
| Вертикальная прокрутка области | `ScrollViewer`; `InputDispatcher` обработает drag. |
| Новый горизонтальный или иной пользовательский pan | Контрол, реализующий `IGestureTarget`. |
| Интерактивный список с удалением и анимацией перестроения | `InteractiveList` и callback ViewModel. |
| Pinch, rotate, несколько пальцев | Расширить платформенный API pointer id и `InputDispatcher`; текущей модели одного pointer недостаточно. |

Главный принцип: платформа поставляет только последовательность координат,
`InputDispatcher` распознаёт и маршрутизирует жест, контрол реализует
универсальную UI-механику, а ViewModel изменяет данные приложения.