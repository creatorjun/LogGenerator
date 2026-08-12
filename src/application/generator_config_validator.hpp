// src/application/generator_config_validator.hpp
#pragma once

#include "domain/generator_config.hpp"

namespace loggen::application {

[[nodiscard]] domain::GeneratorConfig validate_and_normalize(domain::GeneratorConfig config);

}
