/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024 Zhennan Tu <zhennan.tu@gmail.com>
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

#include "vtb_gl_pipeline_plat.h"

#include <string.h>

#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreAudio/CoreAudio.h>
#import <IOSurface/IOSurface.h>

#include <OpenGL/gl3.h>
#include <OpenGL/glext.h>

void ltMapOpenGLTexture(void* context, uint32_t textures[2], int64_t frame, int32_t width, int32_t height)
{
    NSOpenGLContext* glcontext = (NSOpenGLContext*)context;
    CVPixelBufferRef pixel_buffer = (CVPixelBufferRef)frame;
    IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(pixel_buffer);
    uint32_t formats[2] = {GL_RED, GL_RG}; //???
    for (size_t i = 0; i < 2; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        CGLTexImageIOSurface2D([glcontext CGLContextObj],
                                                       GL_TEXTURE_2D,
                                                       formats[i],
                                                       width / (i + 1),
                                                       height / (i + 1),
                                                       formats[i],
                                                       GL_UNSIGNED_BYTE,
                                                       io_surface,
                                                       i);
    }
}

void ltFlushOpenGLBuffer(void* context)
{
    NSOpenGLContext* glcontext = (NSOpenGLContext*)context;
    [glcontext flushBuffer];
}