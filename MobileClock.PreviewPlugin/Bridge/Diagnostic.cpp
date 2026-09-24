#include "Diagnostic.h"

namespace mobileclock::preview::bridge {
    std::string& LastError() {
        static thread_local std::string lastError;
        return lastError;
    }
}