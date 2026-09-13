#include "Storage/AlarmRepository.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>

namespace mobileclock::ui::_details {
    std::string CreateRepositoryAlarmId() {
        static std::atomic_uint64_t sequence;
        return std::format(
            "alarm-{}-{}",
            std::chrono::steady_clock::now().time_since_epoch().count(),
            ++sequence);
    }

    std::string CreateRepositoryMelodyId() {
        static std::atomic_uint64_t sequence;
        return std::format(
            "melody-{}-{}",
            std::chrono::steady_clock::now().time_since_epoch().count(),
            ++sequence);
    }

} // namespace _details

namespace mobileclock::ui {
    AlarmRepository::AlarmRepository(ApplicationStateStore& store)
        : store(store)
        , alarms(this->store.State().alarms) {
    }

    //
    // API
    //
    const std::vector<Alarm>& AlarmRepository::Alarms() const {
        return this->alarms;
    }

    bool AlarmRepository::CreateAlarm(const Alarm& alarm) {
        std::vector<Alarm> candidate = this->alarms;
        Alarm value = alarm;
        value.id = _details::CreateRepositoryAlarmId();
        candidate.push_back(std::move(value));
        return this->Commit(std::move(candidate));
    }

    bool AlarmRepository::UpdateAlarm(std::string_view id, const Alarm& alarm) {
        std::vector<Alarm> candidate = this->alarms;
        const auto item = std::find_if(candidate.begin(), candidate.end(), [id](const Alarm& value) {
            return value.id == id;
        });
        if (item == candidate.end()) {
            return false;
        }
        const std::string existingId = item->id;
        *item = alarm;
        item->id = existingId;
        return this->Commit(std::move(candidate));
    }

    bool AlarmRepository::SetAlarmEnabled(std::string_view id, bool value) {
        std::vector<Alarm> candidate = this->alarms;
        const auto item = std::find_if(candidate.begin(), candidate.end(), [id](const Alarm& alarm) {
            return alarm.id == id;
        });
        if (item == candidate.end()) {
            return false;
        }
        item->isEnabled = value;
        return this->Commit(std::move(candidate));
    }

    bool AlarmRepository::RemoveAlarm(std::string_view id) {
        std::vector<Alarm> candidate = this->alarms;
        const auto item = std::remove_if(candidate.begin(), candidate.end(), [id](const Alarm& alarm) {
            return alarm.id == id;
        });
        if (item == candidate.end()) {
            return false;
        }
        candidate.erase(item, candidate.end());
        return this->Commit(std::move(candidate));
    }

    //
    // Internal
    //
    bool AlarmRepository::Commit(std::vector<Alarm> candidate) {
        ApplicationStateDocument document = this->store.State();
        document.alarms = candidate;
        if (!this->store.Write(std::move(document))) {
            return false;
        }
        this->alarms = std::move(candidate);
        return true;
    }

    AlarmMelodyRepository::AlarmMelodyRepository(ApplicationStateStore& store)
        : store(store)
        , melodies(this->store.State().alarmMelodies) {
    }

    const std::vector<AlarmMelody>& AlarmMelodyRepository::Melodies() const {
        return this->melodies;
    }

    bool AlarmMelodyRepository::SaveMelody(AlarmMelody& value) {
        std::vector<AlarmMelody> candidate = this->melodies;
        if (value.id.empty()) {
            value.id = _details::CreateRepositoryMelodyId();
        }
        const auto item = std::find_if(candidate.begin(), candidate.end(), [&value](const AlarmMelody& melody) { return melody.id == value.id; });
        if (item == candidate.end()) {
            candidate.push_back(value);
        }
        else {
            *item = value;
        }
        return this->Commit(std::move(candidate));
    }

    bool AlarmMelodyRepository::DeleteMelody(std::string_view uri) {
        std::vector<AlarmMelody> candidate = this->melodies;
        const auto item = std::remove_if(candidate.begin(), candidate.end(), [uri](const AlarmMelody& melody) { return melody.uri == uri; });
        if (item == candidate.end()) {
            return false;
        }
        candidate.erase(item, candidate.end());
        return this->Commit(std::move(candidate));
    }

    bool AlarmMelodyRepository::ClearMelodies() {
        std::vector<AlarmMelody> candidate = this->melodies;
        candidate.clear();
        return this->Commit(std::move(candidate));
    }

    AlarmMelodyRepository::Unsubscribe AlarmMelodyRepository::Subscribe(ChangeHandler handler) {
        this->handlers.push_back(std::move(handler));
        const size_t index = this->handlers.size() - 1;
        return [this, index]() { this->handlers[index] = nullptr; };
    }

    bool AlarmMelodyRepository::Commit(std::vector<AlarmMelody> candidate) {
        ApplicationStateDocument document = this->store.State();
        document.alarmMelodies = candidate;
        if (!this->store.Write(std::move(document))) {
            return false;
        }
        this->melodies = std::move(candidate);
        this->Notify();
        return true;
    }

    void AlarmMelodyRepository::Notify() {
        for (const ChangeHandler& handler : this->handlers) {
            if (handler) {
                handler();
            }
        }
    }
}