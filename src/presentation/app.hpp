// src/presentation/app.hpp
#pragma once

#include "application/ports/logger.hpp"
#include "application/use_cases/log_catalog.hpp"
#include "application/use_cases/stress_test.hpp"
#include "domain/log_template.hpp"
#include "presentation/responsive_layout.hpp"

#ifdef _WIN32
#include "presentation/d3d11_context.hpp"
#include <Windows.h>
#else
struct GLFWwindow;
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct ImFont;

namespace loggen::presentation {

class App {
public:
    App(application::ILogCatalogUseCase& catalog_service, application::ILogger& logger, application::IStressTestUseCase& stress_service, std::filesystem::path catalog_file, std::filesystem::path generated_directory);
    ~App();

#ifdef _WIN32
    int run(HINSTANCE instance, int show_command);
#else
    int run();
#endif

private:
    struct CatalogLoadResult {
        std::vector<domain::LogTemplate> items;
        std::vector<std::string> search_names;
        std::vector<std::string> previews;
        std::vector<application::LogTemplateAnalysis> analyses;
        std::string error;
        bool replace_items{true};
    };

    struct EditorTokenizationRequest {
        std::uint64_t revision{0};
        std::string source_sample;
        domain::LogTemplate item;
    };

    struct EditorTokenizationResult {
        std::uint64_t revision{0};
        std::string source_sample;
        application::TokenizedLogTemplate tokenized;
        std::string error;
    };

#ifdef _WIN32
    static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);
    LRESULT handle_message(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);
#endif
    void initialize_imgui();
    void update_ui_scale();
    void shutdown_imgui() noexcept;
    void release_window_resources() noexcept;
    void request_catalog_load();
    void request_catalog_save();
    void apply_catalog_result();
    void rebuild_filter();
    void refresh_stats();
    void render();
    void render_header(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_metrics(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_configuration(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_destination_panel(float height);
    void render_template_panel(const ResponsiveLayout& layout);
    void render_time_offset(int columns);
    void render_time_range();
    void render_catalog_selector();
    void render_catalog_editor();
    void open_new_catalog_editor();
    void open_selected_catalog_editor();
    void save_catalog_editor();
    void delete_selected_catalog_item();
    void mark_editor_sample_changed(bool immediate = false);
    void queue_editor_tokenization();
    void apply_editor_tokenization_result();
    void run_editor_tokenizer(std::stop_token stop_token);
    [[nodiscard]] std::size_t visible_catalog_index() const noexcept;
    void start_test();

    application::ILogCatalogUseCase& catalog_service_;
    application::ILogger& logger_;
    application::IStressTestUseCase& stress_service_;
    std::filesystem::path catalog_file_;
    std::filesystem::path generated_directory_;
#ifdef _WIN32
    D3d11Context d3d_;
    HINSTANCE instance_{nullptr};
    HWND window_{nullptr};
    bool window_class_registered_{false};
#else
    GLFWwindow* window_{nullptr};
    int swap_interval_{1};
#endif
    bool imgui_ready_{false};
    float ui_scale_{1.0F};
    float dpi_scale_{1.0F};
    float visual_scale_{1.0F};
    ImFont* regular_font_{nullptr};
    ImFont* bold_font_{nullptr};
    std::vector<domain::LogTemplate> catalog_items_;
    std::vector<std::string> catalog_search_names_;
    std::vector<std::string> catalog_previews_;
    std::vector<application::LogTemplateAnalysis> catalog_analyses_;
    std::vector<std::size_t> filtered_indices_;
    std::size_t selected_log_{0};
    bool rotate_filtered_{true};
    int protocol_index_{0};
    int transmission_mode_index_{static_cast<int>(domain::TransmissionMode::Parallel)};
    int framing_index_{0};
    int timestamp_mode_index_{0};
    int offset_sign_index_{0};
    int port_{514};
    int offset_days_{0};
    int offset_hours_{0};
    int offset_minutes_{0};
    std::uint64_t target_eps_{0};
    std::uint64_t file_max_total_mib_{0};
    int file_max_count_{0};
    int file_max_duration_seconds_{0};
    bool verify_certificate_{true};
    std::array<char, 256> host_{"127.0.0.1"};
    std::array<char, 256> tls_server_name_{};
    std::array<char, 64> source_ip_{"10.0.0.10"};
    std::array<char, 64> destination_ip_{"10.0.0.20"};
    std::array<char, 11> range_start_{"2026-01-01"};
    std::array<char, 11> range_end_{"2026-07-31"};
    std::array<char, 256> search_{};
    std::string ui_error_;
    domain::TransmissionStats cached_stats_;
    std::string current_eps_text_{"0"};
    std::string average_eps_text_{"0"};
    std::string total_messages_text_{"0"};
    std::string total_bytes_text_{"0 B"};
    std::chrono::steady_clock::time_point next_stats_refresh_{};
    bool editor_popup_requested_{false};
    bool delete_popup_requested_{false};
    bool editor_is_new_{false};
    std::size_t editor_index_{0};
    std::size_t delete_index_{0};
    std::string editor_name_;
    std::string editor_sample_;
    std::string editor_tokenized_preview_;
    std::optional<domain::LogTemplate> editor_tokenized_item_;
    application::LogTemplateAnalysis editor_analysis_;
    bool editor_analysis_pending_{false};
    std::chrono::steady_clock::time_point editor_analysis_due_{};
    std::uint64_t editor_revision_{0};
    std::atomic<bool> editor_tokenizing_{false};
    std::atomic<bool> editor_tokenization_ready_{false};
    std::mutex editor_tokenization_mutex_;
    std::condition_variable_any editor_tokenization_condition_;
    std::optional<EditorTokenizationRequest> pending_editor_tokenization_;
    std::optional<EditorTokenizationResult> pending_editor_tokenization_result_;
    std::jthread editor_tokenizer_;
    std::atomic<bool> catalog_loading_{false};
    std::atomic<bool> catalog_result_ready_{false};
    std::mutex catalog_result_mutex_;
    std::optional<CatalogLoadResult> pending_catalog_result_;
    std::jthread catalog_loader_;
};

}
