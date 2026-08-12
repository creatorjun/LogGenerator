// src/presentation/app.cpp
#include "presentation/app.hpp"

#include "domain/generator_config.hpp"
#include "presentation/ui_theme.hpp"

#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);

namespace loggen::presentation {
namespace {

bool is_active(const domain::GeneratorState state) {
    return state == domain::GeneratorState::Connecting || state == domain::GeneratorState::Running || state == domain::GeneratorState::Stopping;
}

std::string_view state_text(const domain::GeneratorState state) {
    switch (state) {
    case domain::GeneratorState::Stopped:
        return "중지됨";
    case domain::GeneratorState::Connecting:
        return "연결 중";
    case domain::GeneratorState::Running:
        return "전송 중";
    case domain::GeneratorState::Stopping:
        return "중지 중";
    case domain::GeneratorState::Failed:
        return "오류";
    }
    return "알 수 없음";
}

std::string_view timestamp_style_text(const application::TimestampStyle style) {
    switch (style) {
    case application::TimestampStyle::Iso8601:
        return "ISO 8601 / Year First";
    case application::TimestampStyle::YearFirst:
        return "Year First";
    case application::TimestampStyle::SyslogWithYear:
        return "Syslog With Year";
    case application::TimestampStyle::SyslogWithoutYear:
        return "Syslog";
    case application::TimestampStyle::MonthFirstGmt:
        return "Month First GMT";
    case application::TimestampStyle::Apache:
        return "Apache";
    case application::TimestampStyle::Compact:
        return "Compact yyyyMMddHHmmss";
    case application::TimestampStyle::MonthDayYear:
        return "MMM dd yyyy HH:mm:ss";
    case application::TimestampStyle::DateOnly:
        return "Separated Date";
    case application::TimestampStyle::TimeOnly:
        return "Separated Time";
    }
    return "Unknown";
}

ImVec4 state_color(const domain::GeneratorState state) {
    switch (state) {
    case domain::GeneratorState::Running:
        return {0.24F, 0.84F, 0.53F, 1.0F};
    case domain::GeneratorState::Connecting:
    case domain::GeneratorState::Stopping:
        return {1.0F, 0.72F, 0.25F, 1.0F};
    case domain::GeneratorState::Failed:
        return {1.0F, 0.35F, 0.38F, 1.0F};
    case domain::GeneratorState::Stopped:
        return {0.55F, 0.61F, 0.70F, 1.0F};
    }
    return {0.55F, 0.61F, 0.70F, 1.0F};
}

std::string format_number(const double value) {
    char buffer[64]{};
    if (value >= 1'000'000'000.0) {
        std::snprintf(buffer, sizeof(buffer), "%.2fB", value / 1'000'000'000.0);
    } else if (value >= 1'000'000.0) {
        std::snprintf(buffer, sizeof(buffer), "%.2fM", value / 1'000'000.0);
    } else if (value >= 1'000.0) {
        std::snprintf(buffer, sizeof(buffer), "%.2fK", value / 1'000.0);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.0f", value);
    }
    return buffer;
}

std::string format_bytes(const std::uint64_t bytes) {
    static constexpr std::string_view units[]{"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(units)) {
        value /= 1024.0;
        ++unit;
    }
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), unit == 0 ? "%.0f %s" : "%.2f %s", value, units[unit].data());
    return buffer;
}

void metric_card(const char* id, const char* label, const std::string& value, const ImVec4 accent) {
    const float height = ImGui::GetTextLineHeightWithSpacing() * 3.4F;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075F, 0.092F, 0.125F, 1.0F));
    ImGui::BeginChild(id, ImVec2(0.0F, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::TextColored(accent, "%s", label);
    const float value_scale = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 10.0F ? 1.15F : 1.35F;
    ImGui::SetWindowFontScale(value_scale);
    ImGui::TextUnformatted(value.c_str());
    ImGui::SetWindowFontScale(1.0F);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void disabled_wrapped_text(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string sample_preview(const std::string_view sample) {
    const auto length = std::min<std::size_t>(sample.size(), 180);
    std::string result{sample.substr(0, length)};
    if (length < sample.size()) {
        result.append("...");
    }
    return result;
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::optional<std::chrono::sys_days> parse_iso_date(const std::string_view value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return std::nullopt;
    }
    int year_value = 0;
    unsigned int month_value = 0;
    unsigned int day_value = 0;
    const auto year_result = std::from_chars(value.data(), value.data() + 4, year_value);
    const auto month_result = std::from_chars(value.data() + 5, value.data() + 7, month_value);
    const auto day_result = std::from_chars(value.data() + 8, value.data() + 10, day_value);
    if (year_result.ec != std::errc{} || year_result.ptr != value.data() + 4 || month_result.ec != std::errc{} || month_result.ptr != value.data() + 7 || day_result.ec != std::errc{} || day_result.ptr != value.data() + 10) {
        return std::nullopt;
    }
    const std::chrono::year_month_day date{std::chrono::year{year_value}, std::chrono::month{month_value}, std::chrono::day{day_value}};
    if (!date.ok()) {
        return std::nullopt;
    }
    return std::chrono::sys_days{date};
}

}

App::App(application::ILogCatalogUseCase& catalog_service, application::ILogger& logger, application::IStressTestUseCase& stress_service, std::filesystem::path generated_directory)
    : catalog_service_(catalog_service), logger_(logger), stress_service_(stress_service), catalog_tasks_(catalog_service, logger), analysis_tasks_(catalog_service, logger), generated_directory_(std::move(generated_directory)), generated_directory_text_(path_to_utf8(generated_directory_)) {
}

App::~App() {
    stress_service_.stop();
    shutdown_imgui();
    d3d_.reset();
    shutdown_window();
}

int App::run(const HINSTANCE instance, const int show_command) {
    logger_.info("UI initialization started");
    instance_ = instance;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_CLASSDC;
    window_class.lpfnWndProc = &App::window_procedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"LogGeneratorWindow";
    window_class_ = RegisterClassExW(&window_class);
    if (window_class_ == 0) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Window class registration failed");
    }

    const UINT detected_system_dpi = GetDpiForSystem();
    const UINT dpi = detected_system_dpi == 0 ? 96U : detected_system_dpi;
    RECT bounds{0, 0, MulDiv(1180, static_cast<int>(dpi), 96), MulDiv(900, static_cast<int>(dpi), 96)};
    AdjustWindowRectExForDpi(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    RECT work_area{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    const int window_width = static_cast<int>(std::max<LONG>(1, std::min(bounds.right - bounds.left, work_area.right - work_area.left)));
    const int window_height = static_cast<int>(std::max<LONG>(1, std::min(bounds.bottom - bounds.top, work_area.bottom - work_area.top)));
    window_ = CreateWindowExW(0, window_class.lpszClassName, L"LogGenerator", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height, nullptr, nullptr, instance, this);
    if (window_ == nullptr) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Window creation failed");
    }
    const BOOL dark_mode = TRUE;
    DwmSetWindowAttribute(window_, 20, &dark_mode, sizeof(dark_mode));
    d3d_.create(window_);
    initialize_imgui();
    ShowWindow(window_, show_command);
    UpdateWindow(window_);
    request_catalog_load();
    logger_.info("UI initialization completed");

    MSG message{};
    bool finished = false;
    while (!finished) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) {
                finished = true;
            }
        }
        if (finished) {
            break;
        }
        if (IsIconic(window_)) {
            WaitMessage();
            continue;
        }
        apply_pending_resize();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        render();
        ImGui::Render();
        d3d_.clear(0.055F, 0.067F, 0.09F, 1.0F);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        d3d_.present();
    }
    stress_service_.stop();
    shutdown_imgui();
    d3d_.reset();
    shutdown_window();
    logger_.info("UI event loop stopped");
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::window_procedure(const HWND window, const UINT message, const WPARAM word_parameter, const LPARAM long_parameter) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(long_parameter);
        app = static_cast<App*>(creation->lpCreateParams);
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR previous = SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
            app->report_error("Window initialization failed", "window state could not be attached");
            return FALSE;
        }
    }
    if (app != nullptr) {
        try {
            return app->handle_message(window, message, word_parameter, long_parameter);
        } catch (const std::exception& error) {
            app->report_error("Window message processing failed", error.what());
            PostQuitMessage(1);
            return 0;
        } catch (...) {
            app->report_error("Window message processing failed", "unknown error");
            PostQuitMessage(1);
            return 0;
        }
    }
    return DefWindowProcW(window, message, word_parameter, long_parameter);
}

LRESULT App::handle_message(const HWND window, const UINT message, const WPARAM word_parameter, const LPARAM long_parameter) {
    if (imgui_ready_ && ImGui_ImplWin32_WndProcHandler(window, message, word_parameter, long_parameter)) {
        return 1;
    }
    switch (message) {
    case WM_SIZE:
        if (word_parameter != SIZE_MINIMIZED && d3d_.device() != nullptr) {
            pending_resize_width_ = static_cast<unsigned int>(LOWORD(long_parameter));
            pending_resize_height_ = static_cast<unsigned int>(HIWORD(long_parameter));
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* information = reinterpret_cast<MINMAXINFO*>(long_parameter);
        const UINT detected_dpi = GetDpiForWindow(window);
        const UINT dpi = detected_dpi == 0 ? 96U : detected_dpi;
        information->ptMinTrackSize.x = MulDiv(520, static_cast<int>(dpi), 96);
        information->ptMinTrackSize.y = MulDiv(480, static_cast<int>(dpi), 96);
        return 0;
    }
    case WM_DPICHANGED: {
        const auto* suggested_bounds = reinterpret_cast<const RECT*>(long_parameter);
        SetWindowPos(window, nullptr, suggested_bounds->left, suggested_bounds->top, suggested_bounds->right - suggested_bounds->left, suggested_bounds->bottom - suggested_bounds->top, SWP_NOACTIVATE | SWP_NOZORDER);
        update_ui_scale(static_cast<float>(HIWORD(word_parameter)) / 96.0F);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((word_parameter & 0xFFF0U) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, word_parameter, long_parameter);
}

void App::initialize_imgui() {
    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr) {
        throw std::runtime_error("Dear ImGui context creation failed");
    }
    imgui_context_ready_ = true;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    const UINT detected_dpi = GetDpiForWindow(window_);
    ui_scale_ = static_cast<float>(detected_dpi == 0 ? 96U : detected_dpi) / 96.0F;
    const std::filesystem::path korean_font = L"C:\\Windows\\Fonts\\malgun.ttf";
    ImFont* loaded_font = nullptr;
    if (std::filesystem::exists(korean_font)) {
        loaded_font = io.Fonts->AddFontFromFileTTF(path_to_utf8(korean_font).c_str(), 17.0F, nullptr, io.Fonts->GetGlyphRangesKorean());
    }
    if (loaded_font == nullptr) {
        io.Fonts->AddFontDefault();
    }
    apply_ui_theme(ui_scale_);
    if (!ImGui_ImplWin32_Init(window_)) {
        throw std::runtime_error("Dear ImGui Win32 initialization failed");
    }
    imgui_win32_ready_ = true;
    if (!ImGui_ImplDX11_Init(d3d_.device(), d3d_.context())) {
        throw std::runtime_error("Dear ImGui initialization failed");
    }
    imgui_dx11_ready_ = true;
    imgui_ready_ = true;
}

void App::update_ui_scale(const float scale) {
    const float next_scale = std::clamp(scale, 1.0F, 4.0F);
    if (std::abs(next_scale - ui_scale_) < 0.01F) {
        return;
    }
    ui_scale_ = next_scale;
    if (imgui_ready_) {
        apply_ui_theme(ui_scale_);
        logger_.info(std::format("UI DPI scale changed: {:.2f}", ui_scale_));
    }
}

void App::apply_pending_resize() {
    if (pending_resize_width_ == 0 || pending_resize_height_ == 0) {
        return;
    }
    const auto width = std::exchange(pending_resize_width_, 0U);
    const auto height = std::exchange(pending_resize_height_, 0U);
    try {
        d3d_.resize(width, height);
    } catch (const std::exception& error) {
        report_error("Window resize failed", error.what());
        if (!d3d_.renderable()) {
            throw;
        }
    } catch (...) {
        report_error("Window resize failed", "unknown error");
        if (!d3d_.renderable()) {
            throw;
        }
    }
}

void App::shutdown_imgui() noexcept {
    imgui_ready_ = false;
    if (imgui_dx11_ready_) {
        ImGui_ImplDX11_Shutdown();
        imgui_dx11_ready_ = false;
    }
    if (imgui_win32_ready_) {
        ImGui_ImplWin32_Shutdown();
        imgui_win32_ready_ = false;
    }
    if (imgui_context_ready_) {
        ImGui::DestroyContext();
        imgui_context_ready_ = false;
    }
}

void App::shutdown_window() noexcept {
    if (window_ != nullptr && IsWindow(window_)) {
        DestroyWindow(window_);
    }
    window_ = nullptr;
    if (window_class_ != 0 && instance_ != nullptr) {
        UnregisterClassW(L"LogGeneratorWindow", instance_);
    }
    window_class_ = 0;
    instance_ = nullptr;
}

void App::request_catalog_load() {
    try {
        if (!catalog_tasks_.request_load()) {
            report_error("Catalog load was not started", "another catalog result must be processed first");
        }
    } catch (const std::exception& error) {
        report_error("Catalog load could not be started", error.what());
    } catch (...) {
        report_error("Catalog load could not be started", "unknown error");
    }
}

void App::apply_catalog_result() {
    try {
        auto result = catalog_tasks_.poll();
        if (!result) {
            return;
        }
        if (!result->error.empty()) {
            report_error("Catalog operation failed", result->error);
            return;
        }
        if (!result->replace_items) {
            ui_error_.clear();
            return;
        }
        std::vector<CatalogViewItem> replacement;
        replacement.reserve(result->items.size());
        for (auto& item : result->items) {
            auto preview = sample_preview(item.log.sample);
            replacement.push_back(CatalogViewItem{std::move(item), std::move(preview)});
        }
        auto replacement_filter = build_filter(replacement);
        catalog_items_.swap(replacement);
        filtered_indices_.swap(replacement_filter);
        selected_log_ = std::min(selected_log_, catalog_items_.empty() ? std::size_t{0} : catalog_items_.size() - 1);
        if (!filtered_indices_.empty() && std::ranges::find(filtered_indices_, selected_log_) == filtered_indices_.end()) {
            selected_log_ = filtered_indices_.front();
        }
        ui_error_.clear();
    } catch (const std::exception& error) {
        report_error("Catalog result could not be applied", error.what());
    } catch (...) {
        report_error("Catalog result could not be applied", "unknown error");
    }
}

void App::apply_editor_analysis_result() {
    try {
        auto result = analysis_tasks_.poll();
        if (!result || result->request_id != editor_analysis_generation_) {
            return;
        }
        if (!result->error.empty()) {
            report_error("Sample log analysis failed", result->error);
            return;
        }
        if (result->analysis) {
            editor_analysis_ = std::move(*result->analysis);
        }
    } catch (const std::exception& error) {
        report_error("Sample log analysis result could not be applied", error.what());
    } catch (...) {
        report_error("Sample log analysis result could not be applied", "unknown error");
    }
}

void App::rebuild_filter() {
    try {
        auto replacement = build_filter(catalog_items_);
        filtered_indices_.swap(replacement);
        if (!filtered_indices_.empty() && std::ranges::find(filtered_indices_, selected_log_) == filtered_indices_.end()) {
            selected_log_ = filtered_indices_.front();
        }
    } catch (const std::exception& error) {
        report_error("Catalog filter could not be rebuilt", error.what());
    } catch (...) {
        report_error("Catalog filter could not be rebuilt", "unknown error");
    }
}

std::vector<std::size_t> App::build_filter(const std::vector<CatalogViewItem>& items) const {
    const auto query = lowercase(search_.data());
    std::vector<std::size_t> result;
    result.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (query.empty() || items[index].item.search_text.find(query) != std::string::npos) {
            result.push_back(index);
        }
    }
    return result;
}

void App::refresh_stats() {
    const auto now = std::chrono::steady_clock::now();
    if (now < next_stats_refresh_) {
        return;
    }
    try {
        auto stats = stress_service_.snapshot();
        auto current_eps = format_number(stats.current_eps);
        auto average_eps = format_number(stats.average_eps);
        auto total_messages = format_number(static_cast<double>(stats.total_messages));
        auto total_bytes = format_bytes(stats.total_bytes);
        cached_stats_ = std::move(stats);
        current_eps_text_ = std::move(current_eps);
        average_eps_text_ = std::move(average_eps);
        total_messages_text_ = std::move(total_messages);
        total_bytes_text_ = std::move(total_bytes);
        next_stats_refresh_ = now + (is_active(cached_stats_.state) ? std::chrono::milliseconds{100} : std::chrono::milliseconds{250});
    } catch (const std::exception& error) {
        report_error("Transmission statistics refresh failed", error.what());
        next_stats_refresh_ = now + std::chrono::seconds{1};
    } catch (...) {
        report_error("Transmission statistics refresh failed", "unknown error");
        next_stats_refresh_ = now + std::chrono::seconds{1};
    }
}

void App::report_error(const std::string_view context, const std::string_view detail) noexcept {
    try {
        ui_error_ = std::format("{}: {}", context, detail);
        logger_.error(ui_error_);
    } catch (...) {
        logger_.error(context);
    }
}

void App::render() {
    apply_catalog_result();
    apply_editor_analysis_result();
    refresh_stats();
    const auto& stats = cached_stats_;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (ImGui::Begin("LogGeneratorRoot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto layout = responsive_layout(ImGui::GetContentRegionAvail().x, ui_scale_);
        render_header(stats, layout);
        ImGui::Spacing();
        render_metrics(stats, layout);
        ImGui::Spacing();
        render_configuration(stats, layout);
        ImGui::Spacing();
        render_catalog_editor();
    }
    ImGui::End();
}

void App::render_header(const domain::TransmissionStats& stats, const ResponsiveLayout& layout) {
    ImGui::SetWindowFontScale(layout.size == ResponsiveSize::Compact ? 1.25F : 1.45F);
    ImGui::TextUnformatted("LOG GENERATOR");
    ImGui::SetWindowFontScale(1.0F);
    if (layout.inline_header_subtitle) {
        ImGui::SameLine();
    }
    ImGui::TextDisabled("SIEM STRESS TOOL");
    const auto status = state_text(stats.state);
    if (layout.inline_header_status) {
        const float status_width = ImGui::CalcTextSize("● ").x + ImGui::CalcTextSize(status.data(), status.data() + status.size()).x;
        const float right_aligned_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - status_width;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x, right_aligned_x));
    }
    ImGui::TextColored(state_color(stats.state), "● %.*s", static_cast<int>(status.size()), status.data());
}

void App::render_metrics(const domain::TransmissionStats& stats, const ResponsiveLayout& layout) {
    static_cast<void>(stats);
    if (ImGui::BeginTable("metrics", layout.metric_columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableNextColumn();
        metric_card("eps_card", "현재 EPS", current_eps_text_, {0.30F, 0.69F, 1.0F, 1.0F});
        ImGui::TableNextColumn();
        metric_card("avg_card", "평균 EPS", average_eps_text_, {0.64F, 0.55F, 1.0F, 1.0F});
        ImGui::TableNextColumn();
        metric_card("count_card", "총 전송 로그", total_messages_text_, {0.24F, 0.84F, 0.53F, 1.0F});
        ImGui::TableNextColumn();
        metric_card("bytes_card", "총 전송량", total_bytes_text_, {1.0F, 0.68F, 0.25F, 1.0F});
        ImGui::EndTable();
    }
}

void App::render_configuration(const domain::TransmissionStats& stats, const ResponsiveLayout& layout) {
    const bool active = is_active(stats.state);
    ImGui::BeginDisabled(active);
    if (layout.configuration_columns == 2) {
        if (ImGui::BeginTable("configuration", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableNextColumn();
            render_destination_panel();
            ImGui::TableNextColumn();
            render_template_panel(layout);
            ImGui::EndTable();
        }
    } else {
        render_destination_panel();
        ImGui::Spacing();
        render_template_panel(layout);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    const float button_height = ImGui::GetFrameHeight() * 1.5F;
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78F, 0.19F, 0.23F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.92F, 0.25F, 0.29F, 1.0F));
        if (ImGui::Button("전송 중지", ImVec2(-1.0F, button_height))) {
            stress_service_.request_stop();
            next_stats_refresh_ = {};
        }
        ImGui::PopStyleColor(2);
    } else if (ImGui::Button("전송 시작", ImVec2(-1.0F, button_height))) {
        start_test();
    }
    if (!stats.last_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.38F, 0.40F, 1.0F));
        ImGui::TextWrapped("%s", stats.last_error.c_str());
        ImGui::PopStyleColor();
    } else if (!ui_error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.38F, 0.40F, 1.0F));
        ImGui::TextWrapped("%s", ui_error_.c_str());
        ImGui::PopStyleColor();
    } else if (protocol_index_ == static_cast<int>(domain::TransportProtocol::File)) {
        disabled_wrapped_text("FILE은 실행 파일 옆 generated 폴더에 약 1 MiB 단위로 분할 저장합니다.");
    } else {
        disabled_wrapped_text("UDP 통계는 로컬 소켓 전송 완료를 기준으로 집계합니다.");
    }
}

void App::render_destination_panel() {
    ImGui::BeginChild("destination", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);
    ImGui::TextUnformatted("전송 설정");
    ImGui::Separator();
    const char* protocols[]{"UDP", "TCP", "TLS", "FILE"};
    ImGui::TextDisabled("프로토콜");
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::Combo("##protocol", &protocol_index_, protocols, static_cast<int>(std::size(protocols))) && protocol_index_ == static_cast<int>(domain::TransportProtocol::File)) {
        worker_count_ = 1;
    }
    const bool file_protocol = protocol_index_ == static_cast<int>(domain::TransportProtocol::File);
    if (file_protocol) {
        ImGui::TextDisabled("저장 폴더");
        disabled_wrapped_text(generated_directory_text_.c_str());
        disabled_wrapped_text("파일명: yyyyMMdd_HHmmss_SSS.log");
    } else {
        ImGui::TextDisabled("대상 IP / Host");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##host", host_.data(), host_.size());
        ImGui::TextDisabled("Port");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputInt("##port", &port_, 0, 0);
    }
    if (protocol_index_ == static_cast<int>(domain::TransportProtocol::Tcp) || protocol_index_ == static_cast<int>(domain::TransportProtocol::Tls)) {
        const char* framings[]{"Newline", "RFC 6587 Octet Counting"};
        ImGui::TextDisabled("프레이밍");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::Combo("##framing", &framing_index_, framings, static_cast<int>(std::size(framings)));
    }
    if (protocol_index_ == static_cast<int>(domain::TransportProtocol::Tls)) {
        ImGui::TextDisabled("TLS 서버 이름");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##tls_server_name", tls_server_name_.data(), tls_server_name_.size());
        ImGui::Checkbox("서버 인증서 검증", &verify_certificate_);
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("성능");
    ImGui::Separator();
    ImGui::TextDisabled("Worker");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::BeginDisabled(file_protocol);
    ImGui::SliderInt("##worker_count", &worker_count_, 1, 64);
    ImGui::EndDisabled();
    if (file_protocol) {
        ImGui::TextDisabled("FILE은 순차 기록을 위해 단일 writer를 사용합니다.");
    }
    ImGui::TextDisabled("목표 EPS");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputScalar("##target_eps", ImGuiDataType_U64, &target_eps_);
    disabled_wrapped_text("0은 제한 없는 최대 성능 모드입니다.");
    ImGui::EndChild();
}

void App::render_template_panel(const ResponsiveLayout& layout) {
    ImGui::BeginChild("templates", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);
    render_catalog_selector();
    ImGui::Spacing();
    ImGui::TextUnformatted("로그 치환");
    ImGui::Separator();
    ImGui::TextDisabled("src_ip");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##source_ip", source_ip_.data(), source_ip_.size());
    ImGui::TextDisabled("dst_ip");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##destination_ip", destination_ip_.data(), destination_ip_.size());
    ImGui::TextDisabled("날짜/시간 생성 방식");
    const char* timestamp_modes[]{"현재 시간 + 오프셋", "기간 지정"};
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::Combo("##timestamp_mode", &timestamp_mode_index_, timestamp_modes, static_cast<int>(std::size(timestamp_modes)));
    if (timestamp_mode_index_ == static_cast<int>(domain::TimestampGenerationMode::Offset)) {
        render_time_offset(layout.offset_columns);
        disabled_wrapped_text("인식된 타임스탬프를 현재 시각과 오프셋 기준으로 생성합니다.");
    } else {
        render_time_range();
        disabled_wrapped_text("시작일 00:00:00부터 종료일 23:59:59까지 이벤트마다 1초씩 진행하고 범위 끝에서 순환합니다.");
    }
    ImGui::EndChild();
}

void App::render_time_offset(const int columns) {
    if (!ImGui::BeginTable("time_offset", columns, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }
    const char* signs[]{"+", "-"};
    ImGui::TableNextColumn();
    ImGui::TextDisabled("방향");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::Combo("##offset_sign", &offset_sign_index_, signs, static_cast<int>(std::size(signs)));
    ImGui::TableNextColumn();
    ImGui::TextDisabled("일");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputInt("##offset_days", &offset_days_, 0, 0);
    ImGui::TableNextColumn();
    ImGui::TextDisabled("시간");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputInt("##offset_hours", &offset_hours_, 0, 0);
    ImGui::TableNextColumn();
    ImGui::TextDisabled("분");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputInt("##offset_minutes", &offset_minutes_, 0, 0);
    ImGui::EndTable();
}

void App::render_time_range() {
    if (!ImGui::BeginTable("time_range", 2, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }
    ImGui::TableNextColumn();
    ImGui::TextDisabled("시작일 (yyyy-MM-dd)");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##range_start", range_start_.data(), range_start_.size());
    ImGui::TableNextColumn();
    ImGui::TextDisabled("종료일 (yyyy-MM-dd)");
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##range_end", range_end_.data(), range_end_.size());
    ImGui::EndTable();
}

void App::render_catalog_selector() {
    if (ImGui::BeginTable("catalog_header", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("catalog_summary", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("catalog_reload", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("샘플 로그");
        ImGui::SameLine();
        ImGui::TextDisabled("%zu개", catalog_items_.size());
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(catalog_tasks_.busy());
        if (ImGui::SmallButton("새로고침")) {
            request_catalog_load();
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    if (catalog_tasks_.busy()) {
        ImGui::TextColored(ImVec4(0.30F, 0.69F, 1.0F, 1.0F), "JSON 카탈로그 처리 중...");
    }
    ImGui::BeginDisabled(catalog_tasks_.busy());
    if (ImGui::BeginTable("catalog_actions", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        if (ImGui::Button("추가", ImVec2(-1.0F, 0.0F))) {
            open_new_catalog_editor();
        }
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(catalog_items_.empty());
        if (ImGui::Button("수정", ImVec2(-1.0F, 0.0F))) {
            open_selected_catalog_editor();
        }
        ImGui::TableNextColumn();
        if (ImGui::Button("삭제", ImVec2(-1.0F, 0.0F))) {
            delete_index_ = visible_catalog_index();
            delete_popup_requested_ = true;
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputTextWithHint("##search", "이름 / 본문 / 개인정보 범주 검색", search_.data(), search_.size())) {
        rebuild_filter();
    }
    ImGui::Checkbox("검색 결과 전체 순환", &rotate_filtered_);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu개", filtered_indices_.size());
    if (!rotate_filtered_ && !catalog_items_.empty()) {
        const char* preview = catalog_items_[visible_catalog_index()].item.log.name.c_str();
        ImGui::TextDisabled("로그 선택");
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("##log_selection", preview)) {
            for (const auto index : filtered_indices_) {
                const bool selected = index == selected_log_;
                if (ImGui::Selectable(catalog_items_[index].item.log.name.c_str(), selected)) {
                    selected_log_ = index;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    if (filtered_indices_.empty()) {
        disabled_wrapped_text("검색 조건에 맞는 샘플 로그가 없습니다.");
    }
    if (!catalog_items_.empty()) {
        const auto preview_index = visible_catalog_index();
        disabled_wrapped_text(catalog_items_[preview_index].preview.c_str());
        const auto& analysis = catalog_items_[preview_index].item.analysis;
        ImGui::TextDisabled("자동 인식: 시간 토큰 %zu개 | src_ip %zu개 | dst_ip %zu개", analysis.timestamp_count, analysis.source_ip_count, analysis.destination_ip_count);
        ImGui::TextDisabled("개인정보 익명화 토큰 %zu개", analysis.privacy_token_count);
        if (!analysis.timestamp_styles.empty()) {
            ImGui::TextDisabled("날짜 포맷:");
            ImGui::SameLine();
            for (std::size_t index = 0; index < analysis.timestamp_styles.size(); ++index) {
                if (index > 0) {
                    ImGui::SameLine(0.0F, 4.0F);
                    ImGui::TextDisabled("/");
                    ImGui::SameLine(0.0F, 4.0F);
                }
                const auto name = timestamp_style_text(analysis.timestamp_styles[index]);
                ImGui::TextColored(ImVec4(0.30F, 0.69F, 1.0F, 1.0F), "%.*s", static_cast<int>(name.size()), name.data());
            }
        }
    }
}

void App::render_catalog_editor() {
    if (editor_analysis_pending_ && !analysis_tasks_.busy() && std::chrono::steady_clock::now() >= editor_analysis_due_) {
        analyze_editor_sample();
    }
    if (editor_popup_requested_) {
        ImGui::OpenPopup("샘플 로그 편집");
        editor_popup_requested_ = false;
    }
    if (delete_popup_requested_) {
        ImGui::OpenPopup("샘플 로그 삭제");
        delete_popup_requested_ = false;
    }

    const auto* viewport = ImGui::GetMainViewport();
    const ImVec2 editor_size{
        std::min(viewport->WorkSize.x * 0.88F, 900.0F * ui_scale_),
        std::min(viewport->WorkSize.y * 0.88F, 720.0F * ui_scale_)};
    ImGui::SetNextWindowSize(editor_size, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("샘플 로그 편집", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(editor_is_new_ ? "새 샘플 로그" : "샘플 로그 수정");
        ImGui::Separator();
        ImGui::TextDisabled("이름");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##editor_name", &editor_name_);
        ImGui::TextDisabled("샘플 로그");
        const float editor_height = std::max(180.0F * ui_scale_, ImGui::GetContentRegionAvail().y * 0.48F);
        if (ImGui::InputTextMultiline("##editor_sample", &editor_sample_, ImVec2(-1.0F, editor_height), ImGuiInputTextFlags_AllowTabInput)) {
            ++editor_analysis_generation_;
            editor_analysis_pending_ = true;
            editor_analysis_due_ = std::chrono::steady_clock::now() + std::chrono::milliseconds{120};
        }
        ImGui::TextUnformatted("자동 파싱 결과");
        ImGui::Separator();
        ImGui::Text("시간 토큰 %zu개 | src_ip %zu개 | dst_ip %zu개", editor_analysis_.timestamp_count, editor_analysis_.source_ip_count, editor_analysis_.destination_ip_count);
        ImGui::Text("개인정보 익명화 토큰 %zu개", editor_analysis_.privacy_token_count);
        if (editor_analysis_pending_ || analysis_tasks_.busy()) {
            ImGui::SameLine();
            ImGui::TextDisabled("분석 대기...");
        }
        if (editor_analysis_.timestamp_styles.empty()) {
            ImGui::TextColored(ImVec4(1.0F, 0.72F, 0.25F, 1.0F), "인식된 날짜 포맷이 없습니다.");
        } else {
            for (const auto style : editor_analysis_.timestamp_styles) {
                const auto name = timestamp_style_text(style);
                ImGui::BulletText("%.*s", static_cast<int>(name.size()), name.data());
            }
        }
        ImGui::BeginDisabled(editor_name_.empty() || editor_sample_.empty() || catalog_tasks_.busy());
        if (ImGui::Button("저장", ImVec2(140.0F * ui_scale_, 0.0F))) {
            if (save_catalog_editor()) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(140.0F * ui_scale_, 0.0F))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(460.0F * ui_scale_, 0.0F), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("샘플 로그 삭제", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        if (delete_index_ < catalog_items_.size()) {
            ImGui::TextWrapped("'%s' 샘플 로그를 삭제하시겠습니까?", catalog_items_[delete_index_].item.log.name.c_str());
        }
        ImGui::TextColored(ImVec4(1.0F, 0.38F, 0.40F, 1.0F), "JSON 파일에서도 삭제됩니다.");
        if (ImGui::Button("삭제", ImVec2(120.0F * ui_scale_, 0.0F))) {
            if (delete_selected_catalog_item()) {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(120.0F * ui_scale_, 0.0F))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::open_new_catalog_editor() {
    editor_is_new_ = true;
    editor_index_ = catalog_items_.size();
    editor_name_.clear();
    editor_sample_.clear();
    editor_analysis_ = {};
    editor_analysis_pending_ = false;
    ++editor_analysis_generation_;
    editor_popup_requested_ = true;
}

void App::open_selected_catalog_editor() {
    if (catalog_items_.empty()) {
        return;
    }
    try {
        const auto selected = visible_catalog_index();
        auto name = catalog_items_[selected].item.log.name;
        auto sample = catalog_items_[selected].item.log.sample;
        auto analysis = catalog_items_[selected].item.analysis;
        editor_is_new_ = false;
        editor_index_ = selected;
        editor_name_ = std::move(name);
        editor_sample_ = std::move(sample);
        editor_analysis_ = std::move(analysis);
        editor_analysis_pending_ = false;
        ++editor_analysis_generation_;
        editor_popup_requested_ = true;
    } catch (const std::exception& error) {
        report_error("Sample log editor could not be opened", error.what());
    } catch (...) {
        report_error("Sample log editor could not be opened", "unknown error");
    }
}

bool App::save_catalog_editor() {
    if (editor_name_.empty() || editor_sample_.empty()) {
        return false;
    }
    try {
        auto replacement = catalog_items_;
        std::size_t replacement_index = editor_index_;
        if (editor_is_new_) {
            const auto existing = catalog_snapshot();
            auto entry = catalog_service_.create(existing, editor_name_, editor_sample_);
            auto preview = sample_preview(entry.log.sample);
            replacement.push_back(CatalogViewItem{std::move(entry), std::move(preview)});
            replacement_index = replacement.size() - 1;
        } else if (editor_index_ < replacement.size()) {
            auto entry = catalog_service_.update(catalog_items_[editor_index_].item.log, editor_name_, editor_sample_);
            auto preview = sample_preview(entry.log.sample);
            replacement[editor_index_] = CatalogViewItem{std::move(entry), std::move(preview)};
        } else {
            report_error("Sample log save failed", "the selected catalog item no longer exists");
            return false;
        }
        auto replacement_filter = build_filter(replacement);
        auto saved_sample = replacement[replacement_index].item.log.sample;
        auto saved_analysis = replacement[replacement_index].item.analysis;
        if (!catalog_tasks_.request_save(catalog_snapshot(replacement))) {
            report_error("Sample log save failed", "another catalog result must be processed first");
            return false;
        }
        catalog_items_.swap(replacement);
        filtered_indices_.swap(replacement_filter);
        selected_log_ = replacement_index;
        editor_sample_ = std::move(saved_sample);
        editor_analysis_ = std::move(saved_analysis);
        ++editor_analysis_generation_;
        editor_analysis_pending_ = false;
        return true;
    } catch (const std::exception& error) {
        report_error("Sample log save failed", error.what());
    } catch (...) {
        report_error("Sample log save failed", "unknown error");
    }
    return false;
}

bool App::delete_selected_catalog_item() {
    if (delete_index_ >= catalog_items_.size()) {
        report_error("Sample log deletion failed", "the selected catalog item no longer exists");
        return false;
    }
    try {
        auto replacement = catalog_items_;
        replacement.erase(replacement.begin() + static_cast<std::ptrdiff_t>(delete_index_));
        auto replacement_filter = build_filter(replacement);
        if (!catalog_tasks_.request_save(catalog_snapshot(replacement))) {
            report_error("Sample log deletion failed", "another catalog result must be processed first");
            return false;
        }
        catalog_items_.swap(replacement);
        filtered_indices_.swap(replacement_filter);
        selected_log_ = catalog_items_.empty() ? 0 : std::min(delete_index_, catalog_items_.size() - 1);
        if (!filtered_indices_.empty() && std::ranges::find(filtered_indices_, selected_log_) == filtered_indices_.end()) {
            selected_log_ = filtered_indices_.front();
        }
        return true;
    } catch (const std::exception& error) {
        report_error("Sample log deletion failed", error.what());
    } catch (...) {
        report_error("Sample log deletion failed", "unknown error");
    }
    return false;
}

void App::analyze_editor_sample() {
    if (editor_sample_.empty()) {
        editor_analysis_ = {};
        editor_analysis_pending_ = false;
        return;
    }
    try {
        if (analysis_tasks_.request_analyze(editor_sample_, editor_analysis_generation_)) {
            editor_analysis_pending_ = false;
        }
    } catch (const std::exception& error) {
        report_error("Sample log analysis could not be started", error.what());
    } catch (...) {
        report_error("Sample log analysis could not be started", "unknown error");
    }
}

std::size_t App::visible_catalog_index() const noexcept {
    if (catalog_items_.empty()) {
        return 0;
    }
    if (rotate_filtered_ && !filtered_indices_.empty()) {
        return filtered_indices_.front();
    }
    return std::min(selected_log_, catalog_items_.size() - 1);
}

std::vector<domain::LogTemplate> App::catalog_snapshot() const {
    return catalog_snapshot(catalog_items_);
}

std::vector<domain::LogTemplate> App::catalog_snapshot(const std::vector<CatalogViewItem>& items) const {
    std::vector<domain::LogTemplate> result;
    result.reserve(items.size());
    for (const auto& item : items) {
        result.push_back(item.item.log);
    }
    return result;
}

void App::start_test() {
    try {
        if (catalog_items_.empty()) {
            throw std::invalid_argument("전송할 샘플 로그가 없습니다.");
        }
        const auto protocol = static_cast<domain::TransportProtocol>(protocol_index_);
        if (protocol != domain::TransportProtocol::File && (host_[0] == '\0' || port_ < 1 || port_ > 65'535)) {
            throw std::invalid_argument("대상 IP/Host와 유효한 Port를 입력하세요.");
        }
        domain::GeneratorConfig config;
        config.endpoint.protocol = protocol;
        config.endpoint.host = host_.data();
        config.endpoint.port = static_cast<std::uint16_t>(port_);
        config.endpoint.framing = static_cast<domain::StreamFraming>(framing_index_);
        config.endpoint.tls_server_name = tls_server_name_.data();
        config.endpoint.verify_certificate = verify_certificate_;
        config.source_ip = source_ip_.data();
        config.destination_ip = destination_ip_.data();
        config.timestamp_generation.mode = static_cast<domain::TimestampGenerationMode>(timestamp_mode_index_);
        if (config.timestamp_generation.mode == domain::TimestampGenerationMode::Offset) {
            config.timestamp_generation.offset.negative = offset_sign_index_ == 1;
            const auto magnitude = [](const int value) {
                const auto wide = static_cast<std::int64_t>(value);
                return wide < 0 ? -wide : wide;
            };
            config.timestamp_generation.offset.days = static_cast<int>(std::clamp<std::int64_t>(magnitude(offset_days_), 0, 3650));
            config.timestamp_generation.offset.hours = static_cast<int>(std::clamp<std::int64_t>(magnitude(offset_hours_), 0, 23));
            config.timestamp_generation.offset.minutes = static_cast<int>(std::clamp<std::int64_t>(magnitude(offset_minutes_), 0, 59));
        } else {
            const auto range_start = parse_iso_date(range_start_.data());
            const auto range_end = parse_iso_date(range_end_.data());
            if (!range_start || !range_end) {
                throw std::invalid_argument("기간은 yyyy-MM-dd 형식의 유효한 날짜여야 합니다.");
            }
            if (*range_start > *range_end) {
                throw std::invalid_argument("기간 시작일은 종료일보다 늦을 수 없습니다.");
            }
            config.timestamp_generation.range.start = std::chrono::time_point_cast<std::chrono::seconds>(*range_start);
            config.timestamp_generation.range.end = std::chrono::time_point_cast<std::chrono::seconds>(*range_end + std::chrono::days{1}) - std::chrono::seconds{1};
        }
        config.worker_count = static_cast<std::uint32_t>(std::clamp(worker_count_, 1, 64));
        config.target_eps = target_eps_;
        if (rotate_filtered_) {
            config.templates.reserve(filtered_indices_.size());
            for (const auto index : filtered_indices_) {
                if (index >= catalog_items_.size()) {
                    throw std::runtime_error("Catalog filter contains an invalid item index");
                }
                config.templates.push_back(catalog_items_[index].item.log);
            }
        } else {
            config.templates.push_back(catalog_items_[visible_catalog_index()].item.log);
        }
        if (config.templates.empty()) {
            throw std::invalid_argument("검색 조건에 맞는 샘플 로그가 없습니다.");
        }
        stress_service_.start(std::move(config));
        next_stats_refresh_ = {};
        ui_error_.clear();
    } catch (const std::exception& error) {
        report_error("Stress test start rejected", error.what());
    } catch (...) {
        report_error("Stress test start rejected", "unknown error");
    }
}

}
