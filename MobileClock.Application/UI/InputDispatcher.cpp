#include "UI/InputDispatcher.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <Helpers.Logging/Logging.h>
#endif
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include <algorithm>
#include <cmath>

namespace mobileclock::ui::_details {
    // До этого расстояния последовательность остаётся кандидатом на tap.
    constexpr float GestureThreshold = 8.0f;
    // Диагональ выбирается только при сопоставимых смещениях по обеим осям:
    // обычный слегка неровный swipe всё ещё остаётся горизонтальным или вертикальным.
    constexpr float DiagonalRatio = 0.75f;

    GestureDirection DirectionFrom(float horizontalDistance, float verticalDistance) {
        const float horizontalMagnitude = std::abs(horizontalDistance);
        const float verticalMagnitude = std::abs(verticalDistance);
        if (std::max(horizontalMagnitude, verticalMagnitude) < GestureThreshold) {
            return GestureDirection::none;
        }
        // Для диагонали важны одновременно знак каждой компоненты и близость
        // их модулей. Знак Y обратен экранной оси: отрицательное значение — вверх.
        if (std::min(horizontalMagnitude, verticalMagnitude)
            >= std::max(horizontalMagnitude, verticalMagnitude) * DiagonalRatio) {
            if (verticalDistance < 0.0f) {
                return horizontalDistance < 0.0f ? GestureDirection::upLeft : GestureDirection::upRight;
            }
            return horizontalDistance < 0.0f ? GestureDirection::downLeft : GestureDirection::downRight;
        }
        if (horizontalMagnitude > verticalMagnitude) {
            return horizontalDistance < 0.0f ? GestureDirection::left : GestureDirection::right;
        }
        return verticalDistance < 0.0f ? GestureDirection::up : GestureDirection::down;
    }

    bool IsVertical(GestureDirection direction) {
        // Диагональ намеренно не считается прокруткой по умолчанию: её должен
        // явно захватить контрол, иначе случайный наклон пальца запустит scroll.
        return direction == GestureDirection::up || direction == GestureDirection::down;
    }
}

namespace mobileclock::ui {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // API
    //
    InputDispatcher::RuntimePanState InputDispatcher::CaptureRuntimePan() const {
        // Состояние прокрутки восстанавливает сам ScrollViewer. Сохраняем только
        // уже захваченный контролом жест, чтобы hot reload не обрывал swipe.
        if (this->activeGesture != ActiveGesture::target || this->panElement == nullptr) {
            return {};
        }
        return {this->panElement->Id(), this->panElement->DataContext(), this->touchDownX,
            this->touchDownY, this->lastTouchX, this->lastTouchY, this->gestureDirection, true};
    }

    void InputDispatcher::RestoreRuntimePan(xaml::Element& root, const RuntimePanState& state) {
        if (!state.active) {
            return;
        }
        // id недостаточно при повторяющихся строках списка, поэтому сверяем и
        // DataContext — это тот же объект модели, который был под пальцем.
        const auto find = [&state](auto&& self, xaml::Element& element) -> xaml::Element* {
            if (element.Id() == state.id && element.DataContext() == state.dataContext) {
                return &element;
            }
            for (const auto& child : element.Children()) {
                if (auto* result = self(self, *child)) {
                    return result;
                }
            }
            return nullptr;
        };
        auto* element = find(find, root);
        if (element == nullptr) {
            return;
        }
        const IGestureTarget::PanState panState{
            root,
            *element,
            state.downX,
            state.downY,
            state.currentX,
            state.currentY,
            state.currentX,
            state.currentY,
        };
        // После перестройки дерева контрол мог исчезнуть или перестать принимать
        // это направление. В таком случае не восстанавливаем устаревший жест.
        auto* target = IGestureTarget::Find(*element, panState, state.direction);
        if (target == nullptr) {
            return;
        }
        this->inputRoot = &root;
        this->panElement = element;
        this->panTarget = target;
        this->touchDownX = state.downX;
        this->touchDownY = state.downY;
        this->lastTouchX = state.currentX;
        this->lastTouchY = state.currentY;
        this->activeGesture = ActiveGesture::target;
        this->gestureDirection = state.direction;
        if (_details::IsVertical(state.direction)) {
            this->panElement->SetRenderOffsetY(state.currentY - state.downY);
        } else {
            this->panElement->SetRenderOffsetX(state.currentX - state.downX);
        }
    }
#endif
    //
    // API
    //
    void InputDispatcher::PointerDown(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController* animations) {
        if (animations == nullptr) {
            return;
        }
        // Hit-test выполняется один раз: даже если строка сместится из-под пальца,
        // вся последовательность MOVE/UP относится к исходному элементу.
        xaml::Element* const target = xaml::HitTest(root, x, y);
        this->inputRoot = &root;
        this->panElement = target;
        this->panTarget = nullptr;
        this->scrollViewer = xaml::HitTestVisual(root, x, y);
        // Visual hit-test может попасть в дочерний элемент ScrollViewer.
        // Поднимаемся до самого контейнера прокрутки.
        while (this->scrollViewer != nullptr
            && this->scrollViewer->Type() != xaml::ElementType::scrollViewer) {
            this->scrollViewer = this->scrollViewer->Parent();
        }
        if (this->scrollViewer == nullptr) {
            if (target != nullptr) {
                // Шаблон UserControl не всегда виден через Parent(). Реестр
                // контролов даёт запасной путь к ScrollViewer списка.
                this->scrollViewer = IGestureTarget::FindContainingScrollViewer(*target);
            }
        }
        this->scrollController.Cancel();
        this->touchDownX = x;
        this->touchDownY = y;
        this->lastTouchX = x;
        this->lastTouchY = y;
        this->activeGesture = ActiveGesture::none;
        this->gestureDirection = GestureDirection::none;
        // Кастомные pan-ы обрабатывает этот диспетчер. Отключаем recognizer
        // runtime, иначе он добавил бы второе смещение тому же элементу.
        this->interactionController.SetPanTargetPredicate([](const xaml::Element&) {
            return false;
        });
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        LOG_DEBUG(
            "MobileClock.Input",
            "Pointer down: target='{}'",
            target == nullptr ? "" : target->Id());
#endif
        this->interactionController.PointerDown(root, *animations, x, y);
    }

    bool InputDispatcher::PointerMove(float x, float y) {
        // Направление всегда измеряется от точки DOWN, а не от прошлого MOVE:
        // короткие колебания пальца не смогут изменить владельца жеста.
        const float horizontalDistance = x - this->touchDownX;
        const float verticalDistance = y - this->touchDownY;
        if (this->activeGesture == ActiveGesture::none && this->panElement != nullptr) {
            const GestureDirection direction = _details::DirectionFrom(horizontalDistance, verticalDistance);
            if (direction != GestureDirection::none) {
                const IGestureTarget::PanState state{
                    *this->inputRoot, *this->panElement, this->touchDownX, this->touchDownY,
                    this->lastTouchX, this->lastTouchY, x, y};
                // Ищем первый зарегистрированный контрол, которому принадлежит
                // элемент и который готов работать с данным направлением.
                this->panTarget = IGestureTarget::Find(*this->panElement, state, direction);
                const GestureHandling handling = this->panTarget == nullptr
                    ? GestureHandling::ignored
                    : this->panTarget->ResolveGesture(state, direction);
                if (handling == GestureHandling::captured) {
                    // Контрол получает все следующие фазы и самостоятельно
                    // управляет визуальным состоянием, например offset строки.
                    this->activeGesture = ActiveGesture::target;
                    this->gestureDirection = direction;
                    this->interactionController.Cancel();
                    this->panTarget->BeginGesture(state);
                } else if ((handling == GestureHandling::scroll
                    || handling == GestureHandling::ignored && _details::IsVertical(direction))
                    && this->scrollViewer != nullptr) {
                    // Вертикальный жест без явного владельца — обычная прокрутка.
                    // Контрол также может вернуть scroll, чтобы запросить её явно.
                    this->activeGesture = ActiveGesture::scroll;
                    this->gestureDirection = direction;
                    this->scrollController.Begin(*this->scrollViewer);
                    this->interactionController.Cancel();
                }
            }
        }
        if (this->activeGesture == ActiveGesture::scroll) {
            // ScrollController ожидает изменение между соседними MOVE, поэтому
            // здесь используется lastTouchY, а не точка первоначального DOWN.
            const bool wasScrolled = this->scrollController.Drag(this->lastTouchY - y);
            this->lastTouchY = y;
            return wasScrolled;
        }
        if (this->activeGesture == ActiveGesture::target) {
            // После захвата направление больше не пересчитывается и не ищется
            // другой target: жест нельзя разделить между двумя владельцами.
            this->panTarget->UpdateGesture({
                *this->inputRoot,
                *this->panElement,
                this->touchDownX,
                this->touchDownY,
                this->lastTouchX,
                this->lastTouchY,
                x,
                y,
            });
            this->lastTouchX = x;
            this->lastTouchY = y;
            return true;
        }
        return this->interactionController.PointerMove(x, y);
    }

    xaml::Element* InputDispatcher::PointerUp(
        xaml::Element& root,
        float x,
        float y,
        xaml::AnimationController& animations) {
        if (this->activeGesture == ActiveGesture::scroll) {
            // UP завершает инерцию прокрутки и никогда не превращается в tap.
            this->scrollController.End();
            this->scrollViewer = nullptr;
            this->panTarget = nullptr;
            this->inputRoot = nullptr;
            this->panElement = nullptr;
            this->activeGesture = ActiveGesture::none;
            this->gestureDirection = GestureDirection::none;
            return nullptr;
        }
        if (this->activeGesture == ActiveGesture::target) {
            // Конечный контрол решает, зафиксировать действие или вернуть свой
            // элемент в исходное состояние анимацией.
            const bool wasHandled = this->panTarget->EndGesture({
                *this->inputRoot,
                *this->panElement,
                this->touchDownX,
                this->touchDownY,
                this->lastTouchX,
                this->lastTouchY,
                x,
                y,
            }, animations);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
            LOG_DEBUG(
                "MobileClock.Input",
                "Pan completed: target='{}', handled={}",
                this->panElement->Id(),
                wasHandled);
#endif
            this->scrollViewer = nullptr;
            this->panTarget = nullptr;
            this->inputRoot = nullptr;
            this->panElement = nullptr;
            this->activeGesture = ActiveGesture::none;
            this->gestureDirection = GestureDirection::none;
            return nullptr;
        }
        this->scrollViewer = nullptr;
        // Только незахваченная последовательность передаётся recognizer-у tap.
        const xaml::GestureResult result = this->interactionController.PointerUp(root, animations, x, y);
        this->panTarget = nullptr;
        this->inputRoot = nullptr;
        this->panElement = nullptr;
        return result.kind == xaml::GestureKind::tap ? result.target : nullptr;
    }

    void InputDispatcher::Cancel() {
        // Android может отменить последовательность без PointerUp, например при
        // передаче ввода системному жесту. Контрол должен убрать промежуточное
        // состояние так же, как при отменённой собственной анимации.
        if (this->activeGesture == ActiveGesture::target
            && this->panTarget != nullptr
            && this->panElement != nullptr) {
            this->panTarget->CancelGesture(*this->panElement);
        }
        this->interactionController.Cancel();
        this->scrollController.Cancel();
        this->scrollViewer = nullptr;
        this->panTarget = nullptr;
        this->inputRoot = nullptr;
        this->panElement = nullptr;
        this->activeGesture = ActiveGesture::none;
        this->gestureDirection = GestureDirection::none;
    }

    bool InputDispatcher::Update(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        // Вводные события не вызывают эти обновления сами: кадр продвигает инерцию
        // ScrollViewer и отложенные действия интерактивных контролов отдельно.
        const bool interactionUpdated = this->interactionController.Update();
        const bool scrollUpdated = this->scrollController.Update();
        IGestureTarget::Update(pageRoot, animations);
        return interactionUpdated || scrollUpdated;
    }
}