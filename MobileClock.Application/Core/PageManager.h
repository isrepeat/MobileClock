#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "MobileClock.Presentation/Core/PageTransition.h"
#if defined(ANDROID_APP_PREVIEWER)
#include "../UI/Page/preview_XiaomiThemesPageViewModel.h"
#endif
#include "../UI/Page/SettingsPageViewModel.h"
#include "../UI/Page/AddAlarmPageViewModel.h"
#include "../UI/Page/MainPageViewModel.h"
#include "InputDispatcher.h"
#include "PageRegistry.h"

#include <string_view>
#include <memory>
#include <string>
#include <vector>
#include <span>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::core {
    //
    // PageRegistry является единственным владельцем страниц. Менеджер хранит raw pointers
    // только как наблюдающие ссылки: адреса страниц неизменны до уничтожения PageManager.
    //
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
        bool Trigger(NavigationTrigger trigger, std::unique_ptr<base::NavigationStateBase> state) override;
        bool NavigateBack(std::unique_ptr<base::NavigationStateBase> result = {}) override;

        std::string_view CurrentPageName() const;
        bool IsTransitioning() const;
        void Initialize(xaml::Size availableSize);
        void Resize(xaml::Size availableSize);
        void SetAnimationPlaybackRate(float value);
        void SetStatus(std::string value);
        void AddAlarmMelody(model::AlarmMelody alarmMelody);
        void SetAlarmMelody(model::AlarmMelody alarmMelody);
#if defined(ANDROID_APP_PREVIEWER)
        struct preview_Route final {
            std::string_view id;
            std::string_view source;
            std::string_view target;
            std::string_view backwardOfRouteId;
            std::string_view title;
            bool isDefault;
            NavigationTargetKind targetKind;
            std::string_view dataType;
            std::string previewDefault;
        };

        bool preview_NavigateRoute(std::string_view target, std::string& error);
        bool preview_NavigateTransitions(std::span<const std::string_view> transitionIds, std::string& error);
        bool preview_NavigateRoute(std::span<const std::string_view> path, std::string& error);
        std::string preview_RouteGraph() const;
        std::vector<preview_Route> preview_Routes() const;
        std::string_view preview_PageTitle(std::string_view pageName) const;
        bool preview_ApplyScenario(std::string_view page, std::string_view json, std::string& error);
        bool preview_ReloadMarkup(std::string_view page, std::string_view markup, std::string_view sourcePath, std::string& diagnostics);
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
#if defined(ANDROID_APP_PREVIEWER)
            ,
            ui::page::preview_XiaomiThemesPageViewModel>;
#else
            >;
#endif

        struct NavigationRoute final {
            std::string_view id;
            std::string_view source;
            NavigationTrigger trigger;
            std::string_view target;
            NavigationTargetKind targetKind;
            const NavigationDataContract* dataContract;
            mobileclock::presentation::core::NavigationDirection direction;
            std::string_view title;
            bool isDefault;
        };

        struct NavigationHistoryEntry final {
            interface::IPage* page;
            // Невладеющая ссылка на ребро Routes(), по которому открыта page.
            // У корневой страницы входящего ребра нет.
            const NavigationRoute* incomingRoute;
        };

        template <
            typename TSource,
            typename TTarget,
            typename TData,
            NavigationTrigger TTrigger,
            mobileclock::presentation::core::NavigationDirection TDirection
        >
        static NavigationRoute MakeRoute(std::string_view id, std::string_view title, bool isDefault = true);

        template <
            typename TSource,
            typename TData,
            NavigationTrigger TTrigger,
            mobileclock::presentation::core::NavigationDirection TDirection
        >
        static NavigationRoute MakeBackRoute(std::string_view id, std::string_view title, bool isDefault = true);

        static std::span<const NavigationRoute> Routes();
        // previousPage не задаётся в декларации маршрута: его цель зависит от фактической
        // истории переходов, а не от статического графа приложения.
        std::string_view ResolveTarget(const NavigationRoute& navigationRoute) const;
        bool Navigate(const NavigationRoute& navigationRoute, std::unique_ptr<base::NavigationStateBase> state);
        bool IsNavigationDataValid(const NavigationRoute& navigationRoute, const base::NavigationStateBase* state) const;
        bool SwitchPage(interface::IPage& page, mobileclock::presentation::core::NavigationDirection direction);
        static void SetNavigationVisualStates(
            interface::IPage* outgoingPage,
            interface::IPage& currentPage,
            mobileclock::presentation::core::NavigationDirection direction);
#if defined(ANDROID_APP_PREVIEWER)
        bool preview_ExecuteRoute(std::span<const NavigationRoute*> navigationRoutes, std::string& error);
#endif

    private:
        xaml::Size availableSize;
        PageContext pageContext;
        ApplicationPages pages;
        interface::IPage* currentPage = nullptr;
        interface::IPage* outgoingPage = nullptr;
        // Вектор хранит фактический стек переходов: последний элемент всегда совпадает
        // с currentPage. Входящее ребро нужно для точной визуализации обратного пути,
        // когда между одной и той же парой страниц есть несколько маршрутов.
        std::vector<NavigationHistoryEntry> navigationHistory;
        mobileclock::presentation::core::NavigationDirection navigationDirection = mobileclock::presentation::core::NavigationDirection::forward;
        bool isTransitioning = false;
        xaml::AnimationController animationController;
        InputDispatcher inputDispatcher;
    };
}