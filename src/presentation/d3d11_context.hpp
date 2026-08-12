// src/presentation/d3d11_context.hpp
#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace loggen::presentation {

class D3d11Context {
public:
    void create(HWND window);
    void resize(unsigned int width, unsigned int height);
    void clear(float red, float green, float blue, float alpha);
    void present();

    [[nodiscard]] ID3D11Device* device() const noexcept;
    [[nodiscard]] ID3D11DeviceContext* context() const noexcept;

private:
    void create_render_target();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;
};

}
