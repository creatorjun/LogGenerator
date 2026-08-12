// src/presentation/d3d11_context.cpp
#include "presentation/d3d11_context.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>

namespace loggen::presentation {
namespace {

std::runtime_error d3d_error(const char* action, const HRESULT result) {
    return std::runtime_error(std::format("{} failed (HRESULT 0x{:08X})", action, static_cast<std::uint32_t>(result)));
}

}

void D3d11Context::create(const HWND window) {
    if (window == nullptr) {
        throw std::invalid_argument("Direct3D requires a valid window");
    }
    reset();
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferDesc.RefreshRate.Numerator = 0;
    description.BufferDesc.RefreshRate.Denominator = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.SampleDesc.Quality = 0;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    const std::array<D3D_FEATURE_LEVEL, 2> feature_levels{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected_level{};
    HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels.data(), static_cast<UINT>(feature_levels.size()), D3D11_SDK_VERSION, &description, &swap_chain_, &device_, &selected_level, &context_);
    if (FAILED(result)) {
        reset();
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, feature_levels.data(), static_cast<UINT>(feature_levels.size()), D3D11_SDK_VERSION, &description, &swap_chain_, &device_, &selected_level, &context_);
    }
    if (FAILED(result)) {
        reset();
        throw d3d_error("Direct3D 11 initialization", result);
    }
    Microsoft::WRL::ComPtr<IDXGIDevice1> low_latency_device;
    if (SUCCEEDED(device_.As(&low_latency_device))) {
        static_cast<void>(low_latency_device->SetMaximumFrameLatency(1));
    }
    create_render_target();
}

void D3d11Context::resize(const unsigned int width, const unsigned int height) {
    if (!swap_chain_ || width == 0 || height == 0) {
        return;
    }
    render_target_.Reset();
    const HRESULT result = swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        try {
            create_render_target();
        } catch (...) {
        }
        throw d3d_error("Direct3D resize", result);
    }
    create_render_target();
}

void D3d11Context::clear(const float red, const float green, const float blue, const float alpha) {
    if (!renderable()) {
        throw std::runtime_error("Direct3D render target is unavailable");
    }
    const float color[4]{red, green, blue, alpha};
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), color);
}

void D3d11Context::present() {
    if (!swap_chain_) {
        throw std::runtime_error("Direct3D swap chain is unavailable");
    }
    const HRESULT result = swap_chain_->Present(1, 0);
    if (FAILED(result)) {
        throw d3d_error("Direct3D present", result);
    }
}

void D3d11Context::reset() noexcept {
    render_target_.Reset();
    if (context_) {
        context_->ClearState();
        context_->Flush();
    }
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();
}

ID3D11Device* D3d11Context::device() const noexcept {
    return device_.Get();
}

ID3D11DeviceContext* D3d11Context::context() const noexcept {
    return context_.Get();
}

bool D3d11Context::renderable() const noexcept {
    return context_ != nullptr && swap_chain_ != nullptr && render_target_ != nullptr;
}

void D3d11Context::create_render_target() {
    if (!device_ || !swap_chain_) {
        throw std::runtime_error("Direct3D device or swap chain is unavailable");
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    const HRESULT buffer_result = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(buffer_result)) {
        throw d3d_error("Direct3D back buffer acquisition", buffer_result);
    }
    const HRESULT target_result = device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_);
    if (FAILED(target_result)) {
        throw d3d_error("Direct3D render target creation", target_result);
    }
}

}
