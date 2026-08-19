#pragma once

#include "application/log_renderer.hpp"
#include "application/models/tokenized_log_template.hpp"
#include "domain/generator_config.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace loggen::application {

class LogPreparationCache final {
public:
    [[nodiscard]] TokenizedLogTemplate tokenize_and_cache(domain::LogTemplate item);
    void precompile(std::span<const domain::LogTemplate> items);
    [[nodiscard]] std::vector<PreparedLog> prepare(const domain::GeneratorConfig& config);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::uint64_t compilation_count() const noexcept;

private:
    struct CacheEntry {
        std::shared_ptr<const CompiledLog> compiled;
        LogTemplateAnalysis analysis;
    };

    [[nodiscard]] static std::string cache_key(const domain::LogTemplate& item);
    [[nodiscard]] std::shared_ptr<const CacheEntry> find(const std::string& key) const;
    [[nodiscard]] std::shared_ptr<const CacheEntry> get_or_compile(const domain::LogTemplate& item);
    void insert_alias(std::string key, const std::shared_ptr<const CacheEntry>& entry);

    static constexpr std::size_t maximum_entries{512};
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<const CacheEntry>> entries_;
    std::atomic<std::uint64_t> compilation_count_{0};
};

}
