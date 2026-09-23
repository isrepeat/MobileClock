#include "AlarmRepository.h"

#include <algorithm>
namespace mobileclock::application::model {
    AlarmRepository::AlarmRepository(core::ApplicationStateStore& store)
        : base::AppRepositoryBase(store)
        , alarms(this->State().alarms) {
    }


#if defined(ANDROID_APP_PREVIEWER)
    //
    // AppRepositoryBase
    //
    void AlarmRepository::preview_ReloadFromStateStore() {
        this->alarms = this->State().alarms;
    }

    bool AlarmRepository::preview_IsSessionDocumentEquivalentTo(const ApplicationStateDocument& document) const {
        const ApplicationStateDocument& current = this->State();
        if (current.alarms.size() != document.alarms.size() || current.alarmMelodies.size() != document.alarmMelodies.size()) {
            return false;
        }
        const auto melodyKey = [](const ApplicationStateDocument& value, std::string_view id) {
            const auto item = std::find_if(value.alarmMelodies.begin(), value.alarmMelodies.end(), [id](const AlarmMelody& melody) {
                return melody.id == id;
            });
            return item == value.alarmMelodies.end() ? std::pair<std::string, std::string>{} : std::pair{item->name, item->uri};
        };
        for (size_t index = 0; index < current.alarmMelodies.size(); ++index) {
            if (current.alarmMelodies[index].name != document.alarmMelodies[index].name
                || current.alarmMelodies[index].uri != document.alarmMelodies[index].uri) {
                return false;
            }
        }
        for (size_t index = 0; index < current.alarms.size(); ++index) {
            const Alarm& left = current.alarms[index];
            const Alarm& right = document.alarms[index];
            if (left.hour != right.hour || left.minute != right.minute || left.days != right.days
                || left.vibration != right.vibration || left.isEnabled != right.isEnabled
                || melodyKey(current, left.melodyId) != melodyKey(document, right.melodyId)) {
                return false;
            }
        }
        return true;
    }
#endif

    //
    // API
    //
    const std::vector<Alarm>& AlarmRepository::Alarms() const {
        return this->alarms;
    }

    bool AlarmRepository::CreateAlarm(const Alarm& alarm) {
        std::vector<Alarm> candidate = this->alarms;
        Alarm value = alarm;
        value.id = this->CreateAlarmId();
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
        ApplicationStateDocument document = this->State();
        document.alarms = candidate;
        if (!base::AppRepositoryBase::Commit(std::move(document))) {
            return false;
        }
        this->alarms = std::move(candidate);
        return true;
    }

    #if defined(ANDROID_APP_PREVIEWER)
    //
    // AppRepositoryBase
    //
    void AlarmMelodyRepository::preview_ReloadFromStateStore() {
        this->melodies = this->State().alarmMelodies;
        this->Notify();
    }

    bool AlarmMelodyRepository::preview_IsSessionDocumentEquivalentTo(const ApplicationStateDocument& document) const {
        return this->melodies == document.alarmMelodies;
    }
#endif

    AlarmMelodyRepository::AlarmMelodyRepository(core::ApplicationStateStore& store)
        : base::AppRepositoryBase(store)
        , melodies(this->State().alarmMelodies) {
    }

    const std::vector<AlarmMelody>& AlarmMelodyRepository::Melodies() const {
        return this->melodies;
    }

    bool AlarmMelodyRepository::SaveMelody(AlarmMelody& value) {
        std::vector<AlarmMelody> candidate = this->melodies;
        if (value.id.empty()) {
            value.id = this->CreateMelodyId();
        }
        auto item = std::find_if(candidate.begin(), candidate.end(), [&value](const AlarmMelody& melody) { return melody.id == value.id; });
#if defined(ANDROID_APP_PREVIEWER)
        // Каталог тем может передать известный URI с другим id. В preview-сеансе
        // сохраняем id записи из хранилища, чтобы не создать дубликат мелодии
        // и не разорвать уже существующие Alarm::melodyId.
        if (item == candidate.end()) {
            item = std::find_if(candidate.begin(), candidate.end(), [&value](const AlarmMelody& melody) { return melody.uri == value.uri; });
            if (item != candidate.end()) {
                value.id = item->id;
            }
        }
#endif
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
        ApplicationStateDocument document = this->State();
        document.alarmMelodies = candidate;
        if (!base::AppRepositoryBase::Commit(std::move(document))) {
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