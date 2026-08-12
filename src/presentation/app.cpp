// src/presentation/app.cpp
#include "presentation/app.hpp"

#include "domain/generator_config.hpp"
#include "presentation/ui_theme.hpp"

#include <dwmapi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);

namespace loggen::presentation {
namespace {

bool is_active(const domain::GeneratorState state) {
    return state == domain::GeneratorState::Connecting || state == domain::GeneratorState::Running || state == domain::GeneratorState::Stopping;
}

std::string state_text(const domain::GeneratorState state) {
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075F, 0.092F, 0.125F, 1.0F));
    ImGui::BeginChild(id, ImVec2(0.0F, 86.0F), ImGuiChildFlags_Borders);
    ImGui::TextColored(accent, "%s", label);
    ImGui::SetWindowFontScale(1.35F);
    ImGui::TextUnformatted(value.c_str());
    ImGui::SetWindowFontScale(1.0F);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

bool valid_ipv4(const std::string_view value) {
    int segments = 0;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find('.', start);
        const auto part = value.substr(start, end == std::string_view::npos ? value.size() - start : end - start);
        if (part.empty() || part.size() > 3) {
            return false;
        }
        int number = -1;
        const auto conversion = std::from_chars(part.data(), part.data() + part.size(), number);
        if (conversion.ec != std::errc{} || conversion.ptr != part.data() + part.size() || number < 0 || number > 255) {
            return false;
        }
        ++segments;
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return segments == 4;
}

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

}

App::App(application::ILogCatalog& catalog, application::ILogger& logger, application::StressTestService& stress_service, std::filesystem::path sample_directory)
    : catalog_(catalog), logger_(logger), stress_service_(stress_service), sample_directory_(std::move(sample_directory)) {
}

App::~App() {
    stress_service_.stop();
    shutdown_imgui();
}

int App::run(const HINSTANCE instance, const int show_command) {
    logger_.info("UI initialization started");
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_CLASSDC;
    window_class.lpfnWndProc = &App::window_procedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"LogGeneratorWindow";
    if (RegisterClassExW(&window_class) == 0) {
        throw std::runtime_error("Window class registration failed");
    }

    RECT bounds{0, 0, 1080, 860};
    AdjustWindowRectEx(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0);
    window_ = CreateWindowExW(0, window_class.lpszClassName, L"LogGenerator", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, this);
    if (window_ == nullptr) {
        UnregisterClassW(window_class.lpszClassName, instance);
        throw std::runtime_error("Window creation failed");
    }
    const BOOL dark_mode = TRUE;
    DwmSetWindowAttribute(window_, 20, &dark_mode, sizeof(dark_mode));
    d3d_.create(window_);
    initialize_imgui();
    load_catalog();
    logger_.info("UI initialization completed");
    ShowWindow(window_, show_command);
    UpdateWindow(window_);

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
    if (IsWindow(window_)) {
        DestroyWindow(window_);
    }
    UnregisterClassW(window_class.lpszClassName, instance);
    logger_.info("UI event loop stopped");
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::window_procedure(const HWND window, const UINT message, const WPARAM word_parameter, const LPARAM long_parameter) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(long_parameter);
        app = static_cast<App*>(creation->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app != nullptr) {
        return app->handle_message(window, message, word_parameter, long_parameter);
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
            try {
                d3d_.resize(static_cast<unsigned int>(LOWORD(long_parameter)), static_cast<unsigned int>(HIWORD(long_parameter)));
            } catch (const std::exception& error) {
                ui_error_ = error.what();
            }
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* information = reinterpret_cast<MINMAXINFO*>(long_parameter);
        information->ptMinTrackSize.x = 900;
        information->ptMinTrackSize.y = 720;
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
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const float scale = static_cast<float>(GetDpiForWindow(window_)) / 96.0F;
    const std::filesystem::path korean_font = L"C:\\Windows\\Fonts\\malgun.ttf";
    if (std::filesystem::exists(korean_font)) {
        io.Fonts->AddFontFromFileTTF(path_to_utf8(korean_font).c_str(), 17.0F * scale, nullptr, io.Fonts->GetGlyphRangesKorean());
    } else {
        io.Fonts->AddFontDefault();
    }
    apply_ui_theme(scale);
    if (!ImGui_ImplWin32_Init(window_) || !ImGui_ImplDX11_Init(d3d_.device(), d3d_.context())) {
        ImGui::DestroyContext();
        throw std::runtime_error("Dear ImGui initialization failed");
    }
    imgui_ready_ = true;
}

void App::shutdown_imgui() noexcept {
    if (!imgui_ready_) {
        return;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    imgui_ready_ = false;
}

void App::load_catalog() {
    try {
        catalog_items_ = catalog_.load(sample_directory_);
        selected_log_ = std::min(selected_log_, catalog_items_.empty() ? std::size_t{0} : catalog_items_.size() - 1);
        ui_error_.clear();
        logger_.info(std::format("Sample log catalog loaded: directory={}, entries={}", path_to_utf8(sample_directory_), catalog_items_.size()));
    } catch (const std::exception& error) {
        catalog_items_.clear();
        ui_error_ = error.what();
        logger_.error(std::format("Sample log catalog load failed: {}", error.what()));
    }
}

void App::render() {
    const auto stats = stress_service_.snapshot();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("LogGeneratorRoot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    render_header(stats);
    ImGui::Dummy(ImVec2(0.0F, 5.0F));
    render_metrics(stats);
    ImGui::Dummy(ImVec2(0.0F, 8.0F));
    render_configuration(stats);
    ImGui::End();
}

void App::render_header(const domain::TransmissionStats& stats) {
    ImGui::SetWindowFontScale(1.45F);
    ImGui::TextUnformatted("LOG GENERATOR");
    ImGui::SetWindowFontScale(1.0F);
    ImGui::SameLine();
    ImGui::TextDisabled("SIEM STRESS TOOL");
    const auto status = state_text(stats.state);
    const auto width = ImGui::CalcTextSize(status.c_str()).x;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - width + ImGui::GetCursorPosX());
    ImGui::TextColored(state_color(stats.state), "● %s", status.c_str());
}

void App::render_metrics(const domain::TransmissionStats& stats) {
    if (ImGui::BeginTable("metrics", 4, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        metric_card("eps_card", "현재 EPS", format_number(stats.current_eps), {0.30F, 0.69F, 1.0F, 1.0F});
        ImGui::TableNextColumn();
        metric_card("avg_card", "평균 EPS", format_number(stats.average_eps), {0.64F, 0.55F, 1.0F, 1.0F});
        ImGui::TableNextColumn();
        metric_card("count_card", "총 전송 로그", format_number(static_cast<double>(stats.total_messages)), {0.24F, 0.84F, 0.53F, 1.0F});
        ImGui::TableNextColumn();
        metric_card("bytes_card", "총 전송량", format_bytes(stats.total_bytes), {1.0F, 0.68F, 0.25F, 1.0F});
        ImGui::EndTable();
    }
}

void App::render_configuration(const domain::TransmissionStats& stats) {
    const bool active = is_active(stats.state);
    ImGui::BeginDisabled(active);
    if (ImGui::BeginTable("configuration", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::BeginChild("destination", ImVec2(0.0F, 410.0F), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted("전송 설정");
        ImGui::Separator();
        const char* protocols[]{"UDP", "TCP", "TLS"};
        ImGui::TextDisabled("프로토콜");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::Combo("##protocol", &protocol_index_, protocols, static_cast<int>(std::size(protocols)));
        ImGui::TextDisabled("대상 IP / Host");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##host", host_.data(), host_.size());
        ImGui::TextDisabled("Port");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputInt("##port", &port_, 0, 0);
        if (protocol_index_ != 0) {
            const char* framings[]{"Newline", "RFC 6587 Octet Counting"};
            ImGui::TextDisabled("프레이밍");
            ImGui::SetNextItemWidth(-1.0F);
            ImGui::Combo("##framing", &framing_index_, framings, static_cast<int>(std::size(framings)));
        }
        if (protocol_index_ == 2) {
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
        ImGui::SliderInt("##worker_count", &worker_count_, 1, 64);
        ImGui::TextDisabled("목표 EPS");
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputScalar("##target_eps", ImGuiDataType_U64, &target_eps_);
        ImGui::TextDisabled("0은 제한 없는 최대 성능 모드입니다.");
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("templates", ImVec2(0.0F, 410.0F), ImGuiChildFlags_Borders);
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
        ImGui::TextDisabled("타임 오프셋");
        const char* signs[]{"+", "-"};
        ImGui::SetNextItemWidth(70.0F);
        ImGui::Combo("##offset_sign", &offset_sign_index_, signs, static_cast<int>(std::size(signs)));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(85.0F);
        ImGui::InputInt("일", &offset_days_, 0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(85.0F);
        ImGui::InputInt("시간", &offset_hours_, 0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(85.0F);
        ImGui::InputInt("분", &offset_minutes_, 0, 0);
        ImGui::TextDisabled("인식된 타임스탬프를 현재 시각 기준으로 생성합니다.");
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    const float button_height = 48.0F;
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78F, 0.19F, 0.23F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.92F, 0.25F, 0.29F, 1.0F));
        if (ImGui::Button("전송 중지", ImVec2(-1.0F, button_height))) {
            stress_service_.stop();
        }
        ImGui::PopStyleColor(2);
    } else if (ImGui::Button("전송 시작", ImVec2(-1.0F, button_height))) {
        start_test();
    }
    if (!stats.last_error.empty()) {
        ImGui::TextColored({1.0F, 0.38F, 0.40F, 1.0F}, "%s", stats.last_error.c_str());
    } else if (!ui_error_.empty()) {
        ImGui::TextColored({1.0F, 0.38F, 0.40F, 1.0F}, "%s", ui_error_.c_str());
    } else {
        ImGui::TextDisabled("UDP 통계는 로컬 소켓 전송 완료를 기준으로 집계합니다.");
    }
}

void App::render_catalog_selector() {
    ImGui::TextUnformatted("샘플 로그");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu개", catalog_items_.size());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 48.0F + ImGui::GetCursorPosX());
    if (ImGui::SmallButton("새로고침")) {
        load_catalog();
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##search", "장비 / Parser 검색", search_.data(), search_.size());
    const auto filtered = filtered_indices();
    ImGui::Checkbox("검색 결과 전체 순환", &rotate_filtered_);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu개", filtered.size());
    if (!rotate_filtered_ && !catalog_items_.empty()) {
        const char* preview = catalog_items_[selected_log_].name.c_str();
        ImGui::SetNextItemWidth(-1.0F);
        if (ImGui::BeginCombo("로그 선택", preview)) {
            for (const auto index : filtered) {
                const bool selected = index == selected_log_;
                if (ImGui::Selectable(catalog_items_[index].name.c_str(), selected)) {
                    selected_log_ = index;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    if (!catalog_items_.empty()) {
        const auto preview_index = rotate_filtered_ && !filtered.empty() ? filtered.front() : selected_log_;
        const auto& sample = catalog_items_[preview_index];
        ImGui::TextDisabled("%s", sample.source.c_str());
        const auto length = std::min<std::size_t>(sample.sample.size(), 180);
        std::string preview_text = sample.sample.substr(0, length);
        if (length < sample.sample.size()) {
            preview_text.append("...");
        }
        ImGui::PushTextWrapPos(0.0F);
        ImGui::TextDisabled("%s", preview_text.c_str());
        ImGui::PopTextWrapPos();
    }
}

void App::start_test() {
    try {
        if (catalog_items_.empty()) {
            throw std::invalid_argument("전송할 샘플 로그가 없습니다.");
        }
        if (host_[0] == '\0' || port_ < 1 || port_ > 65'535) {
            throw std::invalid_argument("대상 IP/Host와 유효한 Port를 입력하세요.");
        }
        if (!valid_ipv4(source_ip_.data()) || !valid_ipv4(destination_ip_.data())) {
            throw std::invalid_argument("src_ip와 dst_ip는 유효한 IPv4 주소여야 합니다.");
        }
        domain::GeneratorConfig config;
        config.endpoint.protocol = static_cast<domain::TransportProtocol>(protocol_index_);
        config.endpoint.host = host_.data();
        config.endpoint.port = static_cast<std::uint16_t>(port_);
        config.endpoint.framing = static_cast<domain::StreamFraming>(framing_index_);
        config.endpoint.tls_server_name = tls_server_name_.data();
        config.endpoint.verify_certificate = verify_certificate_;
        config.source_ip = source_ip_.data();
        config.destination_ip = destination_ip_.data();
        config.time_offset.negative = offset_sign_index_ == 1;
        config.time_offset.days = std::clamp(std::abs(offset_days_), 0, 3650);
        config.time_offset.hours = std::clamp(std::abs(offset_hours_), 0, 23);
        config.time_offset.minutes = std::clamp(std::abs(offset_minutes_), 0, 59);
        config.worker_count = static_cast<std::uint32_t>(std::clamp(worker_count_, 1, 64));
        config.target_eps = target_eps_;
        if (rotate_filtered_) {
            for (const auto index : filtered_indices()) {
                config.templates.push_back(catalog_items_[index]);
            }
        } else {
            config.templates.push_back(catalog_items_[selected_log_]);
        }
        if (config.templates.empty()) {
            throw std::invalid_argument("검색 조건에 맞는 샘플 로그가 없습니다.");
        }
        stress_service_.start(std::move(config));
        ui_error_.clear();
    } catch (const std::exception& error) {
        ui_error_ = error.what();
        logger_.warning(std::format("Stress test start rejected: {}", error.what()));
    }
}

std::vector<std::size_t> App::filtered_indices() const {
    std::vector<std::size_t> result;
    const auto query = lowercase(search_.data());
    result.reserve(catalog_items_.size());
    for (std::size_t index = 0; index < catalog_items_.size(); ++index) {
        if (query.empty() || lowercase(catalog_items_[index].name).find(query) != std::string::npos) {
            result.push_back(index);
        }
    }
    return result;
}

}
