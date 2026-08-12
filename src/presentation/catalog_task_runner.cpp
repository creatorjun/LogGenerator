// src/presentation/catalog_task_runner.cpp
#include "presentation/catalog_task_runner.hpp"

#include <exception>
#include <format>
#include <utility>

namespace loggen::presentation {

CatalogTaskRunner::CatalogTaskRunner(application::ILogCatalogUseCase& catalog_service, application::ILogger& logger)
    : catalog_service_(catalog_service), logger_(logger) {
}

CatalogTaskRunner::~CatalogTaskRunner() {
    try {
        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }
    } catch (...) {
        logger_.critical("Catalog background task shutdown failed");
    }
}

bool CatalogTaskRunner::request_load() {
    if (busy_.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }
    if (ready_.load(std::memory_order_acquire)) {
        busy_.store(false, std::memory_order_release);
        return false;
    }
    try {
        join_completed();
        worker_ = std::jthread([this](std::stop_token) {
            try {
                CatalogTaskResult result;
                try {
                    result.items = catalog_service_.load();
                } catch (const std::exception& error) {
                    result.error = error.what();
                    try {
                        logger_.error(std::format("Sample log catalog load failed: {}", error.what()));
                    } catch (...) {
                        logger_.error("Sample log catalog load failed");
                    }
                } catch (...) {
                    result.error = "Unknown sample log catalog load failure";
                    logger_.error(result.error);
                }
                publish(std::move(result));
            } catch (...) {
                abandon();
            }
        });
    } catch (const std::exception& error) {
        try {
            CatalogTaskResult result;
            result.error = error.what();
            publish(std::move(result));
        } catch (...) {
            abandon();
        }
    } catch (...) {
        try {
            CatalogTaskResult result;
            result.error = "Unknown catalog task startup failure";
            publish(std::move(result));
        } catch (...) {
            abandon();
        }
    }
    return true;
}

bool CatalogTaskRunner::request_save(std::vector<domain::LogTemplate> items) {
    if (busy_.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }
    if (ready_.load(std::memory_order_acquire)) {
        busy_.store(false, std::memory_order_release);
        return false;
    }
    try {
        join_completed();
        worker_ = std::jthread([this, items = std::move(items)](std::stop_token) {
            try {
                CatalogTaskResult result;
                result.replace_items = false;
                try {
                    catalog_service_.save(items);
                } catch (const std::exception& error) {
                    result.error = error.what();
                    try {
                        logger_.error(std::format("Sample log catalog save failed: {}", error.what()));
                    } catch (...) {
                        logger_.error("Sample log catalog save failed");
                    }
                } catch (...) {
                    result.error = "Unknown sample log catalog save failure";
                    logger_.error(result.error);
                }
                publish(std::move(result));
            } catch (...) {
                abandon();
            }
        });
    } catch (const std::exception& error) {
        try {
            CatalogTaskResult result;
            result.replace_items = false;
            result.error = error.what();
            publish(std::move(result));
        } catch (...) {
            abandon();
        }
    } catch (...) {
        try {
            CatalogTaskResult result;
            result.replace_items = false;
            result.error = "Unknown catalog task startup failure";
            publish(std::move(result));
        } catch (...) {
            abandon();
        }
    }
    return true;
}

bool CatalogTaskRunner::request_analyze(std::string sample, const std::uint64_t request_id) {
    if (busy_.exchange(true, std::memory_order_acq_rel)) {
        return false;
    }
    if (ready_.load(std::memory_order_acquire)) {
        busy_.store(false, std::memory_order_release);
        return false;
    }
    try {
        join_completed();
        worker_ = std::jthread([this, sample = std::move(sample), request_id](std::stop_token) {
            try {
                CatalogTaskResult result;
                result.replace_items = false;
                result.request_id = request_id;
                try {
                    result.analysis = catalog_service_.analyze(sample);
                } catch (const std::exception& error) {
                    result.error = error.what();
                    try {
                        logger_.error(std::format("Sample log analysis failed: {}", error.what()));
                    } catch (...) {
                        logger_.error("Sample log analysis failed");
                    }
                } catch (...) {
                    result.error = "Unknown sample log analysis failure";
                    logger_.error(result.error);
                }
                publish(std::move(result));
            } catch (...) {
                abandon();
            }
        });
    } catch (const std::exception& error) {
        try {
            CatalogTaskResult result;
            result.replace_items = false;
            result.request_id = request_id;
            result.error = error.what();
            publish(std::move(result));
        } catch (...) {
            abandon();
        }
    } catch (...) {
        try {
            CatalogTaskResult result;
            result.replace_items = false;
            result.request_id = request_id;
            result.error = "Unknown catalog task startup failure";
            publish(std::move(result));
        } catch (...) {
            abandon();
        }
    }
    return true;
}

std::optional<CatalogTaskResult> CatalogTaskRunner::poll() {
    if (!ready_.exchange(false, std::memory_order_acq_rel)) {
        return std::nullopt;
    }
    std::scoped_lock lock(result_mutex_);
    auto result = std::move(result_);
    result_.reset();
    return result;
}

bool CatalogTaskRunner::busy() const noexcept {
    return busy_.load(std::memory_order_acquire);
}

void CatalogTaskRunner::join_completed() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void CatalogTaskRunner::publish(CatalogTaskResult result) noexcept {
    try {
        {
            std::scoped_lock lock(result_mutex_);
            result_ = std::move(result);
        }
        ready_.store(true, std::memory_order_release);
    } catch (...) {
        ready_.store(false, std::memory_order_release);
        logger_.critical("Catalog background task result publication failed");
    }
    busy_.store(false, std::memory_order_release);
}

void CatalogTaskRunner::abandon() noexcept {
    ready_.store(false, std::memory_order_release);
    busy_.store(false, std::memory_order_release);
    logger_.critical("Catalog background task failed without a recoverable result");
}

}
