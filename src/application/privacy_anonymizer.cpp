// src/application/privacy_anonymizer.cpp
#include "application/privacy_anonymizer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>

namespace loggen::application {
namespace {

constexpr std::size_t profile_slot(const PrivacyTokenKind kind) noexcept {
    return static_cast<std::size_t>(kind) - 1;
}

using SyntheticProfiles = std::array<PrivacyAnonymizer::SyntheticProfileValues, PrivacyAnonymizer::synthetic_profile_count>;

std::unique_ptr<SyntheticProfiles> make_profiles() {
    auto result = std::make_unique<SyntheticProfiles>();
    for (std::size_t index = 0; index < result->size(); ++index) {
        const auto number = static_cast<unsigned int>(index + 1);
        char buffer[96]{};
        auto& profile = (*result)[index];
        std::snprintf(buffer, sizeof(buffer), "홍길동 %u", number);
        profile[profile_slot(PrivacyTokenKind::Person)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "%u호점", number);
        profile[profile_slot(PrivacyTokenKind::Store)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "user%u", number);
        profile[profile_slot(PrivacyTokenKind::UserId)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "EMP%04u", number);
        profile[profile_slot(PrivacyTokenKind::EmployeeId)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "테스트부서 %u", number);
        profile[profile_slot(PrivacyTokenKind::Department)] = buffer;
        profile[profile_slot(PrivacyTokenKind::Organization)] = "Your-Company";
        std::snprintf(buffer, sizeof(buffer), "user%u@example.invalid", number);
        profile[profile_slot(PrivacyTokenKind::Email)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "010-0000-%04u", number);
        profile[profile_slot(PrivacyTokenKind::Phone)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "서울특별시 테스트로 %u", number);
        profile[profile_slot(PrivacyTokenKind::Address)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "198.51.100.%u", number);
        profile[profile_slot(PrivacyTokenKind::IpAddress)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "02:00:00:00:00:%02X", number);
        profile[profile_slot(PrivacyTokenKind::MacAddress)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "host-%u", number);
        profile[profile_slot(PrivacyTokenKind::Host)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "id-%04u", number);
        profile[profile_slot(PrivacyTokenKind::Identifier)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "secret-%04u", number);
        profile[profile_slot(PrivacyTokenKind::Secret)] = buffer;
        std::snprintf(buffer, sizeof(buffer), "C:\\Test\\file-%u.log", number);
        profile[profile_slot(PrivacyTokenKind::FilePath)] = buffer;
    }
    return result;
}

const SyntheticProfiles& profiles() {
    static const auto value = make_profiles();
    return *value;
}

std::string normalize_field(const std::string_view field_name) {
    std::string result;
    result.reserve(field_name.size());
    for (const auto character : field_name) {
        const auto value = static_cast<unsigned char>(character);
        if (character == '-' || character == '.') {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(std::tolower(value)));
        }
    }
    return result;
}

bool contains_any(const std::string_view value, const std::initializer_list<std::string_view> candidates) {
    return std::ranges::any_of(candidates, [value](const std::string_view candidate) {
        return value.find(candidate) != std::string_view::npos;
    });
}

bool equals_any(const std::string_view value, const std::initializer_list<std::string_view> candidates) {
    return std::ranges::find(candidates, value) != candidates.end();
}

char ascii_lower(const char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool matches_ascii_case_insensitive(const std::string_view value, const std::size_t position, const std::string_view needle) noexcept {
    if (position + needle.size() > value.size()) {
        return false;
    }
    for (std::size_t index = 0; index < needle.size(); ++index) {
        if (ascii_lower(value[position + index]) != needle[index]) {
            return false;
        }
    }
    return true;
}

std::string replace_company_terms(const std::string_view value) {
    std::string result;
    result.reserve(value.size());
    std::size_t position = 0;
    while (position < value.size()) {
        if (ascii_lower(value[position]) == 'l') {
            const bool lottermart = matches_ascii_case_insensitive(value, position, "lottermart");
            if (lottermart || matches_ascii_case_insensitive(value, position, "lottemart")) {
                result.append("Your-Company");
                position += lottermart ? 10 : 9;
                continue;
            }
            if (matches_ascii_case_insensitive(value, position, "lotte")) {
                result.append("Your");
                position += 5;
                continue;
            }
        } else if (ascii_lower(value[position]) == 'm' && matches_ascii_case_insensitive(value, position, "mart")) {
            result.append("company");
            position += 4;
            continue;
        }
        result.push_back(value[position]);
        ++position;
    }
    return result;
}

const std::regex& assigned_field_pattern() {
    static const std::regex pattern(
        R"privacy(((?:^|[\s,;|{])["']?([A-Za-z_][A-Za-z0-9_.-]{0,63})["']?\s*[:=]\s*)(?:"([^"]*)"|'([^']*)'|([^\s,;|}\]]+)))privacy",
        std::regex::ECMAScript | std::regex::optimize);
    return pattern;
}

std::string replace_sensitive_fields(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    std::size_t cursor = 0;
    const std::string_view view{input};
    for (std::sregex_iterator iterator(input.begin(), input.end(), assigned_field_pattern()), end; iterator != end; ++iterator) {
        const auto& match = *iterator;
        const auto field_begin = static_cast<std::size_t>(match[2].first - input.begin());
        const auto kind = PrivacyAnonymizer::classify_field(view.substr(field_begin, static_cast<std::size_t>(match[2].length())));
        if (kind == PrivacyTokenKind::None) {
            continue;
        }
        std::size_t value_group = 5;
        if (match[3].matched) {
            value_group = 3;
        } else if (match[4].matched) {
            value_group = 4;
        }
        const auto value_begin = static_cast<std::size_t>(match[value_group].first - input.begin());
        const auto current_value = view.substr(value_begin, static_cast<std::size_t>(match[value_group].length()));
        if (current_value.find("{{") != std::string::npos || current_value.find("}}") != std::string::npos) {
            continue;
        }
        const auto begin = static_cast<std::size_t>(match[value_group].first - input.begin());
        const auto finish = static_cast<std::size_t>(match[value_group].second - input.begin());
        if (begin < cursor) {
            continue;
        }
        output.append(input, cursor, begin - cursor);
        output.append(PrivacyAnonymizer::marker(kind));
        cursor = finish;
    }
    output.append(input, cursor, std::string::npos);
    return output;
}

std::string replace_pattern(const std::string& input, const std::regex& pattern, const std::string_view marker) {
    std::string output;
    output.reserve(input.size());
    std::size_t cursor = 0;
    for (std::sregex_iterator iterator(input.begin(), input.end(), pattern), end; iterator != end; ++iterator) {
        const auto& match = *iterator;
        const auto begin = static_cast<std::size_t>(match[0].first - input.begin());
        const auto finish = static_cast<std::size_t>(match[0].second - input.begin());
        if (begin < cursor) {
            continue;
        }
        output.append(input, cursor, begin - cursor);
        output.append(marker);
        cursor = finish;
    }
    output.append(input, cursor, std::string::npos);
    return output;
}

}

std::string PrivacyAnonymizer::sanitize(const std::string_view sample) {
    auto result = replace_company_terms(sample);
    result = replace_sensitive_fields(result);

    static const std::regex email_pattern(R"(\b[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+\b)", std::regex::ECMAScript | std::regex::optimize);
    static const std::regex phone_pattern(R"(\b(?:\+?82[- ]?)?0?1[016789][- ]?\d{3,4}[- ]?\d{4}\b)", std::regex::ECMAScript | std::regex::optimize);
    static const std::regex resident_pattern(R"(\b\d{6}[- ]?[1-8]\d{6}\b)", std::regex::ECMAScript | std::regex::optimize);
    static const std::regex mac_pattern(R"(\b(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b)", std::regex::ECMAScript | std::regex::optimize);
    static const std::regex user_path_pattern(R"((\b[A-Za-z]:\\Users\\)[^\\\s]+)", std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
    result = replace_pattern(result, email_pattern, marker(PrivacyTokenKind::Email));
    result = replace_pattern(result, phone_pattern, marker(PrivacyTokenKind::Phone));
    result = replace_pattern(result, resident_pattern, marker(PrivacyTokenKind::Identifier));
    result = replace_pattern(result, mac_pattern, marker(PrivacyTokenKind::MacAddress));

    std::string path_output;
    path_output.reserve(result.size());
    std::size_t cursor = 0;
    for (std::sregex_iterator iterator(result.begin(), result.end(), user_path_pattern), end; iterator != end; ++iterator) {
        const auto& match = *iterator;
        const auto begin = static_cast<std::size_t>(match[0].first - result.begin());
        const auto finish = static_cast<std::size_t>(match[0].second - result.begin());
        path_output.append(result, cursor, begin - cursor);
        const auto prefix_begin = static_cast<std::size_t>(match[1].first - result.begin());
        path_output.append(result, prefix_begin, static_cast<std::size_t>(match[1].length()));
        path_output.append("TestUser");
        cursor = finish;
    }
    path_output.append(result, cursor, std::string::npos);
    return path_output;
}

PrivacyTokenKind PrivacyAnonymizer::classify_field(const std::string_view field_name) {
    const auto field = normalize_field(field_name);
    if (equals_any(field, {"src", "srcip", "src_ip", "srp_ip", "srcaddr", "src_addr", "srcaddress", "src_address", "sourceip", "source_ip", "sourceaddress", "source_address", "clientip", "client_ip", "clientipaddr", "sip", "dst", "dstip", "dst_ip", "dest_ip", "dstnip", "dstn_ip", "dstaddr", "dst_addr", "dstaddress", "dst_address", "destinationip", "destination_ip", "destinationaddress", "destination_address", "serverip", "server_ip", "dip"})) {
        return PrivacyTokenKind::None;
    }
    if (contains_any(field, {"store", "branch", "shop", "site_name", "site_nm", "site_num", "str_nm", "str_cd", "bizpl"})) {
        return PrivacyTokenKind::Store;
    }
    if (field == "name" || contains_any(field, {"user_name", "user_nm", "emp_name", "emp_nm", "employee_name", "person_name", "customer_name", "customer_nm", "cust_name", "cust_nm", "member_name", "member_nm", "client_name", "manager_name", "manager_nm", "mgr_name", "mgr_nm", "operator_name", "admin_name", "requester_name", "approver_name", "owner_name", "suser_name", "duser_name", "first_name", "firstname", "last_name", "lastname", "full_name", "fullname", "given_name", "family_name", "srcnm", "dstnm"})) {
        return PrivacyTokenKind::Person;
    }
    if (contains_any(field, {"email", "e_mail", "mail_addr", "mail_address"})) {
        return PrivacyTokenKind::Email;
    }
    if (contains_any(field, {"phone", "mobile", "cellphone", "cell_phone", "telephone", "tel_no", "telnum", "fax"})) {
        return PrivacyTokenKind::Phone;
    }
    if (contains_any(field, {"address", "postal", "postcode", "zip_code", "zipcode"}) || (field.find("addr") != std::string::npos && field.find("ip") == std::string::npos)) {
        return PrivacyTokenKind::Address;
    }
    if (field == "mac" || field.starts_with("mac_") || field.ends_with("_mac")) {
        return PrivacyTokenKind::MacAddress;
    }
    if (field == "ip" || field.ends_with("_ip") || field.starts_with("ip_") || contains_any(field, {"clientip", "agentip", "web_ip", "access_ip", "remote_ip", "local_ip"})) {
        return PrivacyTokenKind::IpAddress;
    }
    if (contains_any(field, {"password", "passwd", "pwd", "secret", "token", "api_key", "access_key", "private_key", "credential"})) {
        return PrivacyTokenKind::Secret;
    }
    if (contains_any(field, {"dept", "team", "division", "department", "groupname", "group_name"})) {
        return PrivacyTokenKind::Department;
    }
    if (contains_any(field, {"company", "corporate", "organization", "org_name", "org_nm", "com_name"})) {
        return PrivacyTokenKind::Organization;
    }
    if (equals_any(field, {"suser", "duser"}) || contains_any(field, {"username", "user_id", "userid", "login_id", "loginid", "account", "suser_id", "duser_id", "chakra_user_id", "manager_id", "mgr_id", "decide_mgr_id"})) {
        return PrivacyTokenKind::UserId;
    }
    if (contains_any(field, {"emp_id", "employee_id", "empno", "emp_no", "employee_no"})) {
        return PrivacyTokenKind::EmployeeId;
    }
    if (equals_any(field, {"host", "server_nm", "eqp_nm", "sensor_nm", "instance_nm", "dstn_host_nm", "src_host_nm"}) || contains_any(field, {"host_name", "hostname", "host_alias", "device_id", "deviceid", "terminal_id", "terminal_name", "term_name", "serial"})) {
        return PrivacyTokenKind::Host;
    }
    if (field == "path" || contains_any(field, {"file_path", "filepath", "filename", "file_name", "origin_filename", "install_location", "application_path", "directory", "folder"})) {
        return PrivacyTokenKind::FilePath;
    }
    if (contains_any(field, {"resident", "rrn", "ssn", "passport", "card_no", "card_number", "customer_id", "customer_no", "cust_id", "cust_no", "member_id", "member_no", "birth", "birthday", "date_of_birth", "dob", "vehicle_no", "car_no", "license_no", "uuid", "guid", "session_id", "sessionid", "user_num", "client_num", "dept_num", "personal_id", "user_info", "user_key_id"}) || field == "_uid" || field == "uid") {
        return PrivacyTokenKind::Identifier;
    }
    return PrivacyTokenKind::None;
}

std::string_view PrivacyAnonymizer::marker(const PrivacyTokenKind kind) noexcept {
    switch (kind) {
    case PrivacyTokenKind::Person:
        return "{{PERSON}}";
    case PrivacyTokenKind::Store:
        return "{{STORE}}";
    case PrivacyTokenKind::UserId:
        return "{{USER_ID}}";
    case PrivacyTokenKind::EmployeeId:
        return "{{EMPLOYEE_ID}}";
    case PrivacyTokenKind::Department:
        return "{{DEPARTMENT}}";
    case PrivacyTokenKind::Organization:
        return "{{ORGANIZATION}}";
    case PrivacyTokenKind::Email:
        return "{{EMAIL}}";
    case PrivacyTokenKind::Phone:
        return "{{PHONE}}";
    case PrivacyTokenKind::Address:
        return "{{ADDRESS}}";
    case PrivacyTokenKind::IpAddress:
        return "{{IP_ADDRESS}}";
    case PrivacyTokenKind::MacAddress:
        return "{{MAC_ADDRESS}}";
    case PrivacyTokenKind::Host:
        return "{{HOST}}";
    case PrivacyTokenKind::Identifier:
        return "{{IDENTIFIER}}";
    case PrivacyTokenKind::Secret:
        return "{{SECRET}}";
    case PrivacyTokenKind::FilePath:
        return "{{FILE_PATH}}";
    case PrivacyTokenKind::None:
        return {};
    }
    return {};
}

PrivacyTokenKind PrivacyAnonymizer::marker_kind(const std::string_view value) noexcept {
    for (const auto kind : privacy_token_kinds) {
        if (value == marker(kind)) {
            return kind;
        }
    }
    return PrivacyTokenKind::None;
}

std::string_view PrivacyAnonymizer::synthetic_value(const PrivacyTokenKind kind, const std::size_t profile_index) {
    if (kind == PrivacyTokenKind::None) {
        return {};
    }
    if (static_cast<std::size_t>(kind) > privacy_token_kinds.size()) {
        throw std::invalid_argument("Invalid privacy token kind");
    }
    return synthetic_profile(profile_index)[profile_slot(kind)];
}

const PrivacyAnonymizer::SyntheticProfileValues& PrivacyAnonymizer::synthetic_profile(const std::size_t profile_index) {
    const auto normalized_index = profile_index < synthetic_profile_count ? profile_index : profile_index % synthetic_profile_count;
    return profiles()[normalized_index];
}

std::string_view PrivacyAnonymizer::search_terms(const PrivacyTokenKind kind) noexcept {
    switch (kind) {
    case PrivacyTokenKind::Person:
        return "person name user_name user_nm emp_nm 성명 이름 사람";
    case PrivacyTokenKind::Store:
        return "store branch shop site str_nm 점포 지점 매장 호점";
    case PrivacyTokenKind::UserId:
        return "user account login id 계정 사용자 아이디";
    case PrivacyTokenKind::EmployeeId:
        return "employee emp employee_id emp_no 사번 임직원";
    case PrivacyTokenKind::Department:
        return "department dept team group 부서 조직 팀 파트";
    case PrivacyTokenKind::Organization:
        return "organization company corporate org 회사 법인";
    case PrivacyTokenKind::Email:
        return "email e-mail mail 이메일 메일";
    case PrivacyTokenKind::Phone:
        return "phone mobile telephone tel 전화 휴대폰 연락처";
    case PrivacyTokenKind::Address:
        return "address postal zip 주소 우편번호";
    case PrivacyTokenKind::IpAddress:
        return "ip address network 아이피 네트워크";
    case PrivacyTokenKind::MacAddress:
        return "mac address hardware 맥주소 하드웨어";
    case PrivacyTokenKind::Host:
        return "host hostname device terminal server 호스트 장비 단말 서버";
    case PrivacyTokenKind::Identifier:
        return "identifier uuid guid session 개인식별 식별자 세션";
    case PrivacyTokenKind::Secret:
        return "secret password token key credential 비밀번호 비밀 인증";
    case PrivacyTokenKind::FilePath:
        return "file path filename directory 파일 경로 폴더";
    case PrivacyTokenKind::None:
        return {};
    }
    return {};
}

}
