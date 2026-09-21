#pragma once
#include <string_view>
#include <string>
#include <span>

namespace mobileclock::application::core {
    class ApplicationSession;
}

namespace mobileclock::preview {
    // Адаптирует модель маршрутов приложения к JSON-протоколу AndroidAppPreviewer.
    class PreviewNavigationController final {
    public:
        explicit PreviewNavigationController(mobileclock::application::core::ApplicationSession& session);

        std::string BuildGraphJson() const;
        bool Navigate(std::span<const std::string_view> transitionIds, std::string& error) const;

    private:
        mobileclock::application::core::ApplicationSession& session;
    };
}