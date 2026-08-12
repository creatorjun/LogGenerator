// src/presentation/catalog_task_runner.hpp
#pragma once

#include "application/ports/logger.hpp"
#include "application/ports/log_catalog_use_case.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace loggen::presentation {

struct CatalogTaskResult {
    std::vector<application::CatalogItem> items;
    std::optional<application::LogTemplateAnalysis> analysis;
    std::string error;
    std::uint64_t request_id{0};
    bool replace_items{true};
};

class CatalogTaskRunner {
public:
    CatalogTaskRunner(application::ILogCatalogUseCase& catalog_service, application::ILogger& logger);
    ~CatalogTaskRunner();

    CatalogTaskRunner(const CatalogTaskRunner&) = delete;
    CatalogTaskRunner& operator=(const CatalogTaskRunner&) = delete;

    [[nodiscard]] bool request_load();
    [[nodiscard]] bool request_save(std::vector<domain::LogTemplate> items);
    [[nodiscard]] bool request_analyze(std::string sample, std::uint64_t request_id);
    [[nodiscard]] std::optional<CatalogTaskResult> poll();
    [[nodiscard]] bool busy() const noexcept;

private:
    void join_completed();
    void publish(CatalogTaskResult result) noexcept;
    void abandon() noexcept;

    application::ILogCatalogUseCase& catalog_service_;
    application::ILogger& logger_;
    std::atomic<bool> busy_{false};
    std::atomic<bool> ready_{false};
    std::mutex result_mutex_;
    std::optional<CatalogTaskResult> result_;
    std::jthread worker_;
};

}
