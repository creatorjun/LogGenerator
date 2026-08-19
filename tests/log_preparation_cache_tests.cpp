// tests/log_preparation_cache_tests.cpp
#include "test_support.hpp"

#include "application/log_preparation_cache.hpp"
#include "application/privacy_anonymizer.hpp"

#include <chrono>
#include <string>

namespace loggen::tests {

void run_log_preparation_cache_tests() {
    using namespace std::chrono;

    domain::LogTemplate raw{
        "0001",
        "All token kinds",
        R"(timestamp=2025-07-10T07:20:00Z src_ip=10.10.10.10 dst_ip=192.168.1.5 user_name="김테스트" store_name="당진점" str_cd=2201 user_id=real-user emp_no=991122 department="단품관리팀" organization=ExampleCorp email=person@example.com phone=010-1234-5678 address="서울시 테스트로 1" remote_ip=10.20.30.40 mac=AA:BB:CC:DD:EE:FF host=server.internal principalid=AIDAEXAMPLE hmac=abcdef path=/data/private/secret.log mapped={{CUSTOM_VALUE}})",
        "test",
        {}};
    raw.test_case.values["CUSTOM_VALUE"] = {"external@example.com"};

    application::LogPreparationCache cache;
    const auto tokenized = cache.tokenize_and_cache(raw);
    expect(cache.compilation_count() == 1, "Background tokenization did not precompile exactly one render blueprint");
    expect(tokenized.item.sample.find("{{TIMESTAMP:ISO8601:2025-07-10T07:20:00Z}}") != std::string::npos, "Timestamp formatting metadata was not persisted in the tokenized sample");
    expect(tokenized.item.sample.find("{{SRC_IP}}") != std::string::npos && tokenized.item.sample.find("{{DST_IP}}") != std::string::npos, "Source or destination IP was not converted to an explicit token");
    expect(tokenized.item.test_case.values.at("CUSTOM_VALUE").front() == "{{EMAIL}}", "Test-case data was not tokenized with the sample");
    for (const auto kind : application::privacy_token_kinds) {
        const auto marker = application::PrivacyAnonymizer::marker(kind);
        const bool in_sample = tokenized.item.sample.find(marker) != std::string::npos;
        const bool in_test_case = tokenized.item.test_case.values.at("CUSTOM_VALUE").front().find(marker) != std::string::npos;
        expect(in_sample || in_test_case, "A supported privacy data type was not tokenized automatically");
    }
    expect(application::LogRenderer::tokenize(tokenized.item).sample == tokenized.item.sample, "Full log tokenization is not idempotent");

    domain::GeneratorConfig config;
    config.source_ip = "198.51.100.10";
    config.destination_ip = "198.51.100.20";
    config.timestamp_generation.mode = domain::TimestampGenerationMode::Offset;
    config.timestamp_generation.offset.days = 1;
    config.timestamp_generation.offset.hours = 2;
    config.templates = {tokenized.item};

    const auto compiled_before_start = cache.compilation_count();
    auto prepared = cache.prepare(config);
    expect(cache.compilation_count() == compiled_before_start, "Stress-test start recompiled an already warmed log template");
    expect(prepared.size() == 1, "Prepared cache changed the selected template count");
    const auto base_time = sys_days{year{2030} / January / 2} + hours{3} + minutes{4} + seconds{5};
    const std::string rendered{prepared.front().render(base_time)};
    expect(rendered.find("2030-01-03T05:04:05Z") != std::string::npos, "Cached timestamp token lost the configured date offset");
    expect(rendered.find("src_ip=198.51.100.10") != std::string::npos && rendered.find("dst_ip=198.51.100.20") != std::string::npos, "Cached IP tokens did not bind the runtime addresses");
    expect(rendered.find("{{") == std::string::npos, "A token leaked from the cached render blueprint");

    config.templates = {raw};
    static_cast<void>(cache.prepare(config));
    expect(cache.compilation_count() == compiled_before_start, "The raw editor sample did not resolve to its precompiled cache alias");

    auto edited = raw;
    edited.sample.append(" event_id=42");
    static_cast<void>(cache.tokenize_and_cache(std::move(edited)));
    expect(cache.compilation_count() == compiled_before_start + 1, "An edited sample reused a stale compiled cache entry");
}

}
