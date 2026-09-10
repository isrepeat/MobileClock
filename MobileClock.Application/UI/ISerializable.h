#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <string>
#include <string_view>
#endif
namespace mobileclock::ui {
    class ISerializable {
    public:
        virtual ~ISerializable() = default;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        virtual bool Deserialize(std::string_view json, std::string& error) = 0;
#endif
    };
}