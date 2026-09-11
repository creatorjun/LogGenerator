// src/presentation/privacy_policy_launcher.cpp
#include "presentation/privacy_policy_launcher.hpp"

#include <Windows.h>
#include <roapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <limits>
#include <stdexcept>
#include <string>

namespace loggen::presentation {
namespace {

class WindowsRuntimeApartment final {
public:
    WindowsRuntimeApartment() {
        const auto result = RoInitialize(RO_INIT_SINGLETHREADED);
        if (result != RPC_E_CHANGED_MODE) {
            winrt::check_hresult(result);
            initialized_ = true;
        }
    }

    ~WindowsRuntimeApartment() {
        if (initialized_) {
            RoUninitialize();
        }
    }

    WindowsRuntimeApartment(const WindowsRuntimeApartment&) = delete;
    WindowsRuntimeApartment& operator=(const WindowsRuntimeApartment&) = delete;

private:
    bool initialized_{false};
};

std::wstring to_wide_policy_url(const std::string_view url) {
    if (!url.starts_with("https://") || url.size() <= 8 || url.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("Privacy policy URL must use HTTPS");
    }
    for (const char value : url) {
        if (static_cast<unsigned char>(value) <= 0x20 || value == '\\' || value == '"') {
            throw std::invalid_argument("Privacy policy URL contains invalid characters");
        }
    }
    const int source_size = static_cast<int>(url.size());
    const int wide_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.data(), source_size, nullptr, 0);
    if (wide_size <= 0) {
        throw std::invalid_argument("Privacy policy URL must contain valid UTF-8");
    }
    std::wstring wide_url(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.data(), source_size, wide_url.data(), wide_size) != wide_size) {
        throw std::invalid_argument("Privacy policy URL could not be decoded");
    }
    return wide_url;
}

}

struct PrivacyPolicyLauncher::Impl {
    WindowsRuntimeApartment apartment;
    winrt::Windows::Foundation::IAsyncOperation<bool> operation{nullptr};
    PrivacyPolicyLaunchStatus status{PrivacyPolicyLaunchStatus::Idle};

    ~Impl() {
        if (operation) {
            try {
                operation.Cancel();
            } catch (...) {
            }
        }
    }
};

PrivacyPolicyLauncher::PrivacyPolicyLauncher()
    : impl_(std::make_unique<Impl>()) {
}

PrivacyPolicyLauncher::~PrivacyPolicyLauncher() = default;

void PrivacyPolicyLauncher::launch(const std::string_view url) {
    if (impl_->status == PrivacyPolicyLaunchStatus::Opening) {
        return;
    }
    const winrt::Windows::Foundation::Uri uri{to_wide_policy_url(url)};
    if (uri.SchemeName() != L"https" || uri.Host().empty()) {
        throw std::invalid_argument("Privacy policy URL must identify an HTTPS host");
    }
    impl_->operation = winrt::Windows::System::Launcher::LaunchUriAsync(uri);
    impl_->status = PrivacyPolicyLaunchStatus::Opening;
}

PrivacyPolicyLaunchStatus PrivacyPolicyLauncher::poll() noexcept {
    if (!impl_->operation) {
        return impl_->status;
    }
    try {
        switch (impl_->operation.Status()) {
        case winrt::Windows::Foundation::AsyncStatus::Started:
            return PrivacyPolicyLaunchStatus::Opening;
        case winrt::Windows::Foundation::AsyncStatus::Completed:
            impl_->status = impl_->operation.GetResults() ? PrivacyPolicyLaunchStatus::Opened : PrivacyPolicyLaunchStatus::Failed;
            break;
        case winrt::Windows::Foundation::AsyncStatus::Canceled:
        case winrt::Windows::Foundation::AsyncStatus::Error:
            impl_->status = PrivacyPolicyLaunchStatus::Failed;
            break;
        }
    } catch (...) {
        impl_->status = PrivacyPolicyLaunchStatus::Failed;
    }
    impl_->operation = nullptr;
    return impl_->status;
}

}
