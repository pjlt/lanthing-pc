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

#include "worker.h"
#include "worker_check_decode.h"
#if LT_WINDOWS
#include "worker_check_dupl.h"
#include "worker_setting.h"
#include "worker_streaming.h"
#endif // LT_WINDOWS

#include <lt_constants.h>
#include <ltlib/logging.h>

namespace lt {

namespace worker {

std::tuple<std::unique_ptr<Worker>, int32_t>
Worker::create(std::map<std::string, std::string> options) {
    auto iter = options.find("-action");
    if (iter == options.cend()) {
        LOG(ERR) << "Invalid worker parameters: no worker action";
        return {nullptr, kExitCodeInvalidParameters};
    }
#if LT_WINDOWS
    else if (iter->second == "streaming") {
        LOG(INFO) << "Launch worker for streaming";
        return WorkerStreaming::create(options);
    }
    else if (iter->second == "setting") {
        LOG(INFO) << "Launch worker for setting";
        return WorkerSetting::create(options);
    }
    else if (iter->second == "check_dupl") {
        LOG(INFO) << "Launch worker for setting";
        return WorkerCheckDupl::create(options);
    }
#endif // LT_WINDOWS
    else if (iter->second == "check_decode") {
        LOG(INFO) << "Launch worker for check_decode";
        return WorkerCheckDecode::create(options);
    }
    else {
        LOG(ERR) << "Unkonwn worker action: " << iter->second;
        return {nullptr, kExitCodeInvalidParameters};
    }
}

} // namespace worker

} // namespace lt