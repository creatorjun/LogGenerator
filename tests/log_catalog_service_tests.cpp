// tests/log_catalog_service_tests.cpp
#include "test_support.hpp"

#include "application/log_catalog_service.hpp"

#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace loggen::tests {
namespace {

class InMemoryCatalog final : public application::ILogCatalog {
public:
    explicit InMemoryCatalog(std::vector<domain::LogTemplate> items)
        : items_(std::move(items)) {
    }

    [[nodiscard]] std::vector<domain::LogTemplate> load() const override {
        return items_;
    }

    void save(const std::span<const domain::LogTemplate> items) override {
        items_.assign(items.begin(), items.end());
    }

    [[nodiscard]] const std::vector<domain::LogTemplate>& items() const noexcept {
        return items_;
    }

private:
    std::vector<domain::LogTemplate> items_;
};

class CatalogNullLogger final : public application::ILogger {
public:
    void log(const application::LogLevel, const std::string_view) noexcept override {
    }
};

class SequentialIdentifierGenerator final : public application::IIdentifierGenerator {
public:
    [[nodiscard]] std::string next(const std::string_view prefix) override {
        return std::string(prefix) + '-' + std::to_string(++sequence_);
    }

private:
    int sequence_{0};
};

}

void run_log_catalog_service_tests() {
    InMemoryCatalog catalog{{{"sample-1", "Account event", "user_name=김테스트 user_id=real-user timestamp=2025-07-05T13:53:53Z"}}};
    CatalogNullLogger logger;
    SequentialIdentifierGenerator identifier_generator;
    application::LogCatalogService service{catalog, identifier_generator, logger};
    const auto loaded = service.load();
    expect(loaded.size() == 1, "Catalog service changed the loaded item count");
    expect(loaded.front().log.sample.find("김테스트") == std::string::npos, "Catalog service did not anonymize a loaded sample");
    expect(loaded.front().analysis.privacy_token_count == 2, "Catalog service did not analyze privacy tokens");
    expect(loaded.front().search_text.find("account") != std::string::npos, "Catalog service did not create a category search index");

    std::vector<domain::LogTemplate> existing{loaded.front().log};
    const auto created = service.create(existing, "New event", "department=보안팀 host_name=REAL-HOST");
    expect(created.log.id.starts_with("user-"), "Catalog service did not generate a user item id");
    expect(created.log.sample.find("보안팀") == std::string::npos, "Catalog service did not anonymize a created sample");
    const auto updated = service.update(created.log, "Updated event", "user_name=홍테스트");
    expect(updated.log.id == created.log.id, "Catalog service changed an id during update");
    expect(updated.log.name == "Updated event", "Catalog service did not update the item name");

    existing.push_back(updated.log);
    service.save(existing);
    expect(catalog.items().size() == 2, "Catalog service did not persist the supplied snapshot");
}

}
