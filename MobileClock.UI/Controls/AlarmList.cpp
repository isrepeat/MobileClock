#include "MobileClock.UI/Controls/AlarmList.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/Animation.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mobileclock::ui::controls::_details {
    constexpr float PanCompletionThreshold = 180.0f;

    xaml::Element* FindElement(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const std::unique_ptr<xaml::Element>& child : element.Children()) {
            if (xaml::Element* const found = FindElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }
}

namespace mobileclock::ui::controls {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view AlarmList::RuntimeClassName() const {
        return "mobileclock::ui::controls::AlarmList";
    }

    bool AlarmList::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) {
        try {
            const auto& content = templateNode.name == "UserControl" ? templateNode.children.at(0) : templateNode;
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(content, context,
                {this->Bounds().width, this->Bounds().height});
            xaml::Element* const newList = _details::FindElement(*result.root, "interactiveListItems");
            xaml::Element* const newScroll = _details::FindElement(*result.root, "interactiveListScrollViewer");
            if (newList == nullptr || newScroll == nullptr || newList->Type() != xaml::ElementType::listView
                || newScroll->Type() != xaml::ElementType::scrollViewer) {
                throw std::invalid_argument("AlarmList requires interactiveListItems and interactiveListScrollViewer");
            }
            if (const auto* scroll = this->FindElement("interactiveListScrollViewer")) {
                newScroll->SetHorizontalOffset(scroll->HorizontalOffset());
                newScroll->SetVerticalOffset(scroll->VerticalOffset());
            }
            if (context.prepareTree) {
                context.prepareTree(*result.root);
            }
            if (context.beforeCommit) {
                context.beforeCommit();
            }
            this->ReplaceContent(std::move(result.root));
            this->runtimeBindings = std::move(result.bindings);
            diagnostics.clear();
            return true;
        } catch (const std::exception& error) {
            diagnostics = error.what();
            return false;
        }
    }

    //
    // API
    //
    void AlarmList::PreserveInstances(xaml::Element& previous, xaml::Element& replacement, xaml::BindingScope& bindings) {
        std::vector<AlarmList*> oldControls;
        std::vector<AlarmList*> newControls;
        const auto collect = [](auto&& self, xaml::Element& node, std::vector<AlarmList*>& controls) -> void {
            if (auto* control = dynamic_cast<AlarmList*>(&node)) {
                controls.push_back(control);
                return;
            }
            for (const auto& child : node.Children()) {
                self(self, *child, controls);
            }
        };
        collect(collect, previous, oldControls);
        collect(collect, replacement, newControls);
        for (auto* next : newControls) {
            const auto found = std::find_if(oldControls.begin(), oldControls.end(), [next](const auto* old) {
                return old != nullptr && old->Id() == next->Id() && typeid(*old) == typeid(*next);
            });
            if (found != oldControls.end()) {
                (*found)->CopyLayoutFrom(*next);
                bindings.RetargetRuntimeElement(*next, **found);
                (*found)->SwapTreePosition(*next);
                *found = nullptr;
            }
        }
    }
#endif

    //
    // API
    //
    const xaml::DependentProperty<const void*>& AlarmList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    //
    // Internal
    //
    std::string_view AlarmList::ScrollViewerId() const {
        return "interactiveListScrollViewer";
    }

    std::string_view AlarmList::ListViewId() const {
        return "interactiveListItems";
    }

    GestureHandling AlarmList::ResolveInteractiveGesture(
        const PanState& state,
        GestureDirection direction) const {
        if (state.target.Id() != "interactiveListGestureTarget") {
            return GestureHandling::ignored;
        }
        return direction == GestureDirection::left || direction == GestureDirection::right
            ? GestureHandling::captured : GestureHandling::ignored;
    }

    void AlarmList::BeginInteractiveGesture(const PanState&) {
    }

    void AlarmList::UpdateInteractiveGesture(const PanState& state) {
        state.target.SetRenderOffsetX(state.currentX - state.downX);
    }

    bool AlarmList::EndInteractiveGesture(const PanState& state, xaml::AnimationController& animations) {
        const float horizontalDistance = state.currentX - state.downX;
        if (std::abs(horizontalDistance) < _details::PanCompletionThreshold || !this->RequestRemoval(state.target)) {
            animations.Animate(
                state.target,
                xaml::AnimatedProperty::renderOffsetX,
                state.target.RenderOffsetX(),
                0.0f,
                std::chrono::milliseconds(180));
            return false;
        }
        const xaml::Rect rootBounds = state.root.Bounds();
        const xaml::Rect targetBounds = state.target.Bounds();
        const float targetOffset = horizontalDistance < 0.0f
            ? -targetBounds.x - targetBounds.width
            : rootBounds.width - targetBounds.x;
        animations.Animate(
            state.target,
            xaml::AnimatedProperty::renderOffsetX,
            state.target.RenderOffsetX(),
            targetOffset,
            std::chrono::milliseconds(220));
        return true;
    }

    void AlarmList::CancelInteractiveGesture(xaml::Element& element) {
        element.SetRenderOffsetX(0.0f);
    }
}