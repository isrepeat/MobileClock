#pragma once
#include <functional>
#include <memory>

namespace mobileclock::application::model {
    struct ApplicationStateDocument;
}

namespace mobileclock::application::core {
    class ApplicationStateStore final {
    public:
        using DocumentSaveHandler = std::function<bool(const model::ApplicationStateDocument&)>;

        explicit ApplicationStateStore(model::ApplicationStateDocument document, DocumentSaveHandler documentSaveHandler = {});
        ~ApplicationStateStore();

        ApplicationStateStore(const ApplicationStateStore&) = delete;
        ApplicationStateStore& operator=(const ApplicationStateStore&) = delete;

        const model::ApplicationStateDocument& CurrentDocument() const;
        bool TrySaveDocument(model::ApplicationStateDocument candidate);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        void LoadPreviewSessionDocument(model::ApplicationStateDocument candidate);
        bool SavePreviewSessionDocumentToPersistentStorage();
#endif

    private:
        std::unique_ptr<model::ApplicationStateDocument> document;
        DocumentSaveHandler documentSaveHandler;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool isUsingPreviewSessionDocument = false;
#endif
    };
}