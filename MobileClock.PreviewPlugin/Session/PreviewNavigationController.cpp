#include "PreviewNavigationController.h"

#include "MobileClock.Application/Core/ApplicationSession.h"

#include <algorithm>
#include <format>
#include <vector>

namespace mobileclock::preview::session {
    PreviewNavigationController::PreviewNavigationController(application::core::ApplicationSession& session)
        : session(session) {
    }

    std::string PreviewNavigationController::BuildGraphJson() const {
        const std::vector routes = this->session.PreviewRoutes();
        std::vector<std::string_view> pages;
        for (const auto& route : routes) {
            if (std::find(pages.begin(), pages.end(), route.source) == pages.end()) {
                pages.push_back(route.source);
            }
            if (std::find(pages.begin(), pages.end(), route.target) == pages.end()) {
                pages.push_back(route.target);
            }
        }
        std::string json = std::format(
            "{{\"currentPageId\":\"{}\",\"layoutRootPageId\":\"MainPage\",\"pages\":[",
            this->session.CurrentPageName());
        bool first = true;
        for (const std::string_view page : pages) {
            if (!first) {
                json += ',';
            }
            json += std::format(
                "{{\"id\":\"{}\",\"title\":\"{}\"}}",
                page,
                this->session.PreviewPageTitle(page));
            first = false;
        }
        json += "],\"transitions\":[";
        first = true;
        for (const auto& route : routes) {
            if (!first) {
                json += ',';
            }
            // Для previousPage target уже разрешён native-слоем по текущей истории, но тип
            // сохраняем в JSON, чтобы previewer не считал это статическим вторым маршрутом.
            const std::string_view targetKind = route.targetKind == application::core::NavigationTargetKind::previousPage
                ? "previousPage"
                : "page";
            json += std::format(
                "{{\"id\":\"{}\",\"sourcePageId\":\"{}\",\"targetPageId\":\"{}\",\"targetKind\":\"{}\",\"title\":\"{}\",\"isDefault\":{}}}",
                route.id,
                route.source,
                route.target,
                targetKind,
                route.title,
                route.isDefault ? "true" : "false");
            first = false;
        }
        json += "]}";
        return json;
    }

    bool PreviewNavigationController::Navigate(
        std::span<const std::string_view> transitionIds,
        std::string& error) const {
        return this->session.NavigatePreviewTransitions(transitionIds, error);
    }
}