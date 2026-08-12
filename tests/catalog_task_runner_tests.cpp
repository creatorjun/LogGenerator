// tests/catalog_task_runner_tests.cpp
#include "test_support.hpp"

#include "application/log_catalog_service.hpp"
#include "application/ports/identifier_generator.hpp"
#include "application/ports/log_catalog.hpp"
#include "presentation/catalog_task_runner.hpp"

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace loggen::tests {
namespace {

class TaskCatalog final : public application::ILogCatalog {
public:
    [[nodiscard]] std::vector<domain::LogTemplate> load() const override {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        return items_;
    }

    void save(const std::span<const domain::LogTemplate> items) override {
        items_.assign(items.begin(), items.end());
    }

    std::vector<domain::LogTemplate> items_{{"task-1", "Task", "user_name=김테스트"}};
};

class TaskIdentifierGenerator final : public application::IIdentifierGenerator {
public:
    [[nodiscard]] std::string next(const std::string_view prefix) override {
        return std::string(prefix) + "-task";
    }
};

class TaskNullLogger final : public application::ILogger {
public:
    void log(const application::LogLevel, const std::string_view) noexcept override {
    }
};

class ThrowingCatalogUseCase final : public application::ILogCatalogUseCase {
public:
    [[nodiscard]] std::vector<application::CatalogItem> load() const override {
        throw 7;
    }

    void save(const std::span<const domain::LogTemplate>) override {
        throw 7;
    }

    [[nodiscard]] application::CatalogItem create(const std::span<const domain::LogTemplate>, std::string, std::string) override {
        throw 7;
    }

    [[nodiscard]] application::CatalogItem update(const domain::LogTemplate&, std::string, std::string) const override {
        throw 7;
    }

    [[nodiscard]] application::CatalogItem describe(domain::LogTemplate) const override {
        throw 7;
    }

    [[nodiscard]] application::LogTemplateAnalysis analyze(const std::string_view) const override {
        throw 7;
    }
};

void wait_until_idle(presentation::CatalogTaskRunner& runner) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (runner.busy() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    expect(!runner.busy(), "Catalog background task did not complete");
}

}

void run_catalog_task_runner_tests() {
    TaskCatalog catalog;
    TaskIdentifierGenerator identifier_generator;
    TaskNullLogger logger;
    application::LogCatalogService service{catalog, identifier_generator, logger};
    presentation::CatalogTaskRunner runner{service, logger};

    expect(runner.request_load(), "Catalog load task was not accepted");
    expect(!runner.request_load(), "Catalog runner accepted overlapping work");
    wait_until_idle(runner);
    expect(!runner.request_load(), "Catalog runner replaced an unconsumed result");
    auto loaded = runner.poll();
    if (!loaded) {
        throw std::runtime_error("Catalog runner did not publish the load result");
    }
    expect(loaded->items.size() == 1, "Catalog runner published an invalid load result");
    expect(loaded->items.front().log.sample.find("김테스트") == std::string::npos, "Catalog runner bypassed the catalog use case");

    std::vector<domain::LogTemplate> replacement{{"task-2", "Replacement", "event"}};
    expect(runner.request_save(std::move(replacement)), "Catalog save task was not accepted");
    wait_until_idle(runner);
    const auto saved = runner.poll();
    expect(saved.has_value() && !saved->replace_items && saved->error.empty(), "Catalog runner did not publish the save result");
    expect(catalog.items_.size() == 1 && catalog.items_.front().id == "task-2", "Catalog runner did not persist the requested snapshot");

    expect(runner.request_analyze("timestamp=2025-07-05T13:53:53Z user_name={{PERSON}}", 42), "Catalog analysis task was not accepted");
    wait_until_idle(runner);
    const auto analyzed = runner.poll();
    if (!analyzed || !analyzed->analysis) {
        throw std::runtime_error("Catalog runner did not publish the analysis result");
    }
    expect(analyzed->request_id == 42, "Catalog runner published an analysis result with the wrong request id");
    expect(analyzed->analysis->timestamp_count == 1 && analyzed->analysis->privacy_token_count == 1, "Catalog background analysis result is incorrect");

    ThrowingCatalogUseCase throwing_service;
    presentation::CatalogTaskRunner throwing_runner{throwing_service, logger};
    expect(throwing_runner.request_load(), "Throwing catalog load task was not accepted");
    wait_until_idle(throwing_runner);
    const auto failed_load = throwing_runner.poll();
    expect(failed_load.has_value() && !failed_load->error.empty(), "A non-standard catalog exception was not reported");
    expect(throwing_runner.request_analyze("sample", 77), "Throwing catalog analysis task was not accepted");
    wait_until_idle(throwing_runner);
    const auto failed_analysis = throwing_runner.poll();
    expect(failed_analysis.has_value() && failed_analysis->request_id == 77 && !failed_analysis->error.empty(), "A non-standard analysis exception was not reported");
}

}
