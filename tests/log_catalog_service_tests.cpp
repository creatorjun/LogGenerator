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
    catalog.loaded.push_back({"0001", "Sample", "email=test@example.com", "test", {}});
    application::LogCatalogService service{catalog};

    const auto loaded = service.load(std::filesystem::path{"catalog.json"});
    expect(loaded.size() == 1, "Catalog service changed the loaded item count");
    expect(loaded.front().sample.find("test@example.com") == std::string::npos, "Catalog service exposed unsanitized personal data");
    expect(loaded.front().sample.find("{{EMAIL}}") != std::string::npos, "Catalog service did not apply the privacy boundary");

    service.save(std::filesystem::path{"catalog.json"}, catalog.loaded);
    expect(catalog.saved.size() == catalog.loaded.size(), "Catalog service did not delegate persistence");
    expect(catalog.saved.front().sample.find("test@example.com") == std::string::npos, "Catalog service persisted unsanitized personal data");
    expect(catalog.saved.front().sample.find("{{EMAIL}}") != std::string::npos, "Catalog service changed the privacy marker while saving");

    expect(application::LogCatalogService::next_id(catalog.loaded) == "0002", "Catalog service did not create the next numeric sample id");
    catalog.loaded.push_back({"0010", "Later sample", "message=ok", "test", {}});
    expect(application::LogCatalogService::next_id(catalog.loaded) == "0011", "Catalog service did not advance from the maximum numeric sample id");

    catalog.loaded.front().id = "sample-0001";
    bool invalid_load_rejected = false;
    try {
        static_cast<void>(service.load(std::filesystem::path{"catalog.json"}));
    } catch (const std::invalid_argument&) {
        invalid_load_rejected = true;
    }
    expect(invalid_load_rejected, "Catalog service accepted a non-numeric loaded sample id");

    bool invalid_save_rejected = false;
    try {
        service.save(std::filesystem::path{"catalog.json"}, catalog.loaded);
    } catch (const std::invalid_argument&) {
        invalid_save_rejected = true;
    }
    expect(invalid_save_rejected, "Catalog service accepted a non-numeric saved sample id");
}

}
