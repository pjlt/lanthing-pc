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

#include <ltlib/pragma_warning.h>

#include <cstring>
#include <stdexcept>

#include "buffer.h"

WARNING_DISABLE(6297)

namespace rtc2 {

// 放匿名空间里，似乎被检测到不会被外部编译单元引用，直接优化掉了
// 放到rtc2下就没事
void __bco_magic_func() {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    // int8_t i8;
    // int16_t i16;
    // int32_t i32;
    // int64_t i64;
    rtc2::Buffer buf;
    buf.read_big_endian_at(0, u8);
    buf.read_big_endian_at(0, u16);
    buf.read_big_endian_at(0, u32);
    buf.read_big_endian_at(0, u64);
    buf.read_little_endian_at(0, u8);
    buf.read_little_endian_at(0, u16);
    buf.read_little_endian_at(0, u32);
    buf.read_little_endian_at(0, u64);
    buf.write_big_endian_at(0, u8);
    buf.write_big_endian_at(0, u16);
    buf.write_big_endian_at(0, u32);
    buf.write_big_endian_at(0, u64);
    buf.write_little_endian_at(0, u8);
    buf.write_little_endian_at(0, u16);
    buf.write_little_endian_at(0, u32);
    buf.write_little_endian_at(0, u64);
}

namespace {

template <typename T> inline void write_big_endian(uint8_t* buff, const T& value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        buff[i] = static_cast<uint8_t>(value >> ((sizeof(value) - 1 - i) * 8));
    }
}

template <typename T> inline void read_big_endian(const uint8_t* buff, T& value) {
    value = 0;
    for (size_t i = 0; i < sizeof(value); i++) {
        value |= buff[i] << ((sizeof(value) - i - 1) * 8);
    }
}

template <typename T> inline void write_little_endian(uint8_t* buff, const T& value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        buff[i] = static_cast<uint8_t>(value >> (i * 8));
    }
}

template <typename T> inline void read_little_endian(const uint8_t* buff, T& value) {
    value = 0;
    for (size_t i = 0; i < sizeof(value); i++) {
        value |= buff[i] << (i * 8);
    }
}

} // namespace

namespace detail {

BufferBase::BufferBase(size_t size)
    : buffer_({std::vector<uint8_t>(size)}) {}
BufferBase::BufferBase(const std::span<const uint8_t> data)
    : buffer_({data.begin(), data.end()}) {}

BufferBase::BufferBase(std::vector<uint8_t>&& data)
    : buffer_({std::move(data)}) {}

size_t BufferBase::size() const {
    size_t s = 0;
    for (const auto& chunk : buffer_) {
        s += chunk.size();
    }
    return s;
}

void BufferBase::push_back(const std::span<const uint8_t> data, bool new_slice) {
    if (new_slice || buffer_.empty()) {
        buffer_.push_back(std::vector<uint8_t>{data.begin(), data.end()});
    }
    else {
        auto& back = buffer_.back();
        size_t old_size = back.size();
        back.resize(old_size + data.size());
        ::memcpy(back.data() + old_size, data.data(), data.size());
    }
}

void BufferBase::push_back(std::vector<uint8_t>&& data, bool new_slice) {
    if (new_slice || buffer_.empty()) {
        buffer_.push_back(std::move(data));
    }
    else {
        auto& back = buffer_.back();
        size_t old_size = back.size();
        back.resize(old_size + data.size());
        ::memcpy(back.data() + old_size, data.data(), data.size());
    }
}

void BufferBase::insert(size_t index, const std::span<uint8_t> data) {
    size_t buffer_size = size();
    if (index == buffer_size) {
        push_back(data, true);
        return;
    }
    else {
        size_t curr_pos = 0;
        for (auto it = buffer_.begin(); it != buffer_.end(); ++it) {
            if (curr_pos == index) {
                buffer_.insert(it, std::vector<uint8_t>(data.begin(), data.end()));
                return;
            }
            else if (it->size() + curr_pos > index) {
                it->insert(it->begin() + index - curr_pos, data.begin(), data.end());
                return;
            }
            else {
                curr_pos += it->size();
            }
        }
    }
}

void BufferBase::insert(size_t index, std::vector<uint8_t>&& data) {
    size_t buffer_size = size();
    if (index == buffer_size) {
        push_back(data, true);
        return;
    }
    else {
        size_t curr_pos = 0;
        for (auto it = buffer_.begin(); it != buffer_.end(); ++it) {
            if (curr_pos == index) {
                buffer_.insert(it, std::move(data));
                return;
            }
            else if (it->size() + curr_pos > index) {
                // optimize
                it->insert(it->begin() + index - curr_pos, data.begin(), data.end());
                return;
            }
            else {
                curr_pos += it->size();
            }
        }
    }
}

uint8_t& BufferBase::operator[](size_t index) {
    if (buffer_.empty())
        throw std::runtime_error{"Buffer is empty"};
    size_t curr_pos = 0;
    for (auto& chunk : buffer_) {
        if (chunk.size() + curr_pos > index) {
            return chunk[index - curr_pos];
        }
        curr_pos += chunk.size();
    }
    throw std::runtime_error{"Out of index"};
}

std::vector<std::span<uint8_t>> BufferBase::spans(size_t start, size_t end) {
    // TODO: 测试这个函数
    std::vector<std::span<uint8_t>> slices;
    slices.reserve(buffer_.size());
    size_t curr_pos = 0;
    for (auto& chunk : buffer_) {
        if (curr_pos >= end)
            break;
        if (curr_pos == start) {
            slices.emplace_back(chunk.data(), chunk.size());
        }
        else if (curr_pos + chunk.size() <= start) {
            ;
        }
        else if (curr_pos < start && curr_pos + chunk.size() > start) {
            // 比较 end 和 curr_pos + chunk.size()的大小，取小的
            slices.emplace_back(chunk.data() + start - curr_pos,
                                std::min(end - start, chunk.size() - start));
        }
        else if (curr_pos > start && curr_pos + chunk.size() <= end) {
            slices.emplace_back(chunk.data(), chunk.size());
        }
        else if (curr_pos > start && curr_pos + chunk.size() > end) {
            slices.emplace_back(chunk.data(), end - curr_pos);
        }
        else {
            std::abort();
        }
        curr_pos += chunk.size();
    }
    return slices;
}

std::vector<std::span<const uint8_t>> BufferBase::spans_const(size_t start, size_t end) const {
    // TODO: 测试这个函数
    std::vector<std::span<const uint8_t>> slices;
    slices.reserve(buffer_.size());
    size_t curr_pos = 0;
    for (auto& chunk : buffer_) {
        if (curr_pos >= end)
            break;
        if (curr_pos == start) {
            slices.emplace_back(chunk.data(), chunk.size());
        }
        else if (curr_pos + chunk.size() <= start) {
            ;
        }
        else if (curr_pos < start && curr_pos + chunk.size() > start) {
            // 比较 end 和 curr_pos + chunk.size()的大小，取小的
            slices.emplace_back(chunk.data() + start - curr_pos,
                                std::min(end - start, chunk.size() - start));
        }
        else if (curr_pos > start && curr_pos + chunk.size() <= end) {
            slices.emplace_back(chunk.data(), chunk.size());
        }
        else if (curr_pos > start && curr_pos + chunk.size() > end) {
            slices.emplace_back(chunk.data(), end - curr_pos);
        }
        else {
            std::abort();
        }
        curr_pos += chunk.size();
    }
    return slices;
}

// 以下几个函数由使用者保证正确调用，不再做越界判断

template <typename T> inline bool BufferBase::read_big_endian_at(size_t index, T& value) {
    read_big_endian(&operator[](index), value);
    return true;
}

template <typename T> bool BufferBase::write_big_endian_at(size_t index, T value) {
    write_big_endian(&operator[](index), value);
    return true;
}

template <typename T> bool BufferBase::read_little_endian_at(size_t index, T& value) {
    read_little_endian(&operator[](index), value);
    return true;
}

template <typename T> bool BufferBase::write_little_endian_at(size_t index, T value) {
    write_little_endian(&operator[](index), value);
    return true;
}

} // namespace detail

Buffer::Buffer()
    : base_(new detail::BufferBase) {}

Buffer::Buffer(size_t size)
    : base_(new detail::BufferBase{size}) {}

Buffer::Buffer(const std::span<const uint8_t> data)
    : base_(new detail::BufferBase{data}) {}

Buffer::Buffer(std::vector<uint8_t>&& data)
    : base_(new detail::BufferBase{std::move(data)}) {}

Buffer::Buffer(size_t start, size_t end, std::shared_ptr<detail::BufferBase> base)
    : start_(start)
    , end_(end)
    , base_(base) {}

size_t Buffer::size() const {
    if (is_subbuf()) {
        return end_ - start_;
    }
    else {
        return base_->size();
    }
}

bool Buffer::is_subbuf() const {
    return not(start_ == 0 && end_ == std::numeric_limits<decltype(end_)>::max());
}

Buffer Buffer::subbuf(size_t start, size_t count) {
    return Buffer{start, start + count, base_};
}

void Buffer::push_back(const std::span<const uint8_t> data, bool new_slice) {
    if (is_subbuf()) {
        throw std::runtime_error{"Unsupported function"};
    }
    base_->push_back(data, new_slice);
}

void Buffer::push_back(std::vector<uint8_t>&& data, bool new_slice) {
    if (is_subbuf()) {
        throw std::runtime_error{"Unsupported function"};
    }
    base_->push_back(std::move(data), new_slice);
}

void Buffer::insert(size_t index, const std::span<uint8_t> data) {
    if (is_subbuf()) {
        throw std::runtime_error{"Unsupported function"};
    }
    base_->insert(index, data);
}

void Buffer::insert(size_t index, std::vector<uint8_t>&& data) {
    if (is_subbuf()) {
        throw std::runtime_error{"Unsupported function"};
    }
    base_->insert(index, std::move(data));
}

uint8_t& Buffer::operator[](size_t index) {
    if (is_subbuf() && index >= (end_ - start_)) {
        throw std::runtime_error{"Out of index"};
    }
    return base_->operator[](index + start_);
}

const uint8_t& Buffer::operator[](size_t index) const {
    if (is_subbuf() && index >= (end_ - start_)) {
        throw std::runtime_error{"Out of index"};
    }
    return base_->operator[](index + start_);
}

std::vector<std::span<uint8_t>> Buffer::spans() {
    return base_->spans(start_, end_);
}

const std::vector<std::span<const uint8_t>> Buffer::spans() const {
    return base_->spans_const(start_, end_);
}

template <typename T> bool rtc2::Buffer::read_big_endian_at(size_t index, T& value) {
    return base_->read_big_endian_at(index + start_, value);
}

template <typename T> bool rtc2::Buffer::write_big_endian_at(size_t index, T value) {
    return base_->write_big_endian_at(index + start_, value);
}

template <typename T> bool rtc2::Buffer::read_little_endian_at(size_t index, T& value) {
    return base_->read_little_endian_at(index + start_, value);
}

template <typename T> bool rtc2::Buffer::write_little_endian_at(size_t index, T value) {
    return base_->write_little_endian_at(index + start_, value);
}

WARNING_ENABLE(6297)

} // namespace rtc2