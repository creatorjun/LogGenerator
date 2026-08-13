// src/domain/log_template.hpp
#pragma once

#include <map>
#include <string>
#include <vector>

namespace loggen::domain {

struct LogTestCase {
    std::map<std::string, std::vector<std::string>, std::less<>> values;
};

struct LogTemplate {
    std::string id;
    std::string name;
    std::string sample;
    std::string source;
    LogTestCase test_case;
};

}
