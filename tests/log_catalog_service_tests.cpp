// tests/log_catalog_service_tests.cpp
#include "test_support.hpp"

#include "application/log_catalog_service.hpp"

#include <filesystem>
#include <span>
#include <vector>

namespace loggen::tests {
namespace {

class MemoryLogCatalog final : public application::ILogCatalog {
public:
    [[nodiscard]] std::vector<domain::LogTemplate> load(const std::filesystem::path&) const override {
        return loaded;
    }

    void save(const std::filesystem::path&, const std::span<const domain::LogTemplate> items) const override {
        saved.assign(items.begin(), items.end());
    }

    std::vector<domain::LogTemplate> loaded;
    mutable std::vector<domain::LogTemplate> saved;
};

}

void run_log_catalog_service_tests() {
    MemoryLogCatalog catalog;
    catalog.loaded.push_back({"sample", "Sample", "email=test@example.com", "test", {}});
    application::LogCatalogService service{catalog};

    const auto loaded = service.load(std::filesystem::path{"catalog.json"});
    expect(loaded.size() == 1, "Catalog service changed the loaded item count");
    expect(loaded.front().sample.find("test@example.com") == std::string::npos, "Catalog service exposed unsanitized personal data");
    expect(loaded.front().sample.find("{{EMAIL}}") != std::string::npos, "Catalog service did not apply the privacy boundary");

    service.save(std::filesystem::path{"catalog.json"}, catalog.loaded);
    expect(catalog.saved.size() == catalog.loaded.size(), "Catalog service did not delegate persistence");
    expect(catalog.saved.front().sample.find("test@example.com") == std::string::npos, "Catalog service persisted unsanitized personal data");
    expect(catalog.saved.front().sample.find("{{EMAIL}}") != std::string::npos, "Catalog service changed the privacy marker while saving");
}

}
