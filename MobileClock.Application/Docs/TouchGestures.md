# Обработка касаний и жестов

`NativeRenderSurfaceView` передаёт Android `MotionEvent` через JNI в
`ApplicationSession`, затем `PageManager` передаёт его текущему
`InputDispatcher`. Все решения о жестах находятся в native-коде и одинаковы
для Android и XamlPreviewer.

```text
SurfaceView → NativeRenderer → ApplicationSession → PageManager → InputDispatcher
```

## Последовательность касания

На `PointerDown` диспетчер сохраняет исходные координаты, hit-test target и
ближайший `ScrollViewer`. Пока палец не сдвинулся заметно, событие остаётся
кандидатом на tap.

Первый `PointerMove`, где максимальное смещение достигло 8 px, классифицирует
направление. Оно не зависит от конкретного контрола:

```cpp
enum class GestureDirection {
    none,
    up,
    down,
    left,
    right,
    upLeft,
    upRight,
    downLeft,
    downRight,
};
```

Если обе компоненты имеют не менее трёх четвертей большей компоненты, выбирается
диагональ; иначе — доминирующая ось. Выбранное направление фиксируется до
`PointerUp` или `Cancel`, поэтому жест не меняет владельца посередине.

## Решение владельца

`InputDispatcher` строит `PanState` и спрашивает зарегистрированные
`IGestureTarget`, которым принадлежит hit-tested элемент. Каждый target
возвращает один из трёх результатов:

```cpp
enum class GestureHandling {
    ignored,
    scroll,
    captured,
};

virtual GestureHandling ResolveGesture(
    const PanState& state,
    GestureDirection direction) const;
```

`captured` отменяет tap и направляет `BeginGesture`, `UpdateGesture`,
`EndGesture` и `CancelGesture` этому контролу. `scroll` передаёт жест
найденному `ScrollViewer`. `ignored` оставляет вертикальное направление
обычной прокрутке, если `ScrollViewer` существует; горизонтальное и
диагональное движение остаётся без действия.

```cpp
if (handling == GestureHandling::captured) {
    this->panTarget->BeginGesture(state);
} else if (handling == GestureHandling::scroll
    || handling == GestureHandling::ignored && IsVertical(direction)) {
    this->scrollController.Begin(*this->scrollViewer);
}
```

Таким образом `InputDispatcher` не содержит условий о списках, часах или
удалении. Чтобы добавить диагональный жест, контролу достаточно вернуть
`captured` для, например, `GestureDirection::upLeft`; распознавание и
маршрутизация уже существуют.

## Списки проекта

`ScrollableList` — минимальная база: она регистрируется как владелец и умеет
найти свой `ScrollViewer`. Она не захватывает жесты, поэтому вертикальный drag
автоматически остаётся прокруткой.

`InteractiveList` добавляет общую механику отложенного удаления, сохранения
viewport и анимации сдвига оставшихся строк. Конечный список определяет только
свои жесты через `ResolveInteractiveGesture` и четыре фазы:

```cpp
virtual GestureHandling ResolveInteractiveGesture(
    const PanState& state,
    GestureDirection direction) const = 0;
virtual void BeginInteractiveGesture(const PanState& state) = 0;
virtual void UpdateInteractiveGesture(const PanState& state) = 0;
virtual bool EndInteractiveGesture(
    const PanState& state,
    xaml::AnimationController& animations) = 0;
virtual void CancelInteractiveGesture(xaml::Element& element) = 0;
```

`AlarmList` захватывает `left` и `right` для строки будильника. `AlarmMelodyList`
захватывает только `left` у строки мелодии, раскрывая кнопку удаления; `right`
он игнорирует. Вертикальный drag на обеих строках остаётся прокруткой.

## Регистрация

Вложенный XAML-элемент не всегда может подняться через `Parent()` до C++
контрола. Поэтому каждый владелец вызывает `RegisterGestureTarget()` при
инициализации. `IGestureTarget::Find` проходит реестр, проверяет принадлежность
элемента и вызывает `ResolveGesture`; при обновлении кадров `IsIn(pageRoot)`
отсекает контролы не текущей страницы.