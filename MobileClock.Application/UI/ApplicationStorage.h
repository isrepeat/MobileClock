#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mobileclock::ui {
    struct AlarmMelody final {
        std::string name;
        std::string uri;

        bool operator==(const AlarmMelody&) const = default;
    };

    struct ApplicationStorageData final {
        std::vector<AlarmMelody> alarmMelodies;

        bool operator==(const ApplicationStorageData&) const = default;
    };

    enum class StorageAction {
        add,
        remove,
        update,
    };

    struct StoragePath final {
        std::vector<std::string> segments;
    };

    struct StorageChange final {
        StorageAction action;
        StoragePath path;
        std::optional<AlarmMelody> previousValue;
        std::optional<AlarmMelody> value;
    };

    class ApplicationStorage final {
    public:
        class EditTransaction final {
        public:
            explicit EditTransaction(ApplicationStorage& storage);
            ~EditTransaction() = default;

            EditTransaction(const EditTransaction&) = delete;
            EditTransaction& operator=(const EditTransaction&) = delete;

            ApplicationStorageData* operator->();
            bool Commit();

        private:
            ApplicationStorage& storage;
            ApplicationStorageData previousData;
            bool committed = false;
        };

        using ChangeHandler = std::function<void(const StorageChange&)>;
        using Persistence = std::function<bool(const ApplicationStorageData&)>;
        using Unsubscribe = std::function<void()>;

        explicit ApplicationStorage(ApplicationStorageData data = {}, Persistence persistence = {});
        ~ApplicationStorage() = default;

        ApplicationStorage(const ApplicationStorage&) = delete;
        ApplicationStorage& operator=(const ApplicationStorage&) = delete;

        const ApplicationStorageData* operator->() const;
        EditTransaction Edit();
        Unsubscribe Subscribe(ChangeHandler handler);

    private:
        bool Commit(const ApplicationStorageData& previousData);
        void NotifyChanges(const ApplicationStorageData& previousData);

    private:
        ApplicationStorageData data;
        Persistence persistence;
        std::vector<ChangeHandler> changeHandlers;
    };
}