// src/domain/log_limits.hpp
#pragma once

#include <cstddef>

namespace loggen::domain {

inline constexpr std::size_t maximum_log_template_count = 10'000;
inline constexpr std::size_t maximum_log_identifier_bytes = 512;
inline constexpr std::size_t maximum_log_name_bytes = 4'096;
inline constexpr std::size_t maximum_log_sample_bytes = 4U * 1024U * 1024U;
inline constexpr std::size_t maximum_catalog_file_bytes = 64U * 1024U * 1024U;

}
