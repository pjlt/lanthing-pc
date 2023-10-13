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

#pragma once
#include <cstdint>

#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <ltlib/ltlib.h>

namespace ltlib {

class LT_API Settings {
public:
    enum class Storage {
        Toml,
        Sqlite,
    };

public:
    virtual ~Settings() = default;
    static std::unique_ptr<Settings> create(Storage type);
    static std::unique_ptr<Settings> createWithPathForTest(Storage type, const std::string& path);
    virtual Storage type() const = 0;
    virtual void setBoolean(const std::string& key, bool value) = 0;
    virtual auto getBoolean(const std::string& key) -> std::optional<bool> = 0;
    virtual void setInteger(const std::string& key, int64_t value) = 0;
    virtual auto getInteger(const std::string& key) -> std::optional<int64_t> = 0;
    virtual void setString(const std::string& key, const std::string& value) = 0;
    virtual auto getString(const std::string& key) -> std::optional<std::string> = 0;
    virtual auto getUpdateTime(const std::string& key) -> std::optional<int64_t> = 0;

protected:
    virtual bool init() = 0;
};

} // namespace ltlib