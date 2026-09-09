#pragma once

namespace mobileclock::ui {
    enum class Page {
        main,
        settings,
    };

    class IPageNavigator {
    public:
        virtual ~IPageNavigator() = default;

        virtual void Navigate(Page page) = 0;
    };
}