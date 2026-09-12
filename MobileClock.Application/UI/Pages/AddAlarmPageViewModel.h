#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>
#include <XamlRuntime/ObservableCollection.h>

#include "MobileClock.UI/Controls/IGestureTarget.h"
#include "UI/ISerializable.h"
#include "UI/PageRegistry.h"

#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class AddAlarmPageViewModel final : public ISerializable, public IGestureTarget, public INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "AddAlarmPage";
        inline static constexpr std::string_view PreviewGraphTitle = "Новый будильник";

        class Melody final {
        public:
            Melody(std::string name, std::string uri, xaml::Element::Command selectCommand);

            const std::string& Name() const;
            const std::string& Uri() const;
            xaml::Element::Command SelectCommand() const;

            void SetName(std::string value);

        private:
            std::string name;
            std::string uri;
            xaml::Element::Command selectCommand;
        };

        explicit AddAlarmPageViewModel(PageContext& context);
        ~AddAlarmPageViewModel() override = default;

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // ISerializable
        //
        bool Deserialize(std::string_view json, std::string& error) override;
#endif

        //
        // IGestureTarget
        //
        bool CanHandlePan(const xaml::Element& element) const override;
        bool IsVerticalPan() const override;
        xaml::Element* FindScrollViewer(const xaml::Element& element) const override;
        void BeginPan(const PanState& state) override;
        void UpdatePan(const PanState& state) override;
        bool EndPan(const PanState& state, xaml::AnimationController& animations) override;
        void CancelPan(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

        //
        // INavigationPage
        //
        std::unique_ptr<NavigationState> OnNavigatingFrom(const NavigationRequest& request) override;
        bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<NavigationState> state) override;

        void Reset();
        const AlarmSettings& Settings() const;
        void AddMelody(std::string name, std::string uri);
        void SetMelody(std::string name, std::string uri);
        const xaml::ObservableCollection<Melody>& Melodies() const;
        void ChooseAlarmMelody();
        void NavigateToMain();
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        xaml::runtime::RuntimeBindingContext RuntimeContext();
        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        //
        // IGestureTarget
        //
        bool IsIn(const xaml::Element& pageRoot) const override;
        bool Owns(const xaml::Element& element) const override;

        void ConnectControls();
        void OnStorageChange(const StorageChange& change);
        void RemoveMelody(std::string_view uri);
        void Refresh();
        void RefreshWheel(int column, float offset);
        void ChangeTime(int column, int steps);
        int WheelColumn(const xaml::Element& element) const;
        xaml::Element* Find(std::string_view id) const;

    private:
        PageContext& context;
        AlarmSettings settings;
        xaml::ObservableCollection<Melody> melodies;
        ApplicationStorage::Unsubscribe storageSubscription;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
        int activeColumn = 0;
        int startValue = 0;
        float rowHeight = 64.0f;
        float remainder = 0.0f;
        bool dragging = false;
        bool saved = false;
    };
}