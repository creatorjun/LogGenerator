#include "application/log_preparation_cache.hpp"

#include <chrono>
#include <utility>

namespace loggen::application {
namespace {

void append_key_component(std::string& key, const std::string_view value) {
    key.append(std::to_string(value.size()));
    key.push_back(':');
    key.append(value);
    key.push_back('|');
}

}

std::string LogPreparationCache::cache_key(const domain::LogTemplate& item) {
    std::string key;
    key.reserve(item.sample.size() + item.test_case.values.size() * 32U);
    append_key_component(key, item.sample);
    for (const auto& [token, values] : item.test_case.values) {
        append_key_component(key, token);
        for (const auto& value : values) {
            append_key_component(key, value);
        }
    }
    return key;
}

std::shared_ptr<const LogPreparationCache::CacheEntry> LogPreparationCache::find(const std::string& key) const {
    const std::scoped_lock lock(mutex_);
    const auto found = entries_.find(key);
    return found == entries_.end() ? nullptr : found->second;
}

void LogPreparationCache::insert_alias(std::string key, const std::shared_ptr<const CacheEntry>& entry) {
    const std::scoped_lock lock(mutex_);
    if (entries_.size() >= maximum_entries && !entries_.contains(key)) {
        entries_.erase(entries_.begin());
    }
    entries_.insert_or_assign(std::move(key), entry);
}

std::shared_ptr<const LogPreparationCache::CacheEntry> LogPreparationCache::get_or_compile(const domain::LogTemplate& item) {
    const auto original_key = cache_key(item);
    if (auto cached = find(original_key)) {
        return cached;
    }

    auto tokenized = LogRenderer::tokenize(item);
    const auto tokenized_key = cache_key(tokenized);
    if (auto cached = find(tokenized_key)) {
        insert_alias(original_key, cached);
        return cached;
    }

    auto entry = std::make_shared<CacheEntry>();
    entry->compiled = LogRenderer::compile(tokenized);
    entry->analysis = LogRenderer::analyze(*entry->compiled);

    // Regex compilation intentionally happens outside the lock. Only the first
    // completed compiler publishes its immutable blueprint; concurrent callers
    // reuse that object instead of increasing the live cache footprint.
    {
        const std::scoped_lock lock(mutex_);
        if (const auto found = entries_.find(tokenized_key); found != entries_.end()) {
            entries_.insert_or_assign(original_key, found->second);
            return found->second;
        }
        if (entries_.size() >= maximum_entries && !entries_.contains(tokenized_key)) {
            entries_.erase(entries_.begin());
        }
        entries_.insert_or_assign(tokenized_key, entry);
        if (original_key != tokenized_key) {
            if (entries_.size() >= maximum_entries && !entries_.contains(original_key)) {
                entries_.erase(entries_.begin());
            }
            entries_.insert_or_assign(original_key, entry);
        }
    }
    compilation_count_.fetch_add(1, std::memory_order_relaxed);
    return entry;
}

TokenizedLogTemplate LogPreparationCache::tokenize_and_cache(domain::LogTemplate item) {
    const auto original_key = cache_key(item);
    item = LogRenderer::tokenize(std::move(item));
    const auto entry = get_or_compile(item);
    const auto tokenized_key = cache_key(item);
    if (original_key != tokenized_key) {
        insert_alias(original_key, entry);
    }
    return TokenizedLogTemplate{std::move(item), entry->analysis};
}

void LogPreparationCache::precompile(const std::span<const domain::LogTemplate> items) {
    for (const auto& item : items) {
        static_cast<void>(get_or_compile(item));
    }
}

std::vector<PreparedLog> LogPreparationCache::prepare(const domain::GeneratorConfig& config) {
    std::vector<PreparedLog> result;
    result.reserve(config.templates.size());
    const auto offset = config.timestamp_generation.mode == domain::TimestampGenerationMode::Offset ? config.timestamp_generation.offset.value() : std::chrono::seconds{0};
    for (const auto& item : config.templates) {
        const auto entry = get_or_compile(item);
        result.push_back(LogRenderer::bind(entry->compiled, config.source_ip, config.destination_ip, offset));
    }
    return result;
}

std::size_t LogPreparationCache::size() const {
    const std::scoped_lock lock(mutex_);
    return entries_.size();
}

std::uint64_t LogPreparationCache::compilation_count() const noexcept {
    return compilation_count_.load(std::memory_order_relaxed);
}

}
