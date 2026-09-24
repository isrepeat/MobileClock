#pragma once
#include <string_view>
#include <string>

namespace mobileclock::application::interface {
    class ISerializable {
    public:
        virtual ~ISerializable() = default;

        virtual std::string Serialize() const = 0;
        virtual bool Deserialize(std::string_view json, std::string& error) = 0;
    };
}