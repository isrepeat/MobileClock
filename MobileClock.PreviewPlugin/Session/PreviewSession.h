#pragma once
#include "MobileClock.Application/Core/AppSessionController.h"

#include <string_view>
#include <memory>
#include <string>
#include <vector>

namespace mobileclock::preview::session {
    class PreviewNavigationController;

    //
    // Владеет состоянием одного запуска preview и скрывает детали persistent state
    // от C ABI плагина.
    //
    class PreviewSession final {
    public:
        PreviewSession(int width, int height);
        ~PreviewSession();

        PreviewSession(const PreviewSession&) = delete;
        PreviewSession& operator=(const PreviewSession&) = delete;

        mobileclock::application::core::ApplicationSession& Session();
        mobileclock::application::core::AppSessionController& Controller();
        PreviewNavigationController& Navigation();

        bool ExportState();
        bool CanSaveState() const;
        void Resize(int width, int height);

        static std::vector<std::string> ParseNavigationTransitionIds(std::string_view json);

    private:
        class State;

    private:
        std::unique_ptr<State> state;
    };
}