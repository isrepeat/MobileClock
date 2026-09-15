#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/ObservableCollection.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "MobileClock.UI/Interface/IGestureTarget.h"
#include "../../Interface/INavigationPage.h"
#include "../../Interface/ISerializable.h"
#include "../../Core/PageRegistry.h"
#include "../ViewModel/AlarmMelodyViewModel.h"

#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::ui::page {
    class AddAlarmPageViewModel final : public interface::ISerializable, public mobileclock::ui::interface::IGestureTarget, public interface::INavigationPage {
    public:
        inline static constexpr std::string_view PageName = "AddAlarmPage";
        inline static constexpr std::string_view PreviewGraphTitle = "Новый будильник";

        explicit AddAlarmPageViewModel(core::PageContext& context);
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
        xaml::Element* FindScrollViewer(const xaml::Element& element) const override;
        mobileclock::ui::interface::GestureHandling ResolveGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state, mobileclock::ui::interface::GestureDirection direction) const override;
        void BeginGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state) override;
        void UpdateGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state) override;
        bool EndGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state, xaml::AnimationController& animations) override;
        void CancelGesture(xaml::Element& element) override;
        void UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) override;

        //
        // INavigationPage
        //
        std::unique_ptr<base::NavigationStateBase> OnNavigatingFrom(const core::NavigationRequest& request) override;
        bool OnNavigatingTo(const core::NavigationRequest& request, std::unique_ptr<base::NavigationStateBase> state) override;

        void Reset();
        const model::Alarm& Settings() const;
        void AddMelody(model::AlarmMelody alarmMelody);
        void SetMelody(model::AlarmMelody alarmMelody);
        void DeleteMelody(std::string_view uri);
        const xaml::ObservableCollection<view_model::AlarmMelodyViewModel>& Melodies() const;
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
        void OnRepositoryChange();
        void RemoveMelody(std::string_view uri);
        void Refresh();
        void RefreshSaveButton();
        void RefreshWheel(int column, float offset);
        void ChangeTime(int column, int steps);
        void MarkChanged();
        int WheelColumn(const xaml::Element& element) const;
        xaml::Element* Find(std::string_view id) const;

    private:
        core::PageContext& context;
        model::Alarm settings;
        model::Alarm initialSettings;
        xaml::ObservableCollection<view_model::AlarmMelodyViewModel> melodies;
        model::AlarmMelodyRepository::Unsubscribe storageSubscription;
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
        bool isEditing = false;
        bool hasChanges = false;
        std::string editingAlarmId;
    };
}