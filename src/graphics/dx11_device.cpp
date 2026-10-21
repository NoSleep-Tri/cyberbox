#include "dx11_device.hpp"
#include <iostream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace graphics {

DX11Device::DX11Device() = default;

DX11Device::~DX11Device() {
    cleanup();
}

bool DX11Device::initialize(HWND hwnd, UINT width, UINT height) {
    cleanup();
    hwnd_ = hwnd;
    width_ = max(1u, width);
    height_ = max(1u, height);

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    // Uncomment for DX11 Debug Layer if SDK debug layer is installed:
    // creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    // 1. Create Direct3D 11 Hardware Device
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &d3d_device_,
        &feature_level_,
        &d3d_context_
    );

    // Fallback to WARP high-performance software rasterizer if GPU hardware driver fails
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &d3d_device_,
            &feature_level_,
            &d3d_context_
        );
        if (FAILED(hr)) return false;
    }

    // 2. Query DXGI Factory from D3D11 Device
    IDXGIDevice1* dxgiDevice = nullptr;
    hr = d3d_device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr)) return false;

    // Set maximum frame latency to 1 for lowest possible input latency (120 FPS responsive)
    dxgiDevice->SetMaximumFrameLatency(1);

    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();
    if (FAILED(hr)) return false;

    // Extract GPU Adapter Name
    DXGI_ADAPTER_DESC adapterDesc;
    if (SUCCEEDED(dxgiAdapter->GetDesc(&adapterDesc))) {
        char nameBuf[256] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, nameBuf, sizeof(nameBuf), NULL, NULL);
        gpu_name_ = nameBuf;
    }

    IDXGIFactory2* dxgiFactory2 = nullptr;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory2));
    dxgiAdapter->Release();
    if (FAILED(hr)) return false;

    // 3. Configure Modern Flip-Model SwapChain
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = width_;
    scDesc.Height = height_;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.Stereo = FALSE;
    scDesc.SampleDesc.Count = 1;
    scDesc.SampleDesc.Quality = 0;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2; // Double buffering
    scDesc.Scaling = DXGI_SCALING_NONE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Windows 10/11 high-performance flip model
    scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    scDesc.Flags = 0;

    hr = dxgiFactory2->CreateSwapChainForHwnd(
        d3d_device_,
        hwnd_,
        &scDesc,
        nullptr,
        nullptr,
        &swap_chain_
    );

    // Fallback to FLIP_SEQUENTIAL if FLIP_DISCARD is not supported
    if (FAILED(hr)) {
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = dxgiFactory2->CreateSwapChainForHwnd(
            d3d_device_,
            hwnd_,
            &scDesc,
            nullptr,
            nullptr,
            &swap_chain_
        );
    }

    if (SUCCEEDED(hr)) {
        // Prevent DXGI from intercepting Alt+Enter
        dxgiFactory2->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
    }
    dxgiFactory2->Release();

    if (FAILED(hr)) return false;

    // 4. Create BackBuffer surfaces
    return create_buffers();
}

void DX11Device::release_buffers() {
    if (back_buffer_surface_) {
        back_buffer_surface_->Release();
        back_buffer_surface_ = nullptr;
    }
    if (back_buffer_) {
        back_buffer_->Release();
        back_buffer_ = nullptr;
    }
}

bool DX11Device::create_buffers() {
    if (!swap_chain_) return false;
    release_buffers();

    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer_));
    if (FAILED(hr) || !back_buffer_) return false;

    hr = back_buffer_->QueryInterface(IID_PPV_ARGS(&back_buffer_surface_));
    return SUCCEEDED(hr);
}

bool DX11Device::resize(UINT width, UINT height) {
    if (!swap_chain_) return false;
    width_ = max(1u, width);
    height_ = max(1u, height);

    release_buffers();
    HRESULT hr = swap_chain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return false;

    return create_buffers();
}

void DX11Device::present(UINT syncInterval) {
    if (!swap_chain_) return;
    // syncInterval = 1: VSync locked (60Hz, 120Hz, 144Hz, 240Hz depending on monitor)
    // syncInterval = 0: Tear-free immediate presentation with DXGI Flip Model
    swap_chain_->Present(syncInterval, 0);
}

IDXGISurface* DX11Device::get_backbuffer_surface() {
    return back_buffer_surface_;
}

void DX11Device::update_from_bgra_pixels(const void* pixels, UINT rowPitch, UINT width, UINT height) {
    if (!d3d_context_ || !back_buffer_ || !pixels) return;
    if (width == width_ && height == height_) {
        d3d_context_->UpdateSubresource(back_buffer_, 0, nullptr, pixels, rowPitch, 0);
    } else {
        D3D11_BOX box;
        box.left = 0;
        box.right = min(width, width_);
        box.top = 0;
        box.bottom = min(height, height_);
        box.front = 0;
        box.back = 1;
        d3d_context_->UpdateSubresource(back_buffer_, 0, &box, pixels, rowPitch, 0);
    }
}

void DX11Device::cleanup() {
    release_buffers();
    if (swap_chain_) {
        swap_chain_->Release();
        swap_chain_ = nullptr;
    }
    if (d3d_context_) {
        d3d_context_->ClearState();
        d3d_context_->Flush();
        d3d_context_->Release();
        d3d_context_ = nullptr;
    }
    if (d3d_device_) {
        d3d_device_->Release();
        d3d_device_ = nullptr;
    }
}

} // namespace graphics
