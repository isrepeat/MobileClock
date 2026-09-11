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
    AlarmActionsMenu::~AlarmActionsMenu() {
        if (this->parentUnsubscribe) {
            this->parentUnsubscribe();
        }
    }

    void AlarmActionsMenu::OnInitialized() {
        this->RegisterGestureTarget();
        this->ApplyState(false);
    }

    //
    // IGestureTarget
    //
    bool AlarmActionsMenu::CanHandlePan(const xaml::Element& element) const {
        return this->isExpanded && this->Owns(element);
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
        if (distance <= -threshold) {
            this->SetIsExpanded(false);
        } else {
            this->ApplyState(true);
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
            auto bindings = std::make_shared<xaml::runtime::RuntimeBindingRegistry>(context.bindings);
            bindings->AddCommand("OpenMenuCommand", this->OpenMenuCommand());
            auto controlContext = context;
            controlContext.bindings = std::move(bindings);
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(templateNode.children.at(0), controlContext,
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
            this->ApplyState(false);
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
    const std::string& AlarmActionsMenu::Status() const {
        return this->status;
    }

    xaml::Element::Command AlarmActionsMenu::OpenMenuCommand() const {
        return this->openMenuCommand;
    }

    xaml::Element::Command AlarmActionsMenu::UpdateApplicationCommand() const {
        return this->updateApplicationCommand;
    }

    xaml::Element::Command AlarmActionsMenu::UploadScreenshotCommand() const {
        return this->uploadScreenshotCommand;
    }

    bool AlarmActionsMenu::IsExpanded() const {
        return this->isExpanded;
    }

    void AlarmActionsMenu::SetIsExpanded(bool value) {
        this->isExpanded = value;
        this->ApplyState(true);
    }

    AlarmActionsMenu::Unsubscribe AlarmActionsMenu::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
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

    void AlarmActionsMenu::SetStatus(std::string value) {
        if (this->status == value) {
            return;
        }
        this->status = std::move(value);
        this->NotifyPropertyChanged(Property::status);
    }

    void AlarmActionsMenu::NotifyPropertyChanged(Property property) {
        for (const PropertyChangedHandler& handler : this->propertyChangedHandlers) {
            if (handler) {
                handler(property);
            }
        }
    }

    void AlarmActionsMenu::ApplyState(bool useTransitions) {
        auto* host = this->FindElement("alarmActionsHost");
        auto* panel = this->FindElement("alarmActionsPanel");
        if (panel != nullptr) {
            // The collapsed panel is positioned above the bottom by its margin.
            // While expanding, the same distance becomes a render offset so the
            // lower edge reaches the screen bottom while the explicit height grows up.
            xaml::AnimationController animations;
            animations.Animate(
                *panel,
                xaml::AnimatedProperty::renderOffsetY,
                panel->RenderOffsetY(),
                this->isExpanded ? panel->Margin().bottom : 0.0f,
                useTransitions ? std::chrono::milliseconds(500) : std::chrono::milliseconds(0));
        }
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
        if (auto* panel = this->FindElement("alarmActionsPanel")) {
            animations.Animate(
                *panel,
                xaml::AnimatedProperty::renderOffsetY,
                panel->Margin().bottom * progress,
                panel->Margin().bottom * progress,
                std::chrono::milliseconds(0));
        }
    }
}