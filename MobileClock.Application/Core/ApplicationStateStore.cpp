#include "ApplicationStateStore.h"

#include "../Model/AlarmRepository.h"

#include <utility>

namespace mobileclock::application::core {
    ApplicationStateStore::ApplicationStateStore(model::ApplicationStateDocument document, Persistence persistence)
        : document(std::make_unique<model::ApplicationStateDocument>(std::move(document)))
        , persistence(std::move(persistence)) {
    }

    ApplicationStateStore::~ApplicationStateStore() = default;

    //
    // API
    //
    const model::ApplicationStateDocument& ApplicationStateStore::State() const {
        return *this->document;
    }

    bool ApplicationStateStore::Write(model::ApplicationStateDocument candidate) {
        // Сначала сохраняем полный candidate на диск. Если persistence не сработала,
        // document в памяти остаётся прежним и UI не увидит несохранённое состояние.
        if (this->persistence && !this->persistence(candidate)) {
            return false;
        }
        *this->document = std::move(candidate);
        return true;
    }
}