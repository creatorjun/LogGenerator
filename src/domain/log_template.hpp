// src/domain/log_template.hpp
#pragma once

#include <string>

namespace loggen::domain {

struct LogTemplate {
    std::string id;
    std::string name;
    std::string sample;
    std::string source;
};

}
