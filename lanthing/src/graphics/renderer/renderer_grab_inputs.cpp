/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "renderer_grab_inputs.h"

#include <mutex>

// 暂时写死SDL
#include <backends/imgui_impl_sdl2.h>

static bool g_imgui_valid = false;
static std::mutex g_mutex;

namespace lt {

bool rendererGrabInputs(void* inputs) {
    std::lock_guard lk{g_mutex};
    if (!g_imgui_valid) {
        return false;
    }
    if (ImGui_ImplSDL2_ProcessEvent(reinterpret_cast<const SDL_Event*>(inputs))) {
        auto& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
            return true;
        }
    }
    return false;
}

void setImGuiValid() {
    std::lock_guard lk{g_mutex};
    g_imgui_valid = true;
}

void setImGuiInvalid() {
    std::lock_guard lk{g_mutex};
    g_imgui_valid = false;
}

} // namespace lt