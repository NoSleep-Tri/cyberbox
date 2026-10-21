#pragma once
#ifndef CYBERBOX_DX11_DEVICE_HPP
#define CYBERBOX_DX11_DEVICE_HPP

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <string>

namespace graphics {

class DX11Device {
public:
    DX11Device();
    ~DX11Device();

    // Initialize D3D11 hardware device & DXGI SwapChain for HWND
    bool initialize(HWND hwnd, UINT width, UINT height);
    void cleanup();

    // Resize DXGI SwapChain on WM_SIZE
    bool resize(UINT width, UINT height);

    // Present frame (syncInterval: 1 = VSync 60Hz/120Hz/144Hz, 0 = immediate)
    void present(UINT syncInterval = 1);

    // Upload 32-bit BGRA pixels from GDI/Direct2D backbuffer directly into VRAM
    void update_from_bgra_pixels(const void* pixels, UINT rowPitch, UINT width, UINT height);

    // Get backbuffer DXGI Surface for Direct2D render target creation
    IDXGISurface* get_backbuffer_surface();

    ID3D11Device* get_d3d_device() const { return d3d_device_; }
    ID3D11DeviceContext* get_d3d_context() const { return d3d_context_; }
    IDXGISwapChain1* get_swap_chain() const { return swap_chain_; }

    UINT get_width() const { return width_; }
    UINT get_height() const { return height_; }
    const std::string& get_gpu_name() const { return gpu_name_; }
    D3D_FEATURE_LEVEL get_feature_level() const { return feature_level_; }

private:
    HWND hwnd_ = NULL;
    UINT width_ = 0;
    UINT height_ = 0;

    ID3D11Device* d3d_device_ = nullptr;
    ID3D11DeviceContext* d3d_context_ = nullptr;
    IDXGISwapChain1* swap_chain_ = nullptr;
    ID3D11Texture2D* back_buffer_ = nullptr;
    IDXGISurface* back_buffer_surface_ = nullptr;

    std::string gpu_name_;
    D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_11_0;

    void release_buffers();
    bool create_buffers();
};

} // namespace graphics

#endif // CYBERBOX_DX11_DEVICE_HPP
