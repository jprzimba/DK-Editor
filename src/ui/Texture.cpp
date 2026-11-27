#include "ui/Texture.h"

namespace dk {

bool Texture::ensure(ID3D11Device* device, int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (tex_ && width == width_ && height == height_) return true;
    release();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    if (FAILED(device->CreateTexture2D(&desc, nullptr, &tex_))) {
        release();
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(tex_, &srvDesc, &srv_))) {
        release();
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

bool Texture::upload(ID3D11DeviceContext* ctx, const std::vector<u32>& rgba, int width,
                     int height) {
    if (!tex_ || !ctx || width != width_ || height != height_) return false;
    if (rgba.size() < static_cast<std::size_t>(width) * height) return false;

    D3D11_BOX box{};
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = static_cast<UINT>(width);
    box.bottom = static_cast<UINT>(height);
    box.back = 1;
    ctx->UpdateSubresource(tex_, 0, &box, rgba.data(), static_cast<UINT>(width) * 4, 0);
    return true;
}

bool Texture::uploadRegion(ID3D11DeviceContext* ctx, const std::vector<u32>& rgba, int width,
                           int x, int y, int w, int h) {
    if (!tex_ || !ctx || width != width_) return false;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > width_ || y + h > height_) return false;
    if (rgba.size() < static_cast<std::size_t>(width) * height_) return false;
    const u32* base = rgba.data() + static_cast<std::size_t>(y) * width + x;
    D3D11_BOX box{};
    box.left = static_cast<UINT>(x);
    box.top = static_cast<UINT>(y);
    box.front = 0;
    box.right = static_cast<UINT>(x + w);
    box.bottom = static_cast<UINT>(y + h);
    box.back = 1;
    ctx->UpdateSubresource(tex_, 0, &box, base, static_cast<UINT>(width) * 4, 0);
    return true;
}

void Texture::release() {
    if (srv_) {
        srv_->Release();
        srv_ = nullptr;
    }
    if (tex_) {
        tex_->Release();
        tex_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
}

} // namespace dk
