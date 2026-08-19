// src/presentation/d3d11_context.cpp
#include "presentation/d3d11_context.hpp"

#include <array>
#include <stdexcept>

namespace loggen::presentation {

void D3d11Context::create(const HWND window) {
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
    const HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, feature_levels.data(), static_cast<UINT>(feature_levels.size()), D3D11_SDK_VERSION, &description, &swap_chain_, &device_, &selected_level, &context_);
    if (FAILED(result)) {
        throw std::runtime_error("Direct3D 11 initialization failed");
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
        throw std::runtime_error("Direct3D resize failed");
    }
    create_render_target();
}

void D3d11Context::clear(const float red, const float green, const float blue, const float alpha) {
    const float color[4]{red, green, blue, alpha};
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), color);
}

void D3d11Context::present(const unsigned int sync_interval) {
    swap_chain_->Present(sync_interval, 0);
}

ID3D11Device* D3d11Context::device() const noexcept {
    return device_.Get();
}

ID3D11DeviceContext* D3d11Context::context() const noexcept {
    return context_.Get();
}

void D3d11Context::create_render_target() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
        throw std::runtime_error("Direct3D back buffer acquisition failed");
    }
    if (FAILED(device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_))) {
        throw std::runtime_error("Direct3D render target creation failed");
    }
}

}
