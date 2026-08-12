// src/application/ports/identifier_generator.hpp
#pragma once

#include <string>
#include <string_view>

namespace loggen::application {

class IIdentifierGenerator {
public:
    virtual ~IIdentifierGenerator() = default;
    [[nodiscard]] virtual std::string next(std::string_view prefix) = 0;
};

}
