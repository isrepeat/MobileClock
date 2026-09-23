#include "ApplicationStateStore.h"

#include "../Model/AlarmRepository.h"

#include <utility>

namespace mobileclock::application::core {
    ApplicationStateStore::ApplicationStateStore(model::ApplicationStateDocument document, DocumentSaveHandler documentSaveHandler)
        : document(std::make_unique<model::ApplicationStateDocument>(std::move(document)))
        , documentSaveHandler(std::move(documentSaveHandler)) {
    }

    ApplicationStateStore::~ApplicationStateStore() = default;

    //
    // API
    //
    const model::ApplicationStateDocument& ApplicationStateStore::CurrentDocument() const {
        return *this->document;
    }

    bool ApplicationStateStore::TrySaveDocument(model::ApplicationStateDocument candidate) {
        // Обычный документ сначала записывается через handler; при ошибке текущее
        // состояние в памяти не меняется и UI не видит несохранённые данные.
#if defined(ANDROID_APP_PREVIEWER)
        // Документ сценария живёт только в памяти preview-сеанса. Изменения UI
        // применяются к нему, но не затрагивают постоянный storage до экспорта.
        if (!this->isUsingPreviewSessionDocument && this->documentSaveHandler && !this->documentSaveHandler(candidate)) {
#else
        if (this->documentSaveHandler && !this->documentSaveHandler(candidate)) {
#endif
            return false;
        }
        *this->document = std::move(candidate);
        return true;
    }

#if defined(ANDROID_APP_PREVIEWER)
    void ApplicationStateStore::preview_LoadSessionDocument(model::ApplicationStateDocument candidate) {
        // Замена полного документа переводит storage в memory-only режим preview.
        *this->document = std::move(candidate);
        this->isUsingPreviewSessionDocument = true;
    }

    bool ApplicationStateStore::preview_SaveSessionDocumentToPersistentStorage() {
        // Повторный экспорт обычного документа не требуется.
        if (!this->isUsingPreviewSessionDocument) {
            return true;
        }
        // Только успешная запись завершает preview-режим; иначе пользователь может
        // повторить экспорт, не теряя изменений текущего сеанса.
        if (this->documentSaveHandler && !this->documentSaveHandler(*this->document)) {
            return false;
        }
        this->isUsingPreviewSessionDocument = false;
        return true;
    }
#endif
}