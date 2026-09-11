// src/presentation/privacy_policy_launcher.hpp
#pragma once

#include <memory>
#include <string_view>

namespace loggen::presentation {

enum class PrivacyPolicyLaunchStatus {
    Idle,
    Opening,
    Opened,
    Failed
};

class PrivacyPolicyLauncher final {
public:
    PrivacyPolicyLauncher();
    ~PrivacyPolicyLauncher();

    PrivacyPolicyLauncher(const PrivacyPolicyLauncher&) = delete;
    PrivacyPolicyLauncher& operator=(const PrivacyPolicyLauncher&) = delete;

    void launch(std::string_view url);
    [[nodiscard]] PrivacyPolicyLaunchStatus poll() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
