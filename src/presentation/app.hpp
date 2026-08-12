// src/presentation/app.hpp
#pragma once

#include "application/ports/log_catalog_use_case.hpp"
#include "application/ports/logger.hpp"
#include "application/ports/stress_test_use_case.hpp"
#include "domain/log_template.hpp"
#include "presentation/catalog_task_runner.hpp"
#include "presentation/d3d11_context.hpp"
#include "presentation/responsive_layout.hpp"

#include <Windows.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::presentation {

class App {
public:
    App(application::ILogCatalogUseCase& catalog_service, application::ILogger& logger, application::IStressTestUseCase& stress_service, std::filesystem::path generated_directory);
    ~App();

    int run(HINSTANCE instance, int show_command);

private:
    struct CatalogViewItem {
        application::CatalogItem item;
        std::string preview;
    };

    static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);
    LRESULT handle_message(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);
    void initialize_imgui();
    void update_ui_scale(float scale);
    void apply_pending_resize();
    void shutdown_imgui() noexcept;
    void shutdown_window() noexcept;
    void request_catalog_load();
    void apply_catalog_result();
    void apply_editor_analysis_result();
    void rebuild_filter();
    [[nodiscard]] std::vector<std::size_t> build_filter(const std::vector<CatalogViewItem>& items) const;
    void refresh_stats();
    void report_error(std::string_view context, std::string_view detail) noexcept;
    void render();
    void render_header(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_metrics(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_configuration(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_destination_panel();
    void render_template_panel(const ResponsiveLayout& layout);
    void render_time_offset(int columns);
    void render_time_range();
    void render_catalog_selector();
    void render_catalog_editor();
    void open_new_catalog_editor();
    void open_selected_catalog_editor();
    [[nodiscard]] bool save_catalog_editor();
    [[nodiscard]] bool delete_selected_catalog_item();
    void analyze_editor_sample();
    [[nodiscard]] std::size_t visible_catalog_index() const noexcept;
    [[nodiscard]] std::vector<domain::LogTemplate> catalog_snapshot() const;
    [[nodiscard]] std::vector<domain::LogTemplate> catalog_snapshot(const std::vector<CatalogViewItem>& items) const;
    void start_test();

    application::ILogCatalogUseCase& catalog_service_;
    application::ILogger& logger_;
    application::IStressTestUseCase& stress_service_;
    CatalogTaskRunner catalog_tasks_;
    CatalogTaskRunner analysis_tasks_;
    std::filesystem::path generated_directory_;
    std::string generated_directory_text_;
    D3d11Context d3d_;
    HINSTANCE instance_{nullptr};
    ATOM window_class_{0};
    HWND window_{nullptr};
    bool imgui_context_ready_{false};
    bool imgui_win32_ready_{false};
    bool imgui_dx11_ready_{false};
    bool imgui_ready_{false};
    unsigned int pending_resize_width_{0};
    unsigned int pending_resize_height_{0};
    float ui_scale_{1.0F};
    std::vector<CatalogViewItem> catalog_items_;
    std::vector<std::size_t> filtered_indices_;
    std::size_t selected_log_{0};
    bool rotate_filtered_{true};
    int protocol_index_{0};
    int framing_index_{0};
    int timestamp_mode_index_{0};
    int offset_sign_index_{0};
    int port_{514};
    int worker_count_{1};
    int offset_days_{0};
    int offset_hours_{0};
    int offset_minutes_{0};
    std::uint64_t target_eps_{0};
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
    application::LogTemplateAnalysis editor_analysis_;
    bool editor_analysis_pending_{false};
    std::chrono::steady_clock::time_point editor_analysis_due_{};
    std::uint64_t editor_analysis_generation_{0};
};

}
