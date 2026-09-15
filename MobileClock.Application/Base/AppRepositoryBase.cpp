#include "AppRepositoryBase.h"

#include "../Model/AlarmRepository.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>

namespace mobileclock::application::base {
    AppRepositoryBase::AppRepositoryBase(core::ApplicationStateStore& store)
        : store(store) {
    }

    const model::ApplicationStateDocument& AppRepositoryBase::State() const {
        return this->store.CurrentDocument();
    }

    bool AppRepositoryBase::Commit(model::ApplicationStateDocument document) {
        return this->store.TrySaveDocument(std::move(document));
    }

    std::string AppRepositoryBase::CreateAlarmId() {
        static std::atomic_uint64_t sequence;
        return std::format("alarm-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(), ++sequence);
    }

    std::string AppRepositoryBase::CreateMelodyId() {
        static std::atomic_uint64_t sequence;
        return std::format("melody-{}-{}", std::chrono::steady_clock::now().time_since_epoch().count(), ++sequence);
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    void AppRepositoryBase::LoadPreviewScenarioState(model::ApplicationStateDocument document) {
        this->store.LoadPreviewSessionDocument(std::move(document));
    }

    bool AppRepositoryBase::SavePreviewStateToPersistentStorage() {
        model::ApplicationStateDocument document = this->store.CurrentDocument();
        std::vector<std::pair<std::string, std::string>> melodyIds;
        for (model::AlarmMelody& melody : document.alarmMelodies) {
            if (melody.id.starts_with("preview-")) {
                melodyIds.emplace_back(melody.id, this->CreateMelodyId());
                melody.id = melodyIds.back().second;
            }
        }
        for (model::Alarm& alarm : document.alarms) {
            if (alarm.id.starts_with("preview-")) {
                alarm.id = this->CreateAlarmId();
            }
            const auto melody = std::find_if(melodyIds.begin(), melodyIds.end(), [&alarm](const auto& value) {
                return value.first == alarm.melodyId;
            });
            if (melody != melodyIds.end()) {
                alarm.melodyId = melody->second;
            }
        }
        this->store.LoadPreviewSessionDocument(std::move(document));
        return this->store.SavePreviewSessionDocumentToPersistentStorage();
    }
#endif
}