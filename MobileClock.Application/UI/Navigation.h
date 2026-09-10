#pragma once
#include <string_view>

namespace mobileclock::ui {
    class IPageNavigator {
    public:
        virtual ~IPageNavigator() = default;

        virtual bool Navigate(std::string_view pageName) = 0;

        template <typename TPage>
        bool Navigate() {
            return this->Navigate(TPage::PageName);
        }
    };
}