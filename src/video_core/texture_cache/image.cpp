// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/config.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/image.h"
#include "video_core/texture_cache/tile_manager.h"

#include <vk_mem_alloc.h>

namespace VideoCore {

using namespace Vulkan;
using VideoOutFormat = Libraries::VideoOut::PixelFormat;
using Libraries::VideoOut::TilingMode;

static vk::Format ConvertPixelFormat(const VideoOutFormat format) {
    switch (format) {
    case VideoOutFormat::A8R8G8B8Srgb:
        return vk::Format::eB8G8R8A8Srgb;
    case VideoOutFormat::A8B8G8R8Srgb:
        return vk::Format::eR8G8B8A8Srgb;
    case VideoOutFormat::A2R10G10B10:
    case VideoOutFormat::A2R10G10B10Srgb:
        return vk::Format::eA2R10G10B10UnormPack32;
    default:
        break;
    }
    UNREACHABLE_MSG("Unknown format={}", static_cast<u32>(format));
    return {};
}

bool ImageInfo::IsBlockCoded() const {
    switch (pixel_format) {
    case vk::Format::eBc1RgbaSrgbBlock:
    case vk::Format::eBc1RgbaUnormBlock:
    case vk::Format::eBc1RgbSrgbBlock:
    case vk::Format::eBc1RgbUnormBlock:
    case vk::Format::eBc2SrgbBlock:
    case vk::Format::eBc2UnormBlock:
    case vk::Format::eBc3SrgbBlock:
    case vk::Format::eBc3UnormBlock:
    case vk::Format::eBc4SnormBlock:
    case vk::Format::eBc4UnormBlock:
    case vk::Format::eBc5SnormBlock:
    case vk::Format::eBc5UnormBlock:
    case vk::Format::eBc6HSfloatBlock:
    case vk::Format::eBc6HUfloatBlock:
    case vk::Format::eBc7SrgbBlock:
    case vk::Format::eBc7UnormBlock:
        return true;
    default:
        return false;
    }
}

bool ImageInfo::IsPacked() const {
    switch (pixel_format) {
    case vk::Format::eB5G5R5A1UnormPack16:
        [[fallthrough]];
    case vk::Format::eB5G6R5UnormPack16:
        return true;
    default:
        return false;
    }
}

bool ImageInfo::IsDepthStencil() const {
    switch (pixel_format) {
    case vk::Format::eD16Unorm:
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD32Sfloat:
    case vk::Format::eD32SfloatS8Uint:
        return true;
    default:
        return false;
    }
}

static vk::ImageUsageFlags ImageUsageFlags(const ImageInfo& info) {
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eTransferSrc |
                                vk::ImageUsageFlagBits::eTransferDst |
                                vk::ImageUsageFlagBits::eSampled;
    if (info.IsDepthStencil()) {
        usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    } else {
        if (!info.IsBlockCoded() && !info.IsPacked()) {
            usage |= vk::ImageUsageFlagBits::eColorAttachment;
        }
    }

    // In cases where an image is created as a render/depth target and cleared with compute,
    // we cannot predict whether it will be used as a storage image. A proper solution would
    // involve re-creating the resource with a new configuration and copying previous content into
    // it. However, for now, we will set storage usage for all images (if the format allows),
    // sacrificing a bit of performance. Note use of ExtendedUsage flag set by default.
    usage |= vk::ImageUsageFlagBits::eStorage;
    return usage;
}

static vk::ImageType ConvertImageType(AmdGpu::ImageType type) noexcept {
    switch (type) {
    case AmdGpu::ImageType::Color1D:
    case AmdGpu::ImageType::Color1DArray:
        return vk::ImageType::e1D;
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Cube:
    case AmdGpu::ImageType::Color2DArray:
        return vk::ImageType::e2D;
    case AmdGpu::ImageType::Color3D:
        return vk::ImageType::e3D;
    default:
        UNREACHABLE();
    }
}

ImageInfo::ImageInfo(const Libraries::VideoOut::BufferAttributeGroup& group) noexcept {
    const auto& attrib = group.attrib;
    is_tiled = attrib.tiling_mode == TilingMode::Tile;
    tiling_mode =
        is_tiled ? AmdGpu::TilingMode::Display_MacroTiled : AmdGpu::TilingMode::Display_Linear;
    pixel_format = ConvertPixelFormat(attrib.pixel_format);
    type = vk::ImageType::e2D;
    size.width = attrib.width;
    size.height = attrib.height;
    pitch = attrib.tiling_mode == TilingMode::Linear ? size.width : (size.width + 127) & (~127);
    const bool is_32bpp = attrib.pixel_format != VideoOutFormat::A16R16G16B16Float;
    ASSERT(is_32bpp);
    if (!is_tiled) {
        guest_size_bytes = pitch * size.height * 4;
        return;
    }
    if (Config::isNeoMode()) {
        guest_size_bytes = pitch * ((size.height + 127) & (~127)) * 4;
    } else {
        guest_size_bytes = pitch * ((size.height + 63) & (~63)) * 4;
    }
    usage.vo_buffer = true;
}

ImageInfo::ImageInfo(const AmdGpu::Liverpool::ColorBuffer& buffer,
                     const AmdGpu::Liverpool::CbDbExtent& hint /*= {}*/) noexcept {
    is_tiled = buffer.IsTiled();
    tiling_mode = buffer.GetTilingMode();
    pixel_format = LiverpoolToVK::SurfaceFormat(buffer.info.format, buffer.NumFormat());
    num_samples = 1 << buffer.attrib.num_fragments_log2;
    type = vk::ImageType::e2D;
    size.width = hint.Valid() ? hint.width : buffer.Pitch();
    size.height = hint.Valid() ? hint.height : buffer.Height();
    size.depth = 1;
    pitch = size.width;
    guest_size_bytes = buffer.GetSizeAligned();
    meta_info.cmask_addr = buffer.info.fast_clear ? buffer.CmaskAddress() : 0;
    meta_info.fmask_addr = buffer.info.compression ? buffer.FmaskAddress() : 0;
    usage.render_target = true;
}

ImageInfo::ImageInfo(const AmdGpu::Liverpool::DepthBuffer& buffer, VAddr htile_address,
                     const AmdGpu::Liverpool::CbDbExtent& hint) noexcept {
    is_tiled = false;
    pixel_format = LiverpoolToVK::DepthFormat(buffer.z_info.format, buffer.stencil_info.format);
    type = vk::ImageType::e2D;
    num_samples = 1 << buffer.z_info.num_samples; // spec doesn't say it is a log2
    size.width = hint.Valid() ? hint.width : buffer.Pitch();
    size.height = hint.Valid() ? hint.height : buffer.Height();
    size.depth = 1;
    pitch = size.width;
    guest_size_bytes = buffer.GetSizeAligned();
    meta_info.htile_addr = buffer.z_info.tile_surface_en ? htile_address : 0;
    usage.depth_target = true;
}

ImageInfo::ImageInfo(const AmdGpu::Image& image) noexcept {
    is_tiled = image.IsTiled();
    tiling_mode = image.GetTilingMode();
    pixel_format = LiverpoolToVK::SurfaceFormat(image.GetDataFmt(), image.GetNumberFmt());
    type = ConvertImageType(image.GetType());
    size.width = image.width + 1;
    size.height = image.height + 1;
    size.depth = 1;
    pitch = image.Pitch();
    resources.levels = image.NumLevels();
    resources.layers = image.NumLayers();
    guest_size_bytes = image.GetSizeAligned();
    usage.texture = true;
}

UniqueImage::UniqueImage(vk::Device device_, VmaAllocator allocator_)
    : device{device_}, allocator{allocator_} {}

UniqueImage::~UniqueImage() {
    if (image) {
        vmaDestroyImage(allocator, image, allocation);
    }
}

void UniqueImage::Create(const vk::ImageCreateInfo& image_ci) {
    const VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
    };

    const VkImageCreateInfo image_ci_unsafe = static_cast<VkImageCreateInfo>(image_ci);
    VkImage unsafe_image{};
    VkResult result = vmaCreateImage(allocator, &image_ci_unsafe, &alloc_info, &unsafe_image,
                                     &allocation, nullptr);
    ASSERT_MSG(result == VK_SUCCESS, "Failed allocating image with error {}",
               vk::to_string(vk::Result{result}));
    image = vk::Image{unsafe_image};
}

Image::Image(const Vulkan::Instance& instance_, Vulkan::Scheduler& scheduler_,
             const ImageInfo& info_, VAddr cpu_addr)
    : instance{&instance_}, scheduler{&scheduler_}, info{info_},
      image{instance->GetDevice(), instance->GetAllocator()}, cpu_addr{cpu_addr},
      cpu_addr_end{cpu_addr + info.guest_size_bytes} {
    ASSERT(info.pixel_format != vk::Format::eUndefined);
    vk::ImageCreateFlags flags{vk::ImageCreateFlagBits::eMutableFormat |
                               vk::ImageCreateFlagBits::eExtendedUsage};
    if (info.type == vk::ImageType::e2D && info.resources.layers >= 6 &&
        info.size.width == info.size.height) {
        flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }
    if (info.type == vk::ImageType::e3D) {
        flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;
    }
    if (info.IsBlockCoded()) {
        flags |= vk::ImageCreateFlagBits::eBlockTexelViewCompatible;
    }

    usage = ImageUsageFlags(info);

    if (info.pixel_format == vk::Format::eD32Sfloat) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth;
    }
    if (info.pixel_format == vk::Format::eD32SfloatS8Uint) {
        aspect_mask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    }

    const vk::ImageCreateInfo image_ci = {
        .flags = flags,
        .imageType = info.type,
        .format = info.pixel_format,
        .extent{
            .width = info.size.width,
            .height = info.size.height,
            .depth = info.size.depth,
        },
        .mipLevels = static_cast<u32>(info.resources.levels),
        .arrayLayers = static_cast<u32>(info.resources.layers),
        .samples = LiverpoolToVK::NumSamples(info.num_samples),
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    image.Create(image_ci);

    // Create a special view for detiler
    if (info.is_tiled) {
        ImageViewInfo view_info;
        view_info.format = DemoteImageFormatForDetiling(info.pixel_format);
        view_for_detiler.emplace(*instance, view_info, *this, ImageId{});
    }

    Transit(vk::ImageLayout::eGeneral, vk::AccessFlagBits::eNone);
}

void Image::Transit(vk::ImageLayout dst_layout, vk::Flags<vk::AccessFlagBits> dst_mask,
                    vk::CommandBuffer cmdbuf) {
    if (dst_layout == layout && dst_mask == access_mask) {
        return;
    }

    const vk::ImageMemoryBarrier barrier = {
        .srcAccessMask = access_mask,
        .dstAccessMask = dst_mask,
        .oldLayout = layout,
        .newLayout = dst_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange{
            .aspectMask = aspect_mask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    // Adjust pipieline stage
    const vk::PipelineStageFlags dst_pl_stage =
        (dst_mask == vk::AccessFlagBits::eTransferRead ||
         dst_mask == vk::AccessFlagBits::eTransferWrite)
            ? vk::PipelineStageFlagBits::eTransfer
            : vk::PipelineStageFlagBits::eAllGraphics | vk::PipelineStageFlagBits::eComputeShader;

    if (!cmdbuf) {
        // When using external cmdbuf you are responsible for ending rp.
        scheduler->EndRendering();
        cmdbuf = scheduler->CommandBuffer();
    }
    cmdbuf.pipelineBarrier(pl_stage, dst_pl_stage, vk::DependencyFlagBits::eByRegion, {}, {},
                           barrier);

    layout = dst_layout;
    access_mask = dst_mask;
    pl_stage = dst_pl_stage;
}

void Image::Upload(vk::Buffer buffer, u64 offset) {
    scheduler->EndRendering();
    Transit(vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite);

    // Copy to the image.
    const auto aspect = aspect_mask & vk::ImageAspectFlagBits::eStencil
                            ? vk::ImageAspectFlagBits::eDepth
                            : aspect_mask;
    const vk::BufferImageCopy image_copy = {
        .bufferOffset = offset,
        .bufferRowLength = info.pitch,
        .bufferImageHeight = info.size.height,
        .imageSubresource{
            .aspectMask = aspect,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {info.size.width, info.size.height, 1},
    };

    const auto cmdbuf = scheduler->CommandBuffer();
    cmdbuf.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, image_copy);

    Transit(vk::ImageLayout::eGeneral,
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead);
}

Image::~Image() = default;

} // namespace VideoCore
