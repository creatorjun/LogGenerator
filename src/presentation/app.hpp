// src/presentation/app.hpp
#pragma once

#include "application/ports/log_catalog.hpp"
#include "application/ports/logger.hpp"
#include "application/stress_test_service.hpp"
#include "domain/log_template.hpp"
#include "presentation/d3d11_context.hpp"
#include "presentation/responsive_layout.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace loggen::presentation {

class App {
public:
    App(application::ILogCatalog& catalog, application::ILogger& logger, application::StressTestService& stress_service, std::filesystem::path sample_directory);
    ~App();

    int run(HINSTANCE instance, int show_command);

private:
    static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);
    LRESULT handle_message(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);
    void initialize_imgui();
    void update_ui_scale(float scale);
    void shutdown_imgui() noexcept;
    void load_catalog();
    void render();
    void render_header(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_metrics(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_configuration(const domain::TransmissionStats& stats, const ResponsiveLayout& layout);
    void render_destination_panel();
    void render_template_panel(const ResponsiveLayout& layout);
    void render_time_offset(int columns);
    void render_catalog_selector();
    void start_test();
    [[nodiscard]] std::vector<std::size_t> filtered_indices() const;

    application::ILogCatalog& catalog_;
    application::ILogger& logger_;
    application::StressTestService& stress_service_;
    std::filesystem::path sample_directory_;
    D3d11Context d3d_;
    HWND window_{nullptr};
    bool imgui_ready_{false};
    float ui_scale_{1.0F};
    std::vector<domain::LogTemplate> catalog_items_;
    std::size_t selected_log_{0};
    bool rotate_filtered_{true};
    int protocol_index_{0};
    int framing_index_{0};
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
    std::array<char, 256> search_{};
    std::string ui_error_;
};

}
