// src/application/privacy_anonymizer.hpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace loggen::application {

enum class PrivacyTokenKind : std::uint8_t {
    None,
    Person,
    Store,
    UserId,
    EmployeeId,
    Department,
    Organization,
    Email,
    Phone,
    Address,
    IpAddress,
    MacAddress,
    Host,
    Identifier,
    Secret,
    FilePath
};

inline constexpr std::array privacy_token_kinds{
    PrivacyTokenKind::Person,
    PrivacyTokenKind::Store,
    PrivacyTokenKind::UserId,
    PrivacyTokenKind::EmployeeId,
    PrivacyTokenKind::Department,
    PrivacyTokenKind::Organization,
    PrivacyTokenKind::Email,
    PrivacyTokenKind::Phone,
    PrivacyTokenKind::Address,
    PrivacyTokenKind::IpAddress,
    PrivacyTokenKind::MacAddress,
    PrivacyTokenKind::Host,
    PrivacyTokenKind::Identifier,
    PrivacyTokenKind::Secret,
    PrivacyTokenKind::FilePath,
};

[[nodiscard]] constexpr std::uint32_t privacy_token_bit(const PrivacyTokenKind kind) noexcept {
    const auto value = static_cast<std::uint8_t>(kind);
    return value == 0 || value > privacy_token_kinds.size() ? 0U : 1U << (value - 1U);
}

class PrivacyAnonymizer {
public:
    static constexpr std::size_t synthetic_profile_count{50};
    using SyntheticProfileValues = std::array<std::string, privacy_token_kinds.size()>;
    [[nodiscard]] static std::string sanitize(std::string_view sample);
    [[nodiscard]] static PrivacyTokenKind classify_field(std::string_view field_name);
    [[nodiscard]] static std::string_view marker(PrivacyTokenKind kind) noexcept;
    [[nodiscard]] static PrivacyTokenKind marker_kind(std::string_view value) noexcept;
    [[nodiscard]] static const SyntheticProfileValues& synthetic_profile(std::size_t profile_index);
    [[nodiscard]] static std::string_view synthetic_value(PrivacyTokenKind kind, std::size_t profile_index);
    [[nodiscard]] static std::string_view search_terms(PrivacyTokenKind kind) noexcept;
};

}
