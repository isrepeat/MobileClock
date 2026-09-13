#pragma once
#include <functional>
#include <memory>

namespace mobileclock::ui {
    struct ApplicationStateDocument;

    class ApplicationStateStore final {
    public:
        using Persistence = std::function<bool(const ApplicationStateDocument&)>;

        explicit ApplicationStateStore(ApplicationStateDocument document, Persistence persistence = {});
        ~ApplicationStateStore();

        ApplicationStateStore(const ApplicationStateStore&) = delete;
        ApplicationStateStore& operator=(const ApplicationStateStore&) = delete;

        const ApplicationStateDocument& State() const;
        bool Write(ApplicationStateDocument candidate);

    private:
        std::unique_ptr<ApplicationStateDocument> document;
        Persistence persistence;
    };
}