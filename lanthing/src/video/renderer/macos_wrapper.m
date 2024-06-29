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

#include "macos_wrapper.h"

#import <Cocoa/Cocoa.h>

static void replaceWithGLContentView(void* ns_window)
{
    NSWindow* wnd = (NSWindow*)ns_window;
    CGSize size = [[wnd frame] size];
    // ÄÚ´æÐ¹Â¶?
    NSOpenGLView* view = [[NSOpenGLView alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height)];
    [wnd setContentview:view];
}

static void glMakeCurrent(void* ns_window)
{
    NSWindow* wnd = (NSWindow*)ns_window;
    [[wnd contentView] lockFocus];
}

static void glMakeCurrentEmpty(void* ns_window)
{
    NSWindow* wnd = (NSWindow*)ns_window;
    [[wnd contentView] unlockFocus];
}

void* createMacOSWrapper()
{
    MacOSWrapper* obj = (MacOSWrapper*)malloc(sizeof(MacOSWrapper));
    obj->replaceWithGLContentView = &replaceWithGLContentView;
    return obj;
}

void destroyMacOSWrapper(void* _obj)
{
    MacOSWrapper* obj = (MacOSWrapper*)_obj;
    free(obj);
}