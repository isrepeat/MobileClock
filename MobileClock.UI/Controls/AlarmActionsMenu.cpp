#include "AlarmActionsMenu.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/Animation.h>

#include <stdexcept>
#include <algorithm>
#include <chrono>

namespace mobileclock::ui::controls::_details {
    xaml::Element* FindMenuElement(xaml::Element& root, std::string_view id) {
        if (root.Id() == id) {
            return &root;
        }
        for (const auto& child : root.Children()) {
            if (auto* result = FindMenuElement(*child, id)) {
                return result;
            }
        }
        return nullptr;
    }

    bool MenuContains(const xaml::Element& root, const xaml::Element& element) {
        if (&root == &element) {
            return true;
        }
        for (const auto& child : root.Children()) {
            if (MenuContains(*child, element)) {
                return true;
            }
        }
        return false;
    }
}

namespace mobileclock::ui::controls {
    //
    // UserControl
    //
    void AlarmActionsMenu::OnInitialized() {
        this->RegisterGestureTarget();
        this->ApplyState(false);
    }

    //
    // IGestureTarget
    //
    bool AlarmActionsMenu::CanHandlePan(const xaml::Element& element) const {
        return this->Owns(element)
            && this->StateValue("Expanded", "alarmActionsPanel", xaml::AnimatedProperty::height)
                > this->StateValue("Collapsed", "alarmActionsPanel", xaml::AnimatedProperty::height);
    }

    bool AlarmActionsMenu::IsVerticalPan() const {
        return true;
    }

    xaml::Element* AlarmActionsMenu::FindScrollViewer(const xaml::Element&) const {
        return nullptr;
    }

    void AlarmActionsMenu::BeginPan(const PanState&) {
        auto* panel = this->FindElement("alarmActionsPanel");
        this->panStartHeight = panel->Height();
        auto* host = this->FindElement("alarmActionsHost");
        for (auto& group : host->VisualStateGroups()) {
            if (group.name == "AlarmActionsPanelStates") {
                // A short or cancelled drag must be able to return to the same state.
                group.currentState.clear();
            }
        }
    }

    void AlarmActionsMenu::UpdatePan(const PanState& state) {
        const float collapsed = this->StateValue(
            "Collapsed", "alarmActionsPanel", xaml::AnimatedProperty::height);
        const float expanded = this->StateValue(
            "Expanded", "alarmActionsPanel", xaml::AnimatedProperty::height);
        const float height = this->panStartHeight + state.downY - state.currentY;
        this->SetDragProgress(std::clamp((height - collapsed) / (expanded - collapsed), 0.0f, 1.0f));
    }

    bool AlarmActionsMenu::EndPan(const PanState& state, xaml::AnimationController&) {
        this->UpdatePan(state);
        const float collapsed = this->StateValue(
            "Collapsed", "alarmActionsPanel", xaml::AnimatedProperty::height);
        const float expanded = this->StateValue(
            "Expanded", "alarmActionsPanel", xaml::AnimatedProperty::height);
        const float distance = state.downY - state.currentY;
        const float threshold = std::min(48.0f, (expanded - collapsed) * 0.5f);
        const bool visible = distance >= threshold ? true
            : distance <= -threshold ? false : this->isExpanded;
        if (visible == this->isExpanded) {
            this->ApplyState(true);
        } else {
            this->SetIsExpanded(visible);
        }
        return true;
    }

    void AlarmActionsMenu::CancelPan(xaml::Element&) {
        this->ApplyState(false);
    }

    void AlarmActionsMenu::UpdateGestures(xaml::Element&, xaml::AnimationController&) {
    }

    bool AlarmActionsMenu::IsIn(const xaml::Element& pageRoot) const {
        return _details::MenuContains(pageRoot, *this);
    }

    bool AlarmActionsMenu::Owns(const xaml::Element& element) const {
        return &element == this->FindElement("alarmActionsMenu");
    }


#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view AlarmActionsMenu::RuntimeClassName() const {
        return "mobileclock::ui::controls::AlarmActionsMenu";
    }

    bool AlarmActionsMenu::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) {
        try {
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(templateNode.children.at(0), context,
                {this->Bounds().width, this->Bounds().height});
            if (!xaml::VisualStateManager::GoToState(*result.root, "AlarmActionsPanelStates",
                this->isExpanded ? "Expanded" : "Collapsed", false)) {
                throw std::invalid_argument("Alarm actions menu visual state was not found");
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
#endif

    //
    // API
    //
    bool AlarmActionsMenu::IsExpanded() const {
        return this->isExpanded;
    }

    void AlarmActionsMenu::SetIsExpanded(bool value) {
        this->isExpanded = value;
        this->ApplyState(true);
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    void AlarmActionsMenu::PreserveState(const xaml::Element& previous, xaml::Element& replacement) {
        if (auto* menu = dynamic_cast<AlarmActionsMenu*>(&replacement)) {
            const auto restore = [menu](auto&& self, const xaml::Element& node) -> bool {
                const auto* oldMenu = dynamic_cast<const AlarmActionsMenu*>(&node);
                if (oldMenu != nullptr && oldMenu->Id() == menu->Id()) {
                    menu->isExpanded = oldMenu->isExpanded;
                    menu->ApplyState(false);
                    return true;
                }
                for (const auto& child : node.Children()) {
                    if (self(self, *child)) {
                        return true;
                    }
                }
                return false;
            };
            restore(restore, previous);
        }
        for (const auto& child : replacement.Children()) {
            PreserveState(previous, *child);
        }
    }
#endif

    //
    // Internal
    //
    xaml::Element* AlarmActionsMenu::FindElement(std::string_view id) const {
        return this->Content() == nullptr ? nullptr : _details::FindMenuElement(*this->Content(), id);
    }

    void AlarmActionsMenu::ApplyState(bool useTransitions) {
        auto* host = this->FindElement("alarmActionsHost");
        if (host == nullptr || !xaml::VisualStateManager::GoToState(*host, "AlarmActionsPanelStates",
            this->isExpanded ? "Expanded" : "Collapsed", useTransitions)) {
            throw std::invalid_argument("Alarm actions menu visual state was not found");
        }
    }

    float AlarmActionsMenu::StateValue(
        const char* stateName, const char* target, xaml::AnimatedProperty property) const {
        auto* host = this->FindElement("alarmActionsHost");
        if (host != nullptr) {
            for (const auto& group : host->VisualStateGroups()) {
                if (group.name != "AlarmActionsPanelStates") {
                    continue;
                }
                for (const auto& state : group.states) {
                    if (state.name != stateName) {
                        continue;
                    }
                    for (const auto& track : state.tracks) {
                        if (track.targetName == target && track.animation.property == property) {
                            return track.animation.to;
                        }
                    }
                }
            }
        }
        return 0.0f;
    }

    void AlarmActionsMenu::SetDragProgress(float progress) {
        xaml::AnimationController animations;
        for (const char* id : {"alarmActionsPanel", "alarmActionsMenu", "alarmActionsContent"}) {
            auto* element = this->FindElement(id);
            if (element == nullptr) {
                continue;
            }
            const auto property = element->Id() == "alarmActionsContent"
                ? xaml::AnimatedProperty::opacity : xaml::AnimatedProperty::height;
            const float collapsed = this->StateValue("Collapsed", id, property);
            const float expanded = this->StateValue("Expanded", id, property);
            const float value = collapsed + (expanded - collapsed) * progress;
            // Replacing the property track also stops a previous settling animation.
            animations.Animate(*element, property, value, value, std::chrono::milliseconds(0));
        }
    }
}