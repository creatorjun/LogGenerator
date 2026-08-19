// src/presentation/cli_app.cpp
#include "presentation/cli_app.hpp"

#include "domain/sample_id.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <utility>

namespace loggen::presentation {
namespace {

volatile std::sig_atomic_t signal_stop_requested = 0;

void stop_signal_handler(int) {
    signal_stop_requested = 1;
}

class SignalGuard final {
public:
    SignalGuard()
        : interrupt_(std::signal(SIGINT, stop_signal_handler)), terminate_(std::signal(SIGTERM, stop_signal_handler)) {
        signal_stop_requested = 0;
    }

    ~SignalGuard() {
        std::signal(SIGINT, interrupt_);
        std::signal(SIGTERM, terminate_);
    }

    SignalGuard(const SignalGuard&) = delete;
    SignalGuard& operator=(const SignalGuard&) = delete;

private:
    using Handler = void (*)(int);
    Handler interrupt_;
    Handler terminate_;
};

std::string lowercase(std::string_view value) {
    std::string result{value};
    std::ranges::transform(result, result.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

template <typename Value>
Value parse_unsigned(const std::string_view text, const std::string_view option) {
    unsigned long long parsed = 0;
    const auto conversion = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (text.empty() || conversion.ec != std::errc{} || conversion.ptr != text.data() + text.size() || parsed > static_cast<unsigned long long>(std::numeric_limits<Value>::max())) {
        throw std::invalid_argument(std::string(option) + " 값이 올바른 양의 정수가 아닙니다: " + std::string(text));
    }
    return static_cast<Value>(parsed);
}

long long parse_signed(const std::string_view text, const std::string_view option) {
    long long parsed = 0;
    const auto conversion = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (text.empty() || conversion.ec != std::errc{} || conversion.ptr != text.data() + text.size()) {
        throw std::invalid_argument(std::string(option) + " 값이 올바른 정수가 아닙니다: " + std::string(text));
    }
    return parsed;
}

std::chrono::sys_days parse_date(const std::string_view text, const std::string_view option) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        throw std::invalid_argument(std::string(option) + " 값은 yyyy-MM-dd 형식이어야 합니다");
    }
    int year_value = 0;
    unsigned int month_value = 0;
    unsigned int day_value = 0;
    const auto year_result = std::from_chars(text.data(), text.data() + 4, year_value);
    const auto month_result = std::from_chars(text.data() + 5, text.data() + 7, month_value);
    const auto day_result = std::from_chars(text.data() + 8, text.data() + 10, day_value);
    if (year_result.ec != std::errc{} || year_result.ptr != text.data() + 4 || month_result.ec != std::errc{} || month_result.ptr != text.data() + 7 || day_result.ec != std::errc{} || day_result.ptr != text.data() + 10) {
        throw std::invalid_argument(std::string(option) + " 값에 유효하지 않은 날짜가 있습니다");
    }
    const std::chrono::year_month_day date{std::chrono::year{year_value}, std::chrono::month{month_value}, std::chrono::day{day_value}};
    if (!date.ok()) {
        throw std::invalid_argument(std::string(option) + " 값에 유효하지 않은 날짜가 있습니다");
    }
    return std::chrono::sys_days{date};
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

std::string_view state_name(const domain::GeneratorState state) noexcept {
    switch (state) {
    case domain::GeneratorState::Stopped:
        return "STOPPED";
    case domain::GeneratorState::Connecting:
        return "CONNECTING";
    case domain::GeneratorState::Running:
        return "RUNNING";
    case domain::GeneratorState::Stopping:
        return "STOPPING";
    case domain::GeneratorState::Failed:
        return "FAILED";
    }
    return "UNKNOWN";
}

void print_stats(const domain::TransmissionStats& stats, const bool final) {
    std::cout << (final ? "result" : "status")
              << " state=" << state_name(stats.state)
              << " mode=" << domain::transmission_mode_name(stats.transmission_mode)
              << " workers=" << stats.active_workers
              << " messages=" << stats.total_messages
              << " bytes=" << stats.total_bytes
              << " errors=" << stats.send_errors
              << std::format(" elapsed={:.2f}s current_eps={:.2f} average_eps={:.2f}", stats.elapsed_seconds, stats.current_eps, stats.average_eps);
    if (!stats.status_message.empty()) {
        std::cout << " message=\"" << stats.status_message << '"';
    }
    if (!stats.last_error.empty()) {
        std::cout << " error=\"" << stats.last_error << '"';
    }
    std::cout << '\n';
}

}

CliApp::CliApp(application::ILogCatalogUseCase& catalog_service, application::IStressTestUseCase& stress_service, application::ILogger& logger, std::filesystem::path default_catalog_file)
    : catalog_service_(catalog_service), stress_service_(stress_service), logger_(logger), default_catalog_file_(std::move(default_catalog_file)) {
}

int CliApp::run(const std::span<const std::string_view> arguments, const std::string_view executable_name) {
    try {
        auto options = parse_arguments(arguments);
        if (options.catalog_file.empty()) {
            options.catalog_file = default_catalog_file_;
        }
        switch (options.command) {
        case CliCommand::Help:
            print_help(executable_name);
            return 0;
        case CliCommand::List:
            return list_catalog(options);
        case CliCommand::Run:
            return run_generator(std::move(options));
        }
    } catch (const std::invalid_argument& error) {
        logger_.warning(error.what());
        std::cerr << "인자 오류: " << error.what() << "\n\n";
        print_help(executable_name);
        return 2;
    } catch (const std::exception& error) {
        logger_.error(error.what());
        std::cerr << "실행 오류: " << error.what() << '\n';
        return 1;
    }
    return 1;
}

CliOptions CliApp::parse_arguments(const std::span<const std::string_view> arguments) {
    CliOptions options;
    options.config.endpoint.protocol = domain::TransportProtocol::File;
    std::optional<std::chrono::sys_days> range_start;
    std::optional<std::chrono::sys_days> range_end;
    long long offset_minutes = 0;

    if (arguments.empty()) {
        return options;
    }
    const auto command = lowercase(arguments.front());
    if (command == "--help" || command == "-h" || command == "help") {
        return options;
    }
    if (command == "list") {
        options.command = CliCommand::List;
    } else if (command == "run") {
        options.command = CliCommand::Run;
    } else {
        throw std::invalid_argument("첫 번째 인자는 list 또는 run이어야 합니다");
    }

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const auto option = arguments[index];
        const auto value = [&]() -> std::string_view {
            if (index + 1 >= arguments.size()) {
                throw std::invalid_argument(std::string(option) + " 옵션에 값이 필요합니다");
            }
            return arguments[++index];
        };
        if (option == "--help" || option == "-h") {
            options.command = CliCommand::Help;
        } else if (option == "--catalog") {
            options.catalog_file = value();
        } else if (option == "--all") {
            options.all_samples = true;
        } else if (option == "--sample-id") {
            const auto sample_id = value();
            if (!domain::valid_sample_id(sample_id)) {
                throw std::invalid_argument("--sample-id 값은 숫자로만 구성되어야 합니다");
            }
            options.sample_ids.emplace_back(sample_id);
        } else if (option == "--protocol") {
            const auto protocol = lowercase(value());
            if (protocol == "udp") {
                options.config.endpoint.protocol = domain::TransportProtocol::Udp;
            } else if (protocol == "tcp") {
                options.config.endpoint.protocol = domain::TransportProtocol::Tcp;
            } else if (protocol == "tls") {
                options.config.endpoint.protocol = domain::TransportProtocol::Tls;
            } else if (protocol == "file") {
                options.config.endpoint.protocol = domain::TransportProtocol::File;
            } else {
                throw std::invalid_argument("--protocol 값은 udp, tcp, tls, file 중 하나여야 합니다");
            }
        } else if (option == "--host") {
            options.config.endpoint.host = value();
        } else if (option == "--port") {
            options.config.endpoint.port = parse_unsigned<std::uint16_t>(value(), option);
            if (options.config.endpoint.port == 0) {
                throw std::invalid_argument("--port 값은 1 이상이어야 합니다");
            }
        } else if (option == "--tls-server-name") {
            options.config.endpoint.tls_server_name = value();
        } else if (option == "--insecure") {
            options.config.endpoint.verify_certificate = false;
        } else if (option == "--framing") {
            const auto framing = lowercase(value());
            if (framing == "newline") {
                options.config.endpoint.framing = domain::StreamFraming::Newline;
            } else if (framing == "octet") {
                options.config.endpoint.framing = domain::StreamFraming::OctetCounting;
            } else {
                throw std::invalid_argument("--framing 값은 newline 또는 octet이어야 합니다");
            }
        } else if (option == "--source-ip") {
            options.config.source_ip = value();
        } else if (option == "--destination-ip") {
            options.config.destination_ip = value();
        } else if (option == "--mode") {
            const auto mode = lowercase(value());
            if (mode == "sequential") {
                options.config.transmission_mode = domain::TransmissionMode::Sequential;
            } else if (mode == "parallel") {
                options.config.transmission_mode = domain::TransmissionMode::Parallel;
            } else {
                throw std::invalid_argument("--mode 값은 sequential 또는 parallel이어야 합니다");
            }
        } else if (option == "--eps") {
            options.config.target_eps = parse_unsigned<std::uint64_t>(value(), option);
        } else if (option == "--duration") {
            const auto seconds = parse_unsigned<std::uint64_t>(value(), option);
            if (seconds > static_cast<std::uint64_t>(std::numeric_limits<std::chrono::seconds::rep>::max())) {
                throw std::invalid_argument("--duration 값이 너무 큽니다");
            }
            options.duration = std::chrono::seconds{seconds};
        } else if (option == "--status-interval") {
            const auto seconds = parse_unsigned<std::uint64_t>(value(), option);
            if (seconds == 0 || seconds > static_cast<std::uint64_t>(std::numeric_limits<std::chrono::milliseconds::rep>::max() / 1000)) {
                throw std::invalid_argument("--status-interval 값은 1 이상의 초여야 합니다");
            }
            options.status_interval = std::chrono::milliseconds{seconds * 1000};
        } else if (option == "--quiet") {
            options.quiet = true;
        } else if (option == "--output-dir") {
            options.config.endpoint.file_output_directory = std::string(value());
        } else if (option == "--file-max-mib") {
            const auto mib = parse_unsigned<std::uint64_t>(value(), option);
            constexpr std::uint64_t bytes_per_mib = 1024ULL * 1024ULL;
            if (mib > std::numeric_limits<std::uint64_t>::max() / bytes_per_mib) {
                throw std::invalid_argument("--file-max-mib 값이 너무 큽니다");
            }
            options.config.endpoint.file_max_total_bytes = mib * bytes_per_mib;
        } else if (option == "--file-max-count") {
            options.config.endpoint.file_max_count = parse_unsigned<std::uint32_t>(value(), option);
        } else if (option == "--file-max-duration") {
            const auto seconds = parse_unsigned<std::uint64_t>(value(), option);
            if (seconds > static_cast<std::uint64_t>(std::numeric_limits<std::chrono::milliseconds::rep>::max() / 1000)) {
                throw std::invalid_argument("--file-max-duration 값이 너무 큽니다");
            }
            options.config.endpoint.file_max_duration = std::chrono::milliseconds{seconds * 1000};
        } else if (option == "--offset-minutes") {
            offset_minutes = parse_signed(value(), option);
        } else if (option == "--from") {
            range_start = parse_date(value(), option);
        } else if (option == "--to") {
            range_end = parse_date(value(), option);
        } else {
            throw std::invalid_argument("알 수 없는 옵션: " + std::string(option));
        }
    }

    if (options.command == CliCommand::Run) {
        if (options.all_samples && !options.sample_ids.empty()) {
            throw std::invalid_argument("--all과 --sample-id는 함께 사용할 수 없습니다");
        }
        if (!options.all_samples && options.sample_ids.empty()) {
            throw std::invalid_argument("--all 또는 --sample-id를 지정해야 합니다");
        }
    } else if (options.command == CliCommand::List && (options.all_samples || !options.sample_ids.empty())) {
        throw std::invalid_argument("--all과 --sample-id는 run 명령에서만 사용할 수 있습니다");
    }
    if (!valid_ipv4(options.config.source_ip) || !valid_ipv4(options.config.destination_ip)) {
        throw std::invalid_argument("--source-ip와 --destination-ip는 유효한 IPv4 주소여야 합니다");
    }
    if (range_start.has_value() != range_end.has_value()) {
        throw std::invalid_argument("--from과 --to는 함께 지정해야 합니다");
    }
    if (range_start && range_end) {
        if (*range_start > *range_end) {
            throw std::invalid_argument("--from 날짜는 --to 날짜보다 늦을 수 없습니다");
        }
        options.config.timestamp_generation.mode = domain::TimestampGenerationMode::Range;
        options.config.timestamp_generation.range.start = std::chrono::time_point_cast<std::chrono::seconds>(*range_start);
        options.config.timestamp_generation.range.end = std::chrono::time_point_cast<std::chrono::seconds>(*range_end + std::chrono::days{1}) - std::chrono::seconds{1};
    } else {
        constexpr long long minutes_per_day = 24LL * 60LL;
        const auto magnitude = offset_minutes < 0 ? static_cast<unsigned long long>(-(offset_minutes + 1)) + 1ULL : static_cast<unsigned long long>(offset_minutes);
        if (magnitude / minutes_per_day > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument("--offset-minutes 값이 너무 큽니다");
        }
        options.config.timestamp_generation.offset.negative = offset_minutes < 0;
        options.config.timestamp_generation.offset.days = static_cast<int>(magnitude / minutes_per_day);
        options.config.timestamp_generation.offset.hours = static_cast<int>((magnitude % minutes_per_day) / 60ULL);
        options.config.timestamp_generation.offset.minutes = static_cast<int>(magnitude % 60ULL);
    }
    return options;
}

void CliApp::print_help(const std::string_view executable_name) {
    std::cout
        << executable_name << " CLI\n\n"
        << "사용법:\n"
        << "  " << executable_name << " list [--catalog PATH]\n"
        << "  " << executable_name << " run (--all | --sample-id ID...) [OPTIONS]\n\n"
        << "주요 옵션:\n"
        << "  --protocol file|udp|tcp|tls   기본값: file\n"
        << "  --all                         전체 샘플을 전역 Round-Robin으로 순환\n"
        << "  --sample-id ID                반복 지정 가능, --all과 동시 사용 불가\n"
        << "  --host HOST --port PORT       네트워크 목적지\n"
        << "  --mode sequential|parallel    순차 또는 자동 최적화 병렬 전송, 기본값: parallel\n"
        << "  --eps N                       목표 EPS, 0은 무제한\n"
        << "  --duration SECONDS            실행 시간, 0은 Ctrl+C까지 실행\n"
        << "  --output-dir PATH             FILE 출력 디렉터리\n"
        << "  --file-max-count N            FILE 생성 개수 제한\n"
        << "  --file-max-mib N              FILE 총 생성량 제한\n"
        << "  --file-max-duration SECONDS   FILE 생성 시간 제한\n"
        << "  --framing newline|octet       TCP/TLS 프레이밍\n"
        << "  --tls-server-name NAME        TLS 인증서 서버 이름\n"
        << "  --insecure                    TLS 인증서 검증 비활성화\n"
        << "  --source-ip IP --destination-ip IP\n"
        << "  --offset-minutes N            현재 시각 오프셋\n"
        << "  --from yyyy-MM-dd --to yyyy-MM-dd\n"
        << "  --catalog PATH --status-interval SECONDS --quiet\n\n"
        << "예시:\n"
        << "  " << executable_name << " list\n"
        << "  " << executable_name << " run --protocol file --sample-id 0001 --file-max-count 100 --output-dir ./generated\n"
        << "  " << executable_name << " run --all --protocol udp --host 192.0.2.10 --port 514 --mode parallel --eps 1000 --duration 60\n";
}

int CliApp::list_catalog(const CliOptions& options) {
    const auto items = catalog_service_.load(options.catalog_file);
    for (const auto& item : items) {
        std::cout << item.id << '\t' << item.name << '\n';
    }
    std::cout << "총 " << items.size() << "개 샘플\n";
    return 0;
}

int CliApp::run_generator(CliOptions options) {
    auto catalog = catalog_service_.load(options.catalog_file);
    if (options.all_samples) {
        options.config.templates = std::move(catalog);
    } else {
        std::unordered_set<std::string> requested(options.sample_ids.begin(), options.sample_ids.end());
        for (auto& item : catalog) {
            if (requested.erase(item.id) > 0) {
                options.config.templates.push_back(std::move(item));
            }
        }
        if (!requested.empty()) {
            throw std::invalid_argument("카탈로그에 없는 sample-id: " + *requested.begin());
        }
    }
    if (options.config.templates.empty()) {
        throw std::invalid_argument("실행할 샘플 로그가 없습니다");
    }
    if (options.config.endpoint.protocol != domain::TransportProtocol::File && (options.config.endpoint.host.empty() || options.config.endpoint.port == 0)) {
        throw std::invalid_argument("네트워크 프로토콜에는 --host와 --port가 필요합니다");
    }

    SignalGuard signals;
    stress_service_.start(std::move(options.config));
    const auto started = std::chrono::steady_clock::now();
    auto next_status = started + options.status_interval;
    bool stop_requested = false;
    int result = 0;
    while (true) {
        const auto stats = stress_service_.snapshot();
        const auto now = std::chrono::steady_clock::now();
        if (stats.state == domain::GeneratorState::Failed) {
            result = 1;
            break;
        }
        if (stats.state == domain::GeneratorState::Stopped && stop_requested) {
            break;
        }
        if (!stats.status_message.empty() && stats.state == domain::GeneratorState::Stopped) {
            break;
        }
        if (!stop_requested && (signal_stop_requested != 0 || (options.duration.count() > 0 && now - started >= options.duration))) {
            stress_service_.request_stop();
            stop_requested = true;
        }
        if (!options.quiet && now >= next_status) {
            print_stats(stats, false);
            next_status = now + options.status_interval;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    stress_service_.stop();
    const auto final_stats = stress_service_.snapshot();
    print_stats(final_stats, true);
    return result;
}

}
