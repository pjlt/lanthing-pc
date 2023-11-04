#include "va_gl_pipeline.h"

namespace lt {

VaGlPipeline::VaGlPipeline(const Params& params) {}

bool VaGlPipeline::init() {
    return false;
}

bool VaGlPipeline::bindTextures(const std::vector<void*>& textures) {}

VideoRenderer::RenderResult VaGlPipeline::render(int64_t frame) {}

void VaGlPipeline::updateCursor(int32_t cursor_id, float x, float y, bool visible) {}

void VaGlPipeline::switchMouseMode(bool absolute) {}

void VaGlPipeline::resetRenderTarget() {}

bool VaGlPipeline::present() {}

bool VaGlPipeline::waitForPipeline(int64_t max_wait_ms) {}

void* VaGlPipeline::hwDevice() {}

void* VaGlPipeline::hwContext() {}

uint32_t VaGlPipeline::displayWidth() {}

uint32_t VaGlPipeline::displayHeight() {}

} // namespace lt
