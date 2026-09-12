#include "UI/AppSessionController.h"

#include <utility>

namespace mobileclock::ui {
    AppSessionController::AppSessionController(ApplicationStorage& storage)
        : storage(storage)
        , session(*this, storage) {
    }

    //
    // API
    //
    void AppSessionController::Dispatch(AppSessionSignal signal, AppSessionSignalData data) {
        switch (signal) {
        case AppSessionSignal::requestAlarmMelody:
        case AppSessionSignal::toggleAlarm:
        case AppSessionSignal::updateApplication:
        case AppSessionSignal::uploadScreenshot:
        case AppSessionSignal::shareLogs:
        case AppSessionSignal::exportLogs:
            this->Emit(signal, data);
            return;
        case AppSessionSignal::resetAlarmMelodySelection: {
            auto edit = this->storage.Edit();
            edit->alarmMelodies.clear();
            if (edit.Commit()) {
                this->Emit(signal, data);
            }
            return;
        }
        case AppSessionSignal::restoreAlarmMelody:
            this->session.AddAlarmMelody(std::move(data.value), std::move(data.additionalValue));
            return;
        case AppSessionSignal::alarmMelodySelected:
            this->session.SetAlarmMelody(std::move(data.value), std::move(data.additionalValue));
            return;
        case AppSessionSignal::setStatus:
            this->session.SetStatus(std::move(data.value));
            return;
        }
    }

    void AppSessionController::SetHostEventHandler(HostEventHandler handler) {
        this->hostEventHandler = std::move(handler);
    }

    ApplicationSession& AppSessionController::Session() {
        return this->session;
    }

    const ApplicationSession& AppSessionController::Session() const {
        return this->session;
    }

    //
    // Internal
    //
    void AppSessionController::Emit(AppSessionSignal signal, const AppSessionSignalData& data) const {
        if (this->hostEventHandler) {
            this->hostEventHandler(signal, data);
        }
    }
}