#include "UI/ApplicationStorage.h"

#include <algorithm>
#include <utility>

namespace mobileclock::ui {
    ApplicationStorage::EditTransaction::EditTransaction(ApplicationStorage& storage)
        : storage(storage)
        , previousData(storage.data) {
    }

    //
    // API
    //
    ApplicationStorageData* ApplicationStorage::EditTransaction::operator->() {
        return &this->storage.data;
    }

    bool ApplicationStorage::EditTransaction::Commit() {
        if (this->committed) {
            return false;
        }
        this->committed = true;
        return this->storage.Commit(this->previousData);
    }

    ApplicationStorage::ApplicationStorage(ApplicationStorageData data, Persistence persistence)
        : data(std::move(data))
        , persistence(std::move(persistence)) {
    }

    //
    // API
    //
    const ApplicationStorageData* ApplicationStorage::operator->() const {
        return &this->data;
    }

    ApplicationStorage::EditTransaction ApplicationStorage::Edit() {
        return EditTransaction(*this);
    }

    ApplicationStorage::Unsubscribe ApplicationStorage::Subscribe(ChangeHandler handler) {
        this->changeHandlers.push_back(std::move(handler));
        const size_t index = this->changeHandlers.size() - 1;
        return [this, index]() {
            this->changeHandlers[index] = nullptr;
        };
    }

    //
    // Internal
    //
    bool ApplicationStorage::Commit(const ApplicationStorageData& previousData) {
        if (this->data == previousData) {
            return true;
        }
        if (this->persistence && !this->persistence(this->data)) {
            this->data = previousData;
            return false;
        }
        this->NotifyChanges(previousData);
        return true;
    }

    void ApplicationStorage::NotifyChanges(const ApplicationStorageData& previousData) {
        std::vector<StorageChange> changes;
        for (const AlarmMelody& previous : previousData.alarmMelodies) {
            const auto current = std::find_if(this->data.alarmMelodies.begin(), this->data.alarmMelodies.end(), [&previous](const AlarmMelody& value) {
                return value.uri == previous.uri;
            });
            if (current == this->data.alarmMelodies.end()) {
                changes.push_back({StorageAction::remove, {{"alarmMelodies", previous.uri}}, previous, {}});
            } else if (*current != previous) {
                changes.push_back({StorageAction::update, {{"alarmMelodies", previous.uri}}, previous, *current});
            }
        }
        for (const AlarmMelody& current : this->data.alarmMelodies) {
            const auto previous = std::find_if(previousData.alarmMelodies.begin(), previousData.alarmMelodies.end(), [&current](const AlarmMelody& value) {
                return value.uri == current.uri;
            });
            if (previous == previousData.alarmMelodies.end()) {
                changes.push_back({StorageAction::add, {{"alarmMelodies", current.uri}}, {}, current});
            }
        }
        const std::vector<ChangeHandler> handlers = this->changeHandlers;
        for (const StorageChange& change : changes) {
            for (const ChangeHandler& handler : handlers) {
                if (handler) {
                    handler(change);
                }
            }
        }
    }
}