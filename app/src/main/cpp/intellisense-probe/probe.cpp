#include <memory>
#include <mutex>
#include <string>

namespace mobileclock::intellisense_probe {

std::string make_message() {
    auto value = std::make_shared<std::string>("Android libc++ is available");
    std::once_flag initialized;
    std::call_once(initialized, [] {});
    return *value;
}

} // namespace mobileclock::intellisense_probe
