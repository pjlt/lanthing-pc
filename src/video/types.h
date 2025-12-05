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
#include <vector>

#include <string>

namespace lt {

enum class VaType {
    D3D11,
    VAAPI,
    VTB,
};

// https://www.itu.int/rec/T-REC-H.265-202407-I/en
enum class ColorPrimaries {
    Undefined = 0,
    BT709 = 1,
    Unspecified = 2,
    Reserved = 3,
    BT470M = 4,
    BT470BG = 5,
    BT601 = 6,
    ST240M = 7, // same as BT601
    Film = 8,
    BT2020 = 9,
};
enum class TransferCharacteristics {
    Undefined = 0,
    BT709 = 1,
    Unspecified = 2,
    Reserved = 3,
    BT470M = 4,
    BT470BG = 5,
    BT601 = 6,
    ST240M = 7, // same as BT601 ??
    Linear = 8,
    Log100 = 9,
    LogSqrt = 10,
    IEC61966_2_4 = 11,
    BT1361 = 12,
    SRGB = 13,
    BT2020_10bit = 14,
    BT2020_12bit = 15,
};
enum class ColorMatrix {
    Identity = 0,
    BT709 = 1,
    Unspecified = 2,
    Reserved = 3,
    FCC = 4,
    BT470BG = 5,
    BT601 = 6,
    ST240M = 7, // same as BT601 ??
    WhatIsThis = 8,
    BT2020_NCL = 9, // non-constant luminance
    BT2020_CL = 10, // constant luminance
};

inline ::std::string toString(enum ColorMatrix matrix) {
    switch (matrix) {
    case ColorMatrix::Identity:
        return "Identity";
    case ColorMatrix::BT709:
        return "BT709";
    case ColorMatrix::Unspecified:
        return "Unspecified";
    case ColorMatrix::Reserved:
        return "Reserved";
    case ColorMatrix::FCC:
        return "FCC";
    case ColorMatrix::BT470BG:
        return "BT470BG";
    case ColorMatrix::BT601:
        return "BT601";
    case ColorMatrix::ST240M:
        return "ST240M";
    case ColorMatrix::WhatIsThis:
        return "WhatIsThis";
    case ColorMatrix::BT2020_NCL:
        return "BT2020_NCL";
    case ColorMatrix::BT2020_CL:
        return "BT2020_CL";
    default:
        return "ColorMatrix_" + std::to_string((int)matrix);
    }
}

// 将TransferCharacteristics转换为字符串
inline ::std::string toString(enum TransferCharacteristics tc) {
    switch (tc) {
    case TransferCharacteristics::Undefined:
        return "Undefined";
    case TransferCharacteristics::BT709:
        return "BT709";
    case TransferCharacteristics::Unspecified:
        return "Unspecified";
    case TransferCharacteristics::Reserved:
        return "Reserved";
    case TransferCharacteristics::BT470M:
        return "BT470M";
    case TransferCharacteristics::BT470BG:
        return "BT470BG";
    case TransferCharacteristics::BT601:
        return "BT601";
    case TransferCharacteristics::ST240M:
        return "ST240M";
    case TransferCharacteristics::Linear:
        return "Linear";
    case TransferCharacteristics::Log100:
        return "Log100";
    case TransferCharacteristics::LogSqrt:
        return "LogSqrt";
    case TransferCharacteristics::IEC61966_2_4:
        return "IEC61966_2_4";
    case TransferCharacteristics::BT1361:
        return "BT1361";
    case TransferCharacteristics::SRGB:
        return "SRGB";
    case TransferCharacteristics::BT2020_10bit:
        return "BT2020_10bit";
    case TransferCharacteristics::BT2020_12bit:
        return "BT2020_12bit";
    default:
        return "TransferCharacteristics_" + std::to_string((int)tc);
    }
}

// 将ColorPrimaries转换为字符串
inline ::std::string toString(enum ColorPrimaries cp) {
    switch (cp) {
    case ColorPrimaries::Undefined:
        return "Undefined";
    case ColorPrimaries::BT709:
        return "BT709";
    case ColorPrimaries::Unspecified:
        return "Unspecified";
    case ColorPrimaries::Reserved:
        return "Reserved";
    case ColorPrimaries::BT470M:
        return "BT470M";
    case ColorPrimaries::BT470BG:
        return "BT470BG";
    case ColorPrimaries::BT601:
        return "BT601";
    case ColorPrimaries::ST240M:
        return "ST240M";
    case ColorPrimaries::Film:
        return "Film";
    case ColorPrimaries::BT2020:
        return "BT2020";
    default:
        return "ColorPrimaries_" + std::to_string((int)cp);
    }
}

} // namespace lt
