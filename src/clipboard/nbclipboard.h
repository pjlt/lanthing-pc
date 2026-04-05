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

#pragma once
#include <stdint.h>

struct NbClipboard;

enum class NbClipLogLevel { Debug, Info, Warn, Error };

typedef void (*NbLogPrint)(NbClipLogLevel, const char* format, ...);
typedef void (*NbClipSendFilePullRequest)(void*, int64_t device_id, uint32_t file_seq);
typedef void (*NbClipSendFileChunk)(void*, int64_t device_id, uint32_t file_seq, uint32_t seq,
                                    const uint8_t* data, uint16_t size);
typedef void (*NbClipSendFileChunkAck)(void*, int64_t device_id, uint32_t, uint64_t);
typedef uint32_t (*NbClipUpdateLocalFileInfo)(NbClipboard* ctx, const char* fullpath,
                                              const wchar_t* wfullpath, uint64_t size);
typedef int32_t (*NbClipUpdateRemoteFileInfo)(NbClipboard* ctx, int64_t device_id,
                                              uint32_t file_seq, const char* filename,
                                              const wchar_t* wfilename, uint64_t size);
typedef void (*NbClipOnFileChunk)(NbClipboard* ctx, int64_t device_id, uint32_t file_seq,
                                  uint32_t chunk_seq, const uint8_t* data, uint16_t size);
typedef void (*NbClipOnFileChunkAck)(NbClipboard* ctx, uint32_t file_seq, uint32_t chunk_seq);
typedef void (*NbClipOnFilePullRequest)(NbClipboard*, int64_t device_id, uint32_t);

// Must be called in single thread
struct NbClipboard {
    struct Params {
        void* userdata;
        NbLogPrint log_print;
        NbClipSendFilePullRequest send_file_pull_request;
        NbClipSendFileChunk send_file_chunk;
        NbClipSendFileChunkAck send_file_chunk_ack;
    };
    NbClipUpdateLocalFileInfo update_local_file_info;
    NbClipUpdateRemoteFileInfo update_remote_file_info;
    NbClipOnFileChunk on_file_chunk;
    NbClipOnFileChunkAck on_file_chunk_ack;
    NbClipOnFilePullRequest on_file_pull_request;
};

NbClipboard* createNbClipboard(const NbClipboard::Params* params);
void destroyNbClipboard(NbClipboard* ptr);
