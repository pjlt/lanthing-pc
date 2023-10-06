/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2021-2023 Zhennan Tu <zhennan.tu@gmail.com>
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
#include <list>
#include <memory>
#include <span>
#include <vector>

namespace rtc2 {

namespace detail {

class BufferBase {

public:
    BufferBase() = default;
    BufferBase(size_t size);
    BufferBase(const std::span<const uint8_t> data);
    BufferBase(std::vector<uint8_t>&& data);

    BufferBase(const BufferBase&) = delete;
    BufferBase(BufferBase&&) = delete;
    BufferBase& operator=(const BufferBase&) = delete;
    BufferBase& operator=(BufferBase&&) = delete;

    size_t size() const;
    void push_back(const std::span<const uint8_t> data, bool new_slice = false);
    void push_back(std::vector<uint8_t>&& data, bool new_slice = false);
    void insert(size_t index, const std::span<uint8_t> data);
    void insert(size_t index, std::vector<uint8_t>&& data);
    uint8_t& operator[](size_t index);
    std::vector<std::span<uint8_t>> spans(size_t start, size_t end);
    std::vector<std::span<const uint8_t>> spans_const(size_t start, size_t end) const;

    template <typename T> bool read_big_endian_at(size_t index, T& value);

    template <typename T> bool write_big_endian_at(size_t index, T value);

    template <typename T> bool read_little_endian_at(size_t index, T& value);

    template <typename T> bool write_little_endian_at(size_t index, T value);

private:
    std::list<std::vector<uint8_t>> buffer_;
};

} // namespace detail

class Buffer {
public:
    Buffer();
    explicit Buffer(size_t size);
    Buffer(const std::span<const uint8_t> data);
    Buffer(std::vector<uint8_t>&& data);

    size_t size() const;
    bool is_subbuf() const;
    Buffer subbuf(size_t start, size_t count);
    void push_back(const std::span<const uint8_t> data, bool new_slice = false);
    void push_back(std::vector<uint8_t>&& data, bool new_slice = false);
    void insert(size_t index, const std::span<uint8_t> data);
    void insert(size_t index, std::vector<uint8_t>&& data);
    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;
    std::vector<std::span<uint8_t>> spans();
    const std::vector<std::span<const uint8_t>> spans() const;

    template <typename T> bool read_big_endian_at(size_t index, T& value);

    template <typename T> bool write_big_endian_at(size_t index, T value);

    template <typename T> bool read_little_endian_at(size_t index, T& value);

    template <typename T> bool write_little_endian_at(size_t index, T value);

private:
    Buffer(size_t start, size_t end, std::shared_ptr<detail::BufferBase> base);

private:
    size_t start_ = 0;
    size_t end_ = std::numeric_limits<size_t>::max();
    std::shared_ptr<detail::BufferBase> base_;
};

} // namespace rtc2