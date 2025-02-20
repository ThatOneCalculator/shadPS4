// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "video_core/amdgpu/pixel_format.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"

namespace Vulkan::LiverpoolToVK {

using DepthBuffer = Liverpool::DepthBuffer;

vk::StencilOp StencilOp(Liverpool::StencilFunc op) {
    switch (op) {
    case Liverpool::StencilFunc::Keep:
        return vk::StencilOp::eKeep;
    case Liverpool::StencilFunc::Zero:
        return vk::StencilOp::eZero;
    case Liverpool::StencilFunc::ReplaceTest:
        return vk::StencilOp::eReplace;
    case Liverpool::StencilFunc::AddClamp:
        return vk::StencilOp::eIncrementAndClamp;
    case Liverpool::StencilFunc::SubClamp:
        return vk::StencilOp::eDecrementAndClamp;
    case Liverpool::StencilFunc::Invert:
        return vk::StencilOp::eInvert;
    case Liverpool::StencilFunc::AddWrap:
        return vk::StencilOp::eIncrementAndWrap;
    case Liverpool::StencilFunc::SubWrap:
        return vk::StencilOp::eDecrementAndWrap;
    case Liverpool::StencilFunc::ReplaceOp:
        return vk::StencilOp::eReplace;
    default:
        UNREACHABLE();
        return vk::StencilOp::eKeep;
    }
}

vk::CompareOp CompareOp(Liverpool::CompareFunc func) {
    switch (func) {
    case Liverpool::CompareFunc::Always:
        return vk::CompareOp::eAlways;
    case Liverpool::CompareFunc::Equal:
        return vk::CompareOp::eEqual;
    case Liverpool::CompareFunc::GreaterEqual:
        return vk::CompareOp::eGreaterOrEqual;
    case Liverpool::CompareFunc::Greater:
        return vk::CompareOp::eGreater;
    case Liverpool::CompareFunc::LessEqual:
        return vk::CompareOp::eLessOrEqual;
    case Liverpool::CompareFunc::Less:
        return vk::CompareOp::eLess;
    case Liverpool::CompareFunc::NotEqual:
        return vk::CompareOp::eNotEqual;
    case Liverpool::CompareFunc::Never:
        return vk::CompareOp::eNever;
    default:
        UNREACHABLE();
        return vk::CompareOp::eAlways;
    }
}

vk::PrimitiveTopology PrimitiveType(Liverpool::PrimitiveType type) {
    switch (type) {
    case Liverpool::PrimitiveType::PointList:
        return vk::PrimitiveTopology::ePointList;
    case Liverpool::PrimitiveType::LineList:
        return vk::PrimitiveTopology::eLineList;
    case Liverpool::PrimitiveType::LineStrip:
        return vk::PrimitiveTopology::eLineStrip;
    case Liverpool::PrimitiveType::TriangleList:
        return vk::PrimitiveTopology::eTriangleList;
    case Liverpool::PrimitiveType::TriangleFan:
        return vk::PrimitiveTopology::eTriangleFan;
    case Liverpool::PrimitiveType::TriangleStrip:
        return vk::PrimitiveTopology::eTriangleStrip;
    case Liverpool::PrimitiveType::AdjLineList:
        return vk::PrimitiveTopology::eLineListWithAdjacency;
    case Liverpool::PrimitiveType::AdjLineStrip:
        return vk::PrimitiveTopology::eLineStripWithAdjacency;
    case Liverpool::PrimitiveType::AdjTriangleList:
        return vk::PrimitiveTopology::eTriangleListWithAdjacency;
    case Liverpool::PrimitiveType::AdjTriangleStrip:
        return vk::PrimitiveTopology::eTriangleStripWithAdjacency;
    case Liverpool::PrimitiveType::QuadList:
        // Needs to generate index buffer on the fly.
        return vk::PrimitiveTopology::eTriangleList;
    case Liverpool::PrimitiveType::RectList:
        return vk::PrimitiveTopology::eTriangleStrip;
    default:
        UNREACHABLE();
        return vk::PrimitiveTopology::eTriangleList;
    }
}

vk::PolygonMode PolygonMode(Liverpool::PolygonMode mode) {
    switch (mode) {
    case Liverpool::PolygonMode::Point:
        return vk::PolygonMode::ePoint;
    case Liverpool::PolygonMode::Line:
        return vk::PolygonMode::eLine;
    case Liverpool::PolygonMode::Fill:
        return vk::PolygonMode::eFill;
    default:
        UNREACHABLE();
        return vk::PolygonMode::eFill;
    }
}

vk::CullModeFlags CullMode(Liverpool::CullMode mode) {
    switch (mode) {
    case Liverpool::CullMode::None:
        return vk::CullModeFlagBits::eNone;
    case Liverpool::CullMode::Front:
        return vk::CullModeFlagBits::eFront;
    case Liverpool::CullMode::Back:
        return vk::CullModeFlagBits::eBack;
    case Liverpool::CullMode::FrontAndBack:
        return vk::CullModeFlagBits::eFrontAndBack;
    default:
        UNREACHABLE();
        return vk::CullModeFlagBits::eNone;
    }
}

vk::BlendFactor BlendFactor(Liverpool::BlendControl::BlendFactor factor) {
    using BlendFactor = Liverpool::BlendControl::BlendFactor;
    switch (factor) {
    case BlendFactor::Zero:
        return vk::BlendFactor::eZero;
    case BlendFactor::One:
        return vk::BlendFactor::eOne;
    case BlendFactor::SrcColor:
        return vk::BlendFactor::eSrcColor;
    case BlendFactor::OneMinusSrcColor:
        return vk::BlendFactor::eOneMinusSrcColor;
    case BlendFactor::SrcAlpha:
        return vk::BlendFactor::eSrcAlpha;
    case BlendFactor::OneMinusSrcAlpha:
        return vk::BlendFactor::eOneMinusSrcAlpha;
    case BlendFactor::DstAlpha:
        return vk::BlendFactor::eDstAlpha;
    case BlendFactor::OneMinusDstAlpha:
        return vk::BlendFactor::eOneMinusDstAlpha;
    case BlendFactor::DstColor:
        return vk::BlendFactor::eDstColor;
    case BlendFactor::OneMinusDstColor:
        return vk::BlendFactor::eOneMinusDstColor;
    case BlendFactor::SrcAlphaSaturate:
        return vk::BlendFactor::eSrcAlphaSaturate;
    case BlendFactor::ConstantColor:
        return vk::BlendFactor::eConstantColor;
    case BlendFactor::OneMinusConstantColor:
        return vk::BlendFactor::eOneMinusConstantColor;
    case BlendFactor::Src1Color:
        return vk::BlendFactor::eSrc1Color;
    case BlendFactor::InvSrc1Color:
        return vk::BlendFactor::eOneMinusSrc1Color;
    case BlendFactor::Src1Alpha:
        return vk::BlendFactor::eSrc1Alpha;
    case BlendFactor::InvSrc1Alpha:
        return vk::BlendFactor::eOneMinusSrc1Alpha;
    case BlendFactor::ConstantAlpha:
        return vk::BlendFactor::eConstantAlpha;
    case BlendFactor::OneMinusConstantAlpha:
        return vk::BlendFactor::eOneMinusConstantAlpha;
    default:
        UNREACHABLE();
    }
}

vk::BlendOp BlendOp(Liverpool::BlendControl::BlendFunc func) {
    using BlendFunc = Liverpool::BlendControl::BlendFunc;
    switch (func) {
    case BlendFunc::Add:
        return vk::BlendOp::eAdd;
    case BlendFunc::Subtract:
        return vk::BlendOp::eSubtract;
    case BlendFunc::Min:
        return vk::BlendOp::eMin;
    case BlendFunc::Max:
        return vk::BlendOp::eMax;
    case BlendFunc::ReverseSubtract:
        return vk::BlendOp::eReverseSubtract;
    default:
        UNREACHABLE();
    }
}

// https://github.com/chaotic-cx/mesa-mirror/blob/0954afff5/src/amd/vulkan/radv_sampler.c#L21
vk::SamplerAddressMode ClampMode(AmdGpu::ClampMode mode) {
    switch (mode) {
    case AmdGpu::ClampMode::Wrap:
        return vk::SamplerAddressMode::eRepeat;
    case AmdGpu::ClampMode::Mirror:
        return vk::SamplerAddressMode::eMirroredRepeat;
    case AmdGpu::ClampMode::ClampLastTexel:
        return vk::SamplerAddressMode::eClampToEdge;
    case AmdGpu::ClampMode::MirrorOnceLastTexel:
        return vk::SamplerAddressMode::eMirrorClampToEdge;
    case AmdGpu::ClampMode::ClampBorder:
        return vk::SamplerAddressMode::eClampToBorder;
    default:
        UNREACHABLE();
    }
}

vk::CompareOp DepthCompare(AmdGpu::DepthCompare comp) {
    switch (comp) {
    case AmdGpu::DepthCompare::Never:
        return vk::CompareOp::eNever;
    case AmdGpu::DepthCompare::Less:
        return vk::CompareOp::eLess;
    case AmdGpu::DepthCompare::Equal:
        return vk::CompareOp::eEqual;
    case AmdGpu::DepthCompare::LessEqual:
        return vk::CompareOp::eLessOrEqual;
    case AmdGpu::DepthCompare::Greater:
        return vk::CompareOp::eGreater;
    case AmdGpu::DepthCompare::NotEqual:
        return vk::CompareOp::eNotEqual;
    case AmdGpu::DepthCompare::GreaterEqual:
        return vk::CompareOp::eGreaterOrEqual;
    case AmdGpu::DepthCompare::Always:
        return vk::CompareOp::eAlways;
    }
}

vk::Filter Filter(AmdGpu::Filter filter) {
    switch (filter) {
    case AmdGpu::Filter::Point:
    case AmdGpu::Filter::AnisoPoint:
        return vk::Filter::eNearest;
    case AmdGpu::Filter::Bilinear:
    case AmdGpu::Filter::AnisoLinear:
        return vk::Filter::eLinear;
    default:
        UNREACHABLE();
    }
}

vk::SamplerReductionMode FilterMode(AmdGpu::FilterMode mode) {
    switch (mode) {
    case AmdGpu::FilterMode::Blend:
        return vk::SamplerReductionMode::eWeightedAverage;
    case AmdGpu::FilterMode::Min:
        return vk::SamplerReductionMode::eMin;
    case AmdGpu::FilterMode::Max:
        return vk::SamplerReductionMode::eMax;
    default:
        UNREACHABLE();
    }
}

vk::SamplerMipmapMode MipFilter(AmdGpu::MipFilter filter) {
    switch (filter) {
    case AmdGpu::MipFilter::Point:
        return vk::SamplerMipmapMode::eNearest;
    case AmdGpu::MipFilter::Linear:
        return vk::SamplerMipmapMode::eLinear;
    case AmdGpu::MipFilter::None:
        return vk::SamplerMipmapMode::eNearest;
    default:
        UNREACHABLE();
    }
}

vk::BorderColor BorderColor(AmdGpu::BorderColor color) {
    switch (color) {
    case AmdGpu::BorderColor::OpaqueBlack:
        return vk::BorderColor::eFloatOpaqueBlack;
    case AmdGpu::BorderColor::TransparentBlack:
        return vk::BorderColor::eFloatTransparentBlack;
    case AmdGpu::BorderColor::White:
        return vk::BorderColor::eFloatOpaqueWhite;
    case AmdGpu::BorderColor::Custom:
        return vk::BorderColor::eFloatCustomEXT;
    default:
        UNREACHABLE();
    }
}

vk::Format SurfaceFormat(AmdGpu::DataFormat data_format, AmdGpu::NumberFormat num_format) {

    if (data_format == AmdGpu::DataFormat::Format32_32_32_32 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR32G32B32A32Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format32_32_32 &&
        num_format == AmdGpu::NumberFormat::Uint) {
        return vk::Format::eR32G32B32Uint;
    }
    if (data_format == AmdGpu::DataFormat::Format8_8_8_8 &&
        num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eR8G8B8A8Unorm;
    }
    if (data_format == AmdGpu::DataFormat::Format8_8_8_8 &&
        num_format == AmdGpu::NumberFormat::Srgb) {
        return vk::Format::eR8G8B8A8Srgb;
    }
    if (data_format == AmdGpu::DataFormat::Format32_32_32 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR32G32B32Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format32_32 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR32G32Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format5_6_5 &&
        num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eB5G6R5UnormPack16;
    }
    if (data_format == AmdGpu::DataFormat::Format8 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eR8Unorm;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc3 && num_format == AmdGpu::NumberFormat::Srgb) {
        return vk::Format::eBc3SrgbBlock;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc3 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eBc3UnormBlock;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc4 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eBc4UnormBlock;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16_16_16 &&
        num_format == AmdGpu::NumberFormat::Sint) {
        return vk::Format::eR16G16B16A16Sint;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR16G16Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format10_11_11 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eB10G11R11UfloatPack32;
    }
    if (data_format == AmdGpu::DataFormat::Format2_10_10_10 &&
        num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eA2B10G10R10UnormPack32;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc7 && num_format == AmdGpu::NumberFormat::Srgb) {
        return vk::Format::eBc7SrgbBlock;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc1 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eBc1RgbaUnormBlock;
    }
    if (data_format == AmdGpu::DataFormat::Format8_8_8_8 &&
        num_format == AmdGpu::NumberFormat::Uint) {
        return vk::Format::eR8G8B8A8Uint;
    }
    if (data_format == AmdGpu::DataFormat::Format16 && num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR16Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format32 && num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR32Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16_16_16 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR16G16B16A16Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format32 && num_format == AmdGpu::NumberFormat::Uint) {
        return vk::Format::eR32Uint;
    }
    if (data_format == AmdGpu::DataFormat::Format32 && num_format == AmdGpu::NumberFormat::Sint) {
        return vk::Format::eR32Sint;
    }
    if (data_format == AmdGpu::DataFormat::Format8_8 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eR8G8Unorm;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc7 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eBc7UnormBlock;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc2 && num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eBc2UnormBlock;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16 &&
        num_format == AmdGpu::NumberFormat::Snorm) {
        return vk::Format::eR16G16Snorm;
    }
    if (data_format == AmdGpu::DataFormat::Format2_10_10_10 &&
        num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eA2R10G10B10UnormPack32;
    }
    if (data_format == AmdGpu::DataFormat::Format2_10_10_10 &&
        num_format == AmdGpu::NumberFormat::Snorm) {
        return vk::Format::eA2R10G10B10SnormPack32;
    }
    if (data_format == AmdGpu::DataFormat::Format10_11_11 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eB10G11R11UfloatPack32;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16 &&
        num_format == AmdGpu::NumberFormat::Float) {
        return vk::Format::eR16G16Sfloat;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16_16_16 &&
        num_format == AmdGpu::NumberFormat::Snorm) {
        return vk::Format::eR16G16B16A16Snorm;
    }
    if (data_format == AmdGpu::DataFormat::Format32_32 &&
        num_format == AmdGpu::NumberFormat::Uint) {
        return vk::Format::eR32G32Uint;
    }
    if (data_format == AmdGpu::DataFormat::Format4_4_4_4 &&
        num_format == AmdGpu::NumberFormat::Unorm) {
        return vk::Format::eR4G4B4A4UnormPack16;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16_16_16 &&
        num_format == AmdGpu::NumberFormat::Uint) {
        return vk::Format::eR16G16B16A16Uint;
    }
    if (data_format == AmdGpu::DataFormat::Format32_32_32_32 &&
        num_format == AmdGpu::NumberFormat::Uint) {
        return vk::Format::eR32G32B32A32Uint;
    }
    if (data_format == AmdGpu::DataFormat::Format8 && num_format == AmdGpu::NumberFormat::Sint) {
        return vk::Format::eR8Sint;
    }
    if (data_format == AmdGpu::DataFormat::FormatBc1 && num_format == AmdGpu::NumberFormat::Srgb) {
        return vk::Format::eBc1RgbaSrgbBlock;
    }
    if (data_format == AmdGpu::DataFormat::Format16_16 &&
        num_format == AmdGpu::NumberFormat::Sint) {
        return vk::Format::eR16G16Sint;
    }
    UNREACHABLE_MSG("Unknown data_format={} and num_format={}", u32(data_format), u32(num_format));
}

vk::Format AdjustColorBufferFormat(vk::Format base_format,
                                   Liverpool::ColorBuffer::SwapMode comp_swap, bool is_vo_surface) {
    ASSERT_MSG(comp_swap == Liverpool::ColorBuffer::SwapMode::Standard ||
                   comp_swap == Liverpool::ColorBuffer::SwapMode::Alternate,
               "Unsupported component swap mode {}", static_cast<u32>(comp_swap));

    const bool comp_swap_alt = comp_swap == Liverpool::ColorBuffer::SwapMode::Alternate;
    if (comp_swap_alt) {
        switch (base_format) {
        case vk::Format::eR8G8B8A8Unorm:
            return vk::Format::eB8G8R8A8Unorm;
        case vk::Format::eB8G8R8A8Unorm:
            return vk::Format::eR8G8B8A8Unorm;
        case vk::Format::eR8G8B8A8Srgb:
            return is_vo_surface ? vk::Format::eB8G8R8A8Unorm : vk::Format::eB8G8R8A8Srgb;
        case vk::Format::eB8G8R8A8Srgb:
            return is_vo_surface ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb;
        }
    } else {
        if (is_vo_surface && base_format == vk::Format::eR8G8B8A8Srgb) {
            return vk::Format::eR8G8B8A8Unorm;
        }
        if (is_vo_surface && base_format == vk::Format::eB8G8R8A8Srgb) {
            return vk::Format::eB8G8R8A8Unorm;
        }
    }
    return base_format;
}

vk::Format DepthFormat(DepthBuffer::ZFormat z_format, DepthBuffer::StencilFormat stencil_format) {
    if (z_format == DepthBuffer::ZFormat::Z32Float &&
        stencil_format == DepthBuffer::StencilFormat::Stencil8) {
        return vk::Format::eD32SfloatS8Uint;
    }
    if (z_format == DepthBuffer::ZFormat::Z32Float &&
        stencil_format == DepthBuffer::StencilFormat::Invalid) {
        return vk::Format::eD32Sfloat;
    }
    if (z_format == DepthBuffer::ZFormat::Z16 &&
        stencil_format == DepthBuffer::StencilFormat::Invalid) {
        return vk::Format::eD16Unorm;
    }
    if (z_format == DepthBuffer::ZFormat::Z16 &&
        stencil_format == DepthBuffer::StencilFormat::Stencil8) {
        return vk::Format::eD16UnormS8Uint;
    }
    if (z_format == DepthBuffer::ZFormat::Invald &&
        stencil_format == DepthBuffer::StencilFormat::Invalid) {
        return vk::Format::eUndefined;
    }
    UNREACHABLE();
}

void EmitQuadToTriangleListIndices(u8* out_ptr, u32 num_vertices) {
    static constexpr u16 NumVerticesPerQuad = 4;
    u16* out_data = reinterpret_cast<u16*>(out_ptr);
    for (u16 i = 0; i < num_vertices; i += NumVerticesPerQuad) {
        *out_data++ = i;
        *out_data++ = i + 1;
        *out_data++ = i + 2;
        *out_data++ = i + 2;
        *out_data++ = i;
        *out_data++ = i + 3;
    }
}

static constexpr float U8ToUnorm(u8 v) {
    static constexpr auto c = 1.0f / 255.0f;
    return float(v * c);
}

vk::ClearValue ColorBufferClearValue(const AmdGpu::Liverpool::ColorBuffer& color_buffer) {
    const auto comp_swap = color_buffer.info.comp_swap.Value();
    ASSERT_MSG(comp_swap == Liverpool::ColorBuffer::SwapMode::Standard ||
                   comp_swap == Liverpool::ColorBuffer::SwapMode::Alternate,
               "Unsupported component swap mode {}", static_cast<u32>(comp_swap));

    const bool comp_swap_alt = comp_swap == Liverpool::ColorBuffer::SwapMode::Alternate;

    const auto& c0 = color_buffer.clear_word0;
    const auto& c1 = color_buffer.clear_word1;
    const auto num_bits = AmdGpu::NumBits(color_buffer.info.format);

    vk::ClearColorValue color{};
    switch (color_buffer.info.number_type) {
    case AmdGpu::NumberFormat::Snorm:
        [[fallthrough]];
    case AmdGpu::NumberFormat::SnormNz:
        [[fallthrough]];
    case AmdGpu::NumberFormat::Unorm:
        [[fallthrough]];
    case AmdGpu::NumberFormat::Srgb: {
        switch (num_bits) {
        case 32: {
            color.float32 = std::array{
                U8ToUnorm((c0 >> (comp_swap_alt ? 16 : 0)) & 0xff),
                U8ToUnorm((c0 >> 8) & 0xff),
                U8ToUnorm((c0 >> (comp_swap_alt ? 0 : 16)) & 0xff),
                U8ToUnorm((c0 >> 24) & 0xff),
            };
            break;
        }
        default: {
            LOG_ERROR(Render_Vulkan, "Missing clear color conversion for bits {}", num_bits);
            break;
        }
        }
        break;
    }
    default: {
        LOG_ERROR(Render_Vulkan, "Missing clear color conversion for type {}",
                  color_buffer.info.number_type.Value());
        break;
    }
    }
    return {.color = color};
}

vk::SampleCountFlagBits NumSamples(u32 num_samples) {
    switch (num_samples) {
    case 1:
        return vk::SampleCountFlagBits::e1;
    case 2:
        return vk::SampleCountFlagBits::e2;
    case 4:
        return vk::SampleCountFlagBits::e4;
    case 8:
        return vk::SampleCountFlagBits::e8;
    default:
        UNREACHABLE();
    }
}

} // namespace Vulkan::LiverpoolToVK
