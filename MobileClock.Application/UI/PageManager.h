#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "UI/Pages/StatisticsPageViewModel.h"
#include "UI/Pages/XiaomiThemesPageViewModel.h"
#include "UI/Pages/AddAlarmPageViewModel.h"
#include "UI/Pages/SettingsPageViewModel.h"
#include "UI/Pages/MainPageViewModel.h"
#include "UI/InputDispatcher.h"
#include "UI/PageRegistry.h"

#include <span>
#include <string_view>
#include <string>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class PageManager final : public IPageNavigator {
    public:
        explicit PageManager(IApplicationActions& actions);
        ~PageManager() = default;

        PageManager(const PageManager&) = delete;
        PageManager& operator=(const PageManager&) = delete;

        //
        // IPageNavigator
        //
        bool Navigate(std::string_view pageName) override;

        std::string_view CurrentPageName() const;
        bool IsTransitioning() const;
        void Initialize(xaml::Size availableSize);
        void Resize(xaml::Size availableSize);
        void SetAnimationPlaybackRate(float value);
        void SetStatus(std::string value);
        void AddAlarmMelody(std::string name, std::string uri);
        void SetAlarmMelody(std::string name, std::string uri);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool NavigatePreviewRoute(std::string_view target, std::string& error);
        bool NavigatePreviewRoute(std::span<const std::string_view> path, std::string& error);
        std::string PreviewRouteGraph() const;
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
            MainPageViewModel,
            SettingsPageViewModel,
            StatisticsPageViewModel,
            AddAlarmPageViewModel,
            XiaomiThemesPageViewModel>;

        struct PreviewRoute final {
            std::string_view source;
            std::string_view target;
            void (*trigger)(ApplicationPages& pages);
        };

        template <typename TSource, typename TTarget, void (TSource::*TTrigger)()>
        static PreviewRoute MakePreviewRoute();

        static std::span<const PreviewRoute> PreviewRoutes();
        bool ExecutePreviewRoute(std::span<const PreviewRoute*> route, std::string& error);

    private:
        xaml::Size availableSize;
        PageContext pageContext;
        ApplicationPages pages;
        IPage* currentPage = nullptr;
        IPage* outgoingPage = nullptr;
        bool isTransitioning = false;
        bool preserveAddAlarmDraft = false;
        xaml::AnimationController animations;
        InputDispatcher inputDispatcher;
    };
}