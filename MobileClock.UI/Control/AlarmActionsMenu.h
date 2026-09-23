#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/IRuntimeReloadableControl.h>
#endif
#include <XamlRuntime/UserControl.h>

#include "!Generated/MobileClock.UI/Xaml/Control/AlarmActionsMenu.xaml.h"
#include "../Interface/IGestureTarget.h"

#include <string_view>
#include <functional>
#include <utility>
#include <string>
#include <vector>
#include <memory>

namespace mobileclock::ui::control {
    class AlarmActionsMenu final : public xaml::UserControl, public interface::IGestureTarget
#if defined(ANDROID_APP_PREVIEWER)
        , public xaml::runtime::IRuntimeReloadableControl
#endif
    {
    public:
        enum class Property {
            status,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        AlarmActionsMenu() = default;
        ~AlarmActionsMenu() override;

    private:
        //
        // UserControl
        //
        void OnInitialized() override;

    public:
        //
        // IGestureTarget
        //
        xaml::Element* FindScrollViewer(const xaml::Element& element) const override;
        interface::GestureHandling ResolveGesture(const interface::IGestureTarget::PanState& state, interface::GestureDirection direction) const override;
        void BeginGesture(const interface::IGestureTarget::PanState& state) override;
        void UpdateGesture(const interface::IGestureTarget::PanState& state) override;
        bool EndGesture(const interface::IGestureTarget::PanState& state, xaml::AnimationController& animations) override;
        void CancelGesture(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

    private:
        bool IsIn(const xaml::Element& pageRoot) const override;
        bool Owns(const xaml::Element& element) const override;

    public:
#if defined(ANDROID_APP_PREVIEWER)
        //
        // IRuntimeReloadableControl
        //
        std::string_view RuntimeClassName() const override;
        bool ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
            const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) override;
#endif

        template<typename TViewModel>
        static std::unique_ptr<AlarmActionsMenu> Create(TViewModel& viewModel, xaml::BindingScope&) {
            auto control = std::make_unique<AlarmActionsMenu>();
            control->status = viewModel.Status();
            control->updateApplicationCommand = viewModel.UpdateApplicationCommand();
            control->uploadScreenshotCommand = viewModel.UploadScreenshotCommand();
            control->toggleMenuCommand = [menu = control.get()]() {
                menu->SetIsExpanded(!menu->isExpanded);
            };
            control->parentUnsubscribe = viewModel.Subscribe([menu = control.get(), &viewModel](auto) {
                menu->SetStatus(viewModel.Status());
            });
            auto bindings = std::make_unique<xaml::BindingScope>();
            auto content = xaml::generated::AlarmActionsMenuXaml::BuildContent(*control, *bindings);
            control->InitializeComponent(
                std::move(content), std::move(bindings));
            return control;
        }

        const std::string& Status() const;
        xaml::Element::Command ToggleMenuCommand() const;
        xaml::Element::Command UpdateApplicationCommand() const;
        xaml::Element::Command UploadScreenshotCommand() const;
        bool IsExpanded() const;
        void SetIsExpanded(bool value);
        Unsubscribe Subscribe(PropertyChangedHandler handler);
#if defined(ANDROID_APP_PREVIEWER)
        static void preview_PreserveState(const xaml::Element& previous, xaml::Element& replacement);
#endif

    private:
        xaml::Element* FindElement(std::string_view id) const;
        void SetStatus(std::string value);
        void NotifyPropertyChanged(Property property);
        void ApplyState(bool useTransitions);
        float StateValue(const char* state, const char* target, xaml::AnimatedProperty property) const;
        void SetDragProgress(float progress);

    private:
        bool isExpanded = false;
        float panStartHeight = 0.0f;
        std::string status;
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        Unsubscribe parentUnsubscribe;
        xaml::Element::Command toggleMenuCommand;
        xaml::Element::Command updateApplicationCommand;
        xaml::Element::Command uploadScreenshotCommand;
    };
}