#pragma once
#include "UI/ApplicationSession.h"

#include <functional>
#include <string>

namespace mobileclock::ui {
    enum class AppSessionSignal {
        requestAlarmMelody,
        resetAlarmMelodySelection,
        toggleAlarm,
        updateApplication,
        uploadScreenshot,
        shareLogs,
        exportLogs,
        restoreAlarmMelody,
        alarmMelodySelected,
        setStatus,
    };

    struct AppSessionSignalData final {
        std::string value;
        std::string additionalValue;
    };

    class AppSessionController final {
    public:
        using HostEventHandler = std::function<void(AppSessionSignal, const AppSessionSignalData&)>;

        explicit AppSessionController(ApplicationStorage& storage);
        ~AppSessionController() = default;

        AppSessionController(const AppSessionController&) = delete;
        AppSessionController& operator=(const AppSessionController&) = delete;

        void Dispatch(AppSessionSignal signal, AppSessionSignalData data);
        void SetHostEventHandler(HostEventHandler handler);
        ApplicationSession& Session();
        const ApplicationSession& Session() const;

    private:
        void Emit(AppSessionSignal signal, const AppSessionSignalData& data) const;

    private:
        ApplicationStorage& storage;
        ApplicationSession session;
        HostEventHandler hostEventHandler;
    };
}