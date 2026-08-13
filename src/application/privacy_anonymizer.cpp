// src/application/privacy_anonymizer.cpp
#include "application/privacy_anonymizer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <regex>
#include <string>

namespace loggen::application {
namespace {

struct SyntheticProfile {
    std::string person;
    std::string store;
    std::string store_code;
    std::string user_id;
    std::string employee_id;
    std::string department;
    std::string organization;
    std::string email;
    std::string phone;
    std::string address;
    std::string ip_address;
    std::string mac_address;
    std::string host;
    std::string identifier;
    std::string secret;
    std::string file_path;
};

std::array<SyntheticProfile, PrivacyAnonymizer::synthetic_profile_count> make_profiles() {
    std::array<SyntheticProfile, PrivacyAnonymizer::synthetic_profile_count> result;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto number = static_cast<unsigned int>(index + 1);
        char buffer[96]{};
        auto& profile = result[index];
        std::snprintf(buffer, sizeof(buffer), "홍길동 %u", number);
        profile.person = buffer;
        std::snprintf(buffer, sizeof(buffer), "%u호점", number);
        profile.store = buffer;
        std::snprintf(buffer, sizeof(buffer), "%u", number);
        profile.store_code = buffer;
        std::snprintf(buffer, sizeof(buffer), "user%u", number);
        profile.user_id = buffer;
        std::snprintf(buffer, sizeof(buffer), "EMP%04u", number);
        profile.employee_id = buffer;
        std::snprintf(buffer, sizeof(buffer), "테스트부서 %u", number);
        profile.department = buffer;
        profile.organization = "Your-Company";
        std::snprintf(buffer, sizeof(buffer), "user%u@example.invalid", number);
        profile.email = buffer;
        std::snprintf(buffer, sizeof(buffer), "010-0000-%04u", number);
        profile.phone = buffer;
        std::snprintf(buffer, sizeof(buffer), "서울특별시 테스트로 %u", number);
        profile.address = buffer;
        std::snprintf(buffer, sizeof(buffer), "198.51.100.%u", number);
        profile.ip_address = buffer;
        std::snprintf(buffer, sizeof(buffer), "02:00:00:00:00:%02X", number);
        profile.mac_address = buffer;
        std::snprintf(buffer, sizeof(buffer), "host-%u", number);
        profile.host = buffer;
        std::snprintf(buffer, sizeof(buffer), "id-%04u", number);
        profile.identifier = buffer;
        std::snprintf(buffer, sizeof(buffer), "secret-%04u", number);
        profile.secret = buffer;
        std::snprintf(buffer, sizeof(buffer), "C:/ProgramData/Your-Company/SecurityData/event-%u.dat", number);
        profile.file_path = buffer;
    }
    return result;
}

const std::array<SyntheticProfile, PrivacyAnonymizer::synthetic_profile_count>& profiles() {
    static const auto value = make_profiles();
    return value;
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

void replace_ascii_case_insensitive(std::string& value, const std::string_view needle, const std::string_view replacement) {
    if (needle.empty() || value.size() < needle.size()) {
        return;
    }
    std::size_t position = 0;
    while (position + needle.size() <= value.size()) {
        const auto matches = std::equal(needle.begin(), needle.end(), value.begin() + static_cast<std::ptrdiff_t>(position), [](const char left, const char right) {
            return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
        });
        if (!matches) {
            ++position;
            continue;
        }
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

const std::regex& assigned_field_pattern() {
    static const std::regex pattern(
        R"privacy(((?:^|[\s,;|{])["']?([A-Za-z_][A-Za-z0-9_.-]{0,63})["']?[ \t]*[:=][ ]*)(?:"([^"]*)"|'([^']*)'|([^\s,;|}\]]+)))privacy",
        std::regex::ECMAScript | std::regex::optimize);
    return pattern;
}

const std::regex& legacy_store_code_pattern() {
    static const std::regex pattern(
        R"(((?:\b(?:store_code|branch_code|shop_code|site_num|site_code|site_cd|str_cd|bizpl_cd))\s*[:=]\s*["']?)\{\{STORE\}\})",
        std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
    return pattern;
}

std::string replace_sensitive_fields(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    std::size_t cursor = 0;
    for (std::sregex_iterator iterator(input.begin(), input.end(), assigned_field_pattern()), end; iterator != end; ++iterator) {
        const auto& match = *iterator;
        const auto kind = PrivacyAnonymizer::classify_field(match[2].str());
        if (kind == PrivacyTokenKind::None) {
            continue;
        }
        std::size_t value_group = 5;
        if (match[3].matched) {
            value_group = 3;
        } else if (match[4].matched) {
            value_group = 4;
        }
        const auto current_value = match[value_group].str();
        if (current_value.empty() || current_value.find("{{") != std::string::npos || current_value.find("}}") != std::string::npos) {
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
    std::string result{sample};
    replace_ascii_case_insensitive(result, "test123", "Your-Company");
    replace_ascii_case_insensitive(result, "lottermart", "Your-Company");
    replace_ascii_case_insensitive(result, "lottemart", "Your-Company");
    replace_ascii_case_insensitive(result, "lotte", "Your");
    replace_ascii_case_insensitive(result, "mart", "company");
    result = std::regex_replace(result, legacy_store_code_pattern(), "$1{{STORE_CODE}}");
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
        path_output.append(match[1].str());
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
    if (contains_any(field, {"store_code", "branch_code", "shop_code", "site_num", "site_code", "site_cd", "str_cd", "bizpl_cd"})) {
        return PrivacyTokenKind::StoreCode;
    }
    if (contains_any(field, {"store", "branch", "shop", "site_name", "site_nm", "str_nm", "bizpl"})) {
        return PrivacyTokenKind::Store;
    }
    if (field == "name" || contains_any(field, {"user_name", "user_nm", "emp_name", "emp_nm", "employee_name", "person_name", "customer_name", "customer_nm", "cust_name", "cust_nm", "member_name", "member_nm", "client_name", "manager_name", "manager_nm", "mgr_name", "mgr_nm", "operator_name", "admin_name", "requester_name", "approver_name", "owner_name", "suser_name", "duser_name", "first_name", "firstname", "last_name", "lastname", "full_name", "fullname", "given_name", "family_name", "srcnm", "src_nm", "dstnm", "dst_nm", "ldap_name", "sess_uname"}) || field == "ssun") {
        return PrivacyTokenKind::Person;
    }
    if (contains_any(field, {"email", "e_mail", "mail_addr", "mail_address"})) {
        return PrivacyTokenKind::Email;
    }
    if (field != "ldap_tel" && contains_any(field, {"phone", "mobile", "cellphone", "cell_phone", "telephone", "tel_no", "telnum", "fax"})) {
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
    if (contains_any(field, {"password", "passwd", "pwd", "secret", "token", "api_key", "access_key", "private_key", "credential", "hmac"}) || field == "payload" || field == "reassembled") {
        return PrivacyTokenKind::Secret;
    }
    if (contains_any(field, {"dept", "team", "division", "department", "groupname", "group_name"}) || field == "ldap_tel" || field == "ssdn") {
        return PrivacyTokenKind::Department;
    }
    if (contains_any(field, {"company", "corporate", "organization", "org_name", "org_nm", "com_name", "sess_dname", "sess_serverdname"}) || equals_any(field, {"sssdn", "ssdbn"})) {
        return PrivacyTokenKind::Organization;
    }
    if (equals_any(field, {"suser", "duser", "srcid", "src_id", "ldap_sno", "ssui", "ssda", "ssli", "tda"}) || contains_any(field, {"username", "user_id", "userid", "login_id", "loginid", "account", "suser_id", "duser_id", "chakra_user_id", "manager_id", "mgr_id", "decide_mgr_id", "sess_localid", "sess_userid", "sess_dbaccount"})) {
        return PrivacyTokenKind::UserId;
    }
    if (contains_any(field, {"emp_id", "employee_id", "empno", "emp_no", "employee_no"})) {
        return PrivacyTokenKind::EmployeeId;
    }
    if (equals_any(field, {"host", "server_nm", "eqp_nm", "sensor_nm", "instance_nm", "dstn_host_nm", "src_host_nm", "sess_domain", "sess_serveruname", "ssd", "sssun"}) || contains_any(field, {"host_name", "hostname", "host_alias", "device_id", "deviceid", "terminal_id", "terminal_name", "term_name", "serial"})) {
        return PrivacyTokenKind::Host;
    }
    if (field == "path" || contains_any(field, {"file_path", "filepath", "filename", "file_name", "origin_filename", "install_location", "application_path", "directory", "folder", "safpath"})) {
        return PrivacyTokenKind::FilePath;
    }
    if (contains_any(field, {"resident", "rrn", "ssn", "passport", "card_no", "card_number", "customer_id", "customer_no", "cust_id", "cust_no", "member_id", "member_no", "birth", "birthday", "date_of_birth", "dob", "vehicle_no", "car_no", "license_no", "uuid", "guid", "session_id", "sessionid", "dbi_session", "dbi_statement", "dbi_transaction", "user_num", "client_num", "dept_num", "personal_id", "user_info", "user_key_id", "principalid", "identifier", "rcvid", "ec2_id", "dstid", "dst_id", "srcgrpid", "src_grp_id", "dstgrpid", "dst_grp_id", "sess_did", "sess_serverdid", "sess_uid", "sess_serveruid", "txn_stmguid", "txn_sessguid"}) || equals_any(field, {"_uid", "uid", "sssd", "sssu", "ssuid", "ssdid"})) {
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
    case PrivacyTokenKind::StoreCode:
        return "{{STORE_CODE}}";
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
    const auto& profile = profiles()[profile_index % synthetic_profile_count];
    switch (kind) {
    case PrivacyTokenKind::Person:
        return profile.person;
    case PrivacyTokenKind::Store:
        return profile.store;
    case PrivacyTokenKind::StoreCode:
        return profile.store_code;
    case PrivacyTokenKind::UserId:
        return profile.user_id;
    case PrivacyTokenKind::EmployeeId:
        return profile.employee_id;
    case PrivacyTokenKind::Department:
        return profile.department;
    case PrivacyTokenKind::Organization:
        return profile.organization;
    case PrivacyTokenKind::Email:
        return profile.email;
    case PrivacyTokenKind::Phone:
        return profile.phone;
    case PrivacyTokenKind::Address:
        return profile.address;
    case PrivacyTokenKind::IpAddress:
        return profile.ip_address;
    case PrivacyTokenKind::MacAddress:
        return profile.mac_address;
    case PrivacyTokenKind::Host:
        return profile.host;
    case PrivacyTokenKind::Identifier:
        return profile.identifier;
    case PrivacyTokenKind::Secret:
        return profile.secret;
    case PrivacyTokenKind::FilePath:
        return profile.file_path;
    case PrivacyTokenKind::None:
        return {};
    }
    return {};
}

std::string_view PrivacyAnonymizer::search_terms(const PrivacyTokenKind kind) noexcept {
    switch (kind) {
    case PrivacyTokenKind::Person:
        return "person name user_name user_nm emp_nm 성명 이름 사람";
    case PrivacyTokenKind::Store:
        return "store branch shop site str_nm 점포 지점 매장 호점";
    case PrivacyTokenKind::StoreCode:
        return "store_code branch_code site_num str_cd bizpl_cd 점포코드 매장코드 지점코드";
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
