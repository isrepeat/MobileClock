#pragma once
#include <string_view>
#include <memory>

namespace mobileclock::ui {
    enum class NavigationTrigger {
        createAlarm,
        navigateToSettings,
        navigateToMain,
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        chooseAlarmMelody,
        applySelectedMelody,
        cancelMelodySelection,
#endif
    };

    struct NavigationRequest final {
        std::string_view source;
        std::string_view target;
        NavigationTrigger trigger;
    };

    class NavigationState {
    public:
        virtual ~NavigationState() = default;
    };

    class INavigationPage {
    public:
        virtual ~INavigationPage() = default;
        virtual std::unique_ptr<NavigationState> OnNavigatingFrom(const NavigationRequest& request) = 0;
        virtual bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<NavigationState> state) = 0;
    };

    class IPageNavigator {
    public:
        virtual ~IPageNavigator() = default;

        virtual bool Navigate(std::string_view pageName) = 0;
        virtual bool Trigger(NavigationTrigger trigger) = 0;

        template <typename TPage>
        bool Navigate() {
            return this->Navigate(TPage::PageName);
        }
    };
}