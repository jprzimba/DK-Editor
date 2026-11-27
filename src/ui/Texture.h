#pragma once

#include "core/Util.h"

#include <d3d11.h>
#include <imgui.h>

#include <vector>

namespace dk {

class Texture {
public:
    ~Texture() { release(); }
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool ensure(ID3D11Device* device, int width, int height);
    bool upload(ID3D11DeviceContext* ctx, const std::vector<u32>& rgba, int width,
                int height);
    bool uploadRegion(ID3D11DeviceContext* ctx, const std::vector<u32>& rgba, int width,
                      int x, int y, int w, int h);
    void release();

    ImTextureID imguiId() const { return reinterpret_cast<ImTextureID>(srv_); }
    int width() const { return width_; }
    int height() const { return height_; }
    bool valid() const { return srv_ != nullptr; }

private:
    ID3D11Texture2D* tex_ = nullptr;
    ID3D11ShaderResourceView* srv_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

} // namespace dk
