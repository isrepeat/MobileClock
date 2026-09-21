#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "MobileClock.Presentation/Core/PageTransition.h"
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "../UI/Page/XiaomiThemesPageViewModel.h"
#endif
#include "../UI/Page/SettingsPageViewModel.h"
#include "../UI/Page/AddAlarmPageViewModel.h"
#include "../UI/Page/MainPageViewModel.h"
#include "InputDispatcher.h"
#include "PageRegistry.h"

#include <string_view>
#include <string>
#include <span>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::core {
    class PageManager final : public interface::IPageNavigator {
    public:
        PageManager(AppSessionController& appSessionController, model::AlarmRepository& alarmRepository, model::AlarmMelodyRepository& alarmMelodyRepository);
        ~PageManager() = default;

        PageManager(const PageManager&) = delete;
        PageManager& operator=(const PageManager&) = delete;

        //
        // IPageNavigator
        //
        bool Navigate(std::string_view pageName) override;
        bool Trigger(NavigationTrigger trigger) override;

        std::string_view CurrentPageName() const;
        bool IsTransitioning() const;
        void Initialize(xaml::Size availableSize);
        void Resize(xaml::Size availableSize);
        void SetAnimationPlaybackRate(float value);
        void SetStatus(std::string value);
        void AddAlarmMelody(model::AlarmMelody alarmMelody);
        void SetAlarmMelody(model::AlarmMelody alarmMelody);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        struct PreviewRoute final {
            std::string_view id;
            std::string_view source;
            std::string_view target;
            std::string_view title;
            bool isDefault;
        };

        bool NavigatePreviewRoute(std::string_view target, std::string& error);
        bool NavigatePreviewTransitions(std::span<const std::string_view> transitionIds, std::string& error);
        bool NavigatePreviewRoute(std::span<const std::string_view> path, std::string& error);
        std::string PreviewRouteGraph() const;
        std::vector<PreviewRoute> PreviewRoutes() const;
        std::string_view PreviewPageTitle(std::string_view pageName) const;
        bool ApplyPreviewScenario(std::string_view page, std::string_view json, std::string& error);
        bool ReloadMarkup(std::string_view page, std::string_view markup, std::string_view sourcePath, std::string& diagnostics);
#endif
        void HandleTouchDown(float x, float y);
        bool HandleTouchMove(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        xaml::Element& Root();
        void UpdateClock();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;

    private:
        using ApplicationPages = PageRegistry<
            ui::page::MainPageViewModel,
            ui::page::SettingsPageViewModel,
            ui::page::AddAlarmPageViewModel
#if defined(MOBILECLOCK_XAML_PREVIEWER)
            ,
            ui::page::XiaomiThemesPageViewModel>;
#else
            >;
#endif

        struct NavigationRoute final {
            std::string_view id;
            std::string_view source;
            NavigationTrigger trigger;
            std::string_view target;
            mobileclock::presentation::core::NavigationDirection direction;
            std::string_view title;
            bool isDefault;
        };

        template <typename TSource, typename TTarget, NavigationTrigger TTrigger,
            mobileclock::presentation::core::NavigationDirection TDirection>
        static NavigationRoute MakeRoute(std::string_view id, std::string_view title, bool isDefault = true);

        static std::span<const NavigationRoute> Routes();
        bool Navigate(std::string_view pageName, mobileclock::presentation::core::NavigationDirection direction);
        static void SetNavigationVisualStates(
            interface::IPage* outgoing,
            interface::IPage& current,
            mobileclock::presentation::core::NavigationDirection direction);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool ExecutePreviewRoute(std::span<const NavigationRoute*> route, std::string& error);
#endif

    private:
        xaml::Size availableSize;
        PageContext pageContext;
        ApplicationPages pages;
        interface::IPage* currentPage = nullptr;
        interface::IPage* outgoingPage = nullptr;
        mobileclock::presentation::core::NavigationDirection navigationDirection = mobileclock::presentation::core::NavigationDirection::forward;
        bool isTransitioning = false;
        xaml::AnimationController animations;
        InputDispatcher inputDispatcher;
    };
}