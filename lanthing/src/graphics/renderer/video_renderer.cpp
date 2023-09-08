#include "video_renderer.h"

#include "d3d11_pipeline.h"

namespace lt {

std::unique_ptr<VideoRenderer> lt::VideoRenderer::create(const Params& params) {
#if LT_WINDOWS
    D3D11Pipeline::Params d3d11_params{};
    d3d11_params.hwnd = (HWND)params.window;
    d3d11_params.luid = params.device;
    d3d11_params.widht = params.video_width;
    d3d11_params.height = params.video_height;
    d3d11_params.align = params.align;
    auto renderer = std::make_unique<D3D11Pipeline>(d3d11_params);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
#else
    return nullptr;
#endif //
}

} // namespace lt
