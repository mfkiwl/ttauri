// Copyright 2019 Pokitec
// All rights reserved.

#include "PipelineSDF.hpp"
#include "PipelineSDF_DeviceShared.hpp"
#include "gui_device_vulkan.hpp"
#include "../text/ShapedText.hpp"
#include "../PixelMap.hpp"
#include "../URL.hpp"
#include "../memory.hpp"
#include "../cast.hpp"
#include "../BezierCurve.hpp"
#include "../numeric_array.hpp"
#include "../aarect.hpp"
#include "../mat.hpp"
#include <array>

namespace tt::PipelineSDF {

using namespace std;

DeviceShared::DeviceShared(gui_device_vulkan const &device) :
    device(device)
{
    buildShaders();
    buildAtlas();
}

DeviceShared::~DeviceShared()
{
}

void DeviceShared::destroy(gui_device_vulkan *vulkanDevice)
{
    tt_axiom(vulkanDevice);

    teardownShaders(vulkanDevice);
    teardownAtlas(vulkanDevice);
}

[[nodiscard]] AtlasRect DeviceShared::allocateRect(f32x4 drawExtent) noexcept
{
    auto imageWidth = narrow_cast<int>(std::ceil(drawExtent.width()));
    auto imageHeight = narrow_cast<int>(std::ceil(drawExtent.height()));

    if (atlasAllocationPosition.y() + imageHeight > atlasImageHeight) {
        atlasAllocationPosition.x() = 0; 
        atlasAllocationPosition.y() = 0;
        atlasAllocationPosition.z() = atlasAllocationPosition.z() + 1;

        if (atlasAllocationPosition.z() >= atlasMaximumNrImages) {
            LOG_FATAL("PipelineSDF atlas overflow, too many glyphs in use.");
        }

        if (atlasAllocationPosition.z() >= size(atlasTextures)) {
            addAtlasImage();
        }
    }

    if (atlasAllocationPosition.x() + imageWidth > atlasImageWidth) {
        atlasAllocationPosition.x() = 0;
        atlasAllocationPosition.y() = atlasAllocationPosition.y() + atlasAllocationMaxHeight;
    }

    auto r = AtlasRect{atlasAllocationPosition, drawExtent};
    
    atlasAllocationPosition.x() = atlasAllocationPosition.x() + imageWidth;
    atlasAllocationMaxHeight = std::max(atlasAllocationMaxHeight, imageHeight);

    return r;
}


void DeviceShared::uploadStagingPixmapToAtlas(AtlasRect location)
{
    // Flush the given image, included the border.
    device.flushAllocation(
        stagingTexture.allocation,
        0,
        (stagingTexture.pixelMap.height * stagingTexture.pixelMap.stride) * sizeof (SDF8)
    );
    
    stagingTexture.transitionLayout(device, vk::Format::eR8Snorm, vk::ImageLayout::eTransferSrcOptimal);

    array<vector<vk::ImageCopy>, atlasMaximumNrImages> regionsToCopyPerAtlasTexture; 

    auto regionsToCopy = std::vector{vk::ImageCopy{
        { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        { 0, 0, 0 },
        { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
        { narrow_cast<int32_t>(location.atlasPosition.x()), narrow_cast<int32_t>(location.atlasPosition.y()), 0 },
        { narrow_cast<uint32_t>(location.atlasExtent.x()), narrow_cast<uint32_t>(location.atlasExtent.y()), 1}
    }};

    auto &atlasTexture = atlasTextures.at(location.atlasPosition.z());
    atlasTexture.transitionLayout(device, vk::Format::eR8Snorm, vk::ImageLayout::eTransferDstOptimal);

    device.copyImage(stagingTexture.image, vk::ImageLayout::eTransferSrcOptimal, atlasTexture.image, vk::ImageLayout::eTransferDstOptimal, std::move(regionsToCopy));
}

void DeviceShared::prepareStagingPixmapForDrawing()
{
    stagingTexture.transitionLayout(device, vk::Format::eR8Snorm, vk::ImageLayout::eGeneral);
}

void DeviceShared::prepareAtlasForRendering()
{
    for (auto &atlasTexture: atlasTextures) {
        atlasTexture.transitionLayout(device, vk::Format::eR8Snorm, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}




/** Prepare the atlas for drawing a text.
 *
 *  +---------------------+
 *  |     draw border     |
 *  |  +---------------+  |
 *  |  | render border |  |
 *  |  |  +---------+  |  |
 *  |  |  |  glyph  |  |  |
 *  |  |  | bounding|  |  |
 *  |  |  |   box   |  |  |
 *  |  |  +---------+  |  |
 *  |  |               |  |
 *  |  +---------------+  |
 *  |                     |
 *  O---------------------+
*/
AtlasRect DeviceShared::addGlyphToAtlas(FontGlyphIDs glyph) noexcept
{
    ttlet [glyphPath, glyphBoundingBox] = glyph.getPathAndBoundingBox();

    ttlet drawScale = mat::S(drawFontSize, drawFontSize);
    ttlet scaledBoundingBox = drawScale * glyphBoundingBox;

    // We will draw the font at a fixed size into the texture. And we need a border for the texture to
    // allow proper bi-linear interpolation on the edges.

    // Determine the size of the image in the atlas.
    // This is the bounding box sized to the fixed font size and a border
    ttlet drawOffset = f32x4{drawBorder, drawBorder} - scaledBoundingBox.offset();
    ttlet drawExtent = scaledBoundingBox.extent() + 2.0f * f32x4{drawBorder, drawBorder};
    ttlet drawTranslate = mat::T(drawOffset);

    // Transform the path to the scale of the fixed font size and drawing the bounding box inside the image.
    ttlet drawPath = (drawTranslate * drawScale) * glyphPath;

    // Draw glyphs into staging buffer of the atlas and upload it to the correct position in the atlas.
    prepareStagingPixmapForDrawing();
    auto atlas_rect = allocateRect(drawExtent);
    auto pixmap = stagingTexture.pixelMap.submap(iaarect{i32x4::point(), atlas_rect.atlasExtent});
    fill(pixmap, drawPath);
    uploadStagingPixmapToAtlas(atlas_rect);

    return atlas_rect;
}

std::pair<AtlasRect,bool> DeviceShared::getGlyphFromAtlas(FontGlyphIDs glyph) noexcept
{
    ttlet i = glyphs_in_atlas.find(glyph);
    if (i != glyphs_in_atlas.cend()) {
        return {i->second, false};

    } else {
        ttlet aarect = addGlyphToAtlas(glyph);
        glyphs_in_atlas.emplace(glyph, aarect);
        return {aarect, true};
    }
}

aarect DeviceShared::getBoundingBox(FontGlyphIDs const &glyphs) noexcept {
    // Adjust bounding box by adding a border based on 1EM.
    return expand(glyphs.getBoundingBox(), scaledDrawBorder);
}

bool DeviceShared::_placeVertices(vspan<Vertex> &vertices, FontGlyphIDs const &glyphs, rect box, f32x4 color, aarect clippingRectangle) noexcept
{
    ttlet [atlas_rect, glyph_was_added] = getGlyphFromAtlas(glyphs);

    ttlet v0 = box.corner<0>();
    ttlet v1 = box.corner<1>();
    ttlet v2 = box.corner<2>();
    ttlet v3 = box.corner<3>();

    // If none of the vertices is inside the clipping rectangle then don't add the
    // quad to the vertex list.
    if (!overlaps(clippingRectangle, box.aabb())) {
        return glyph_was_added;
    }

    vertices.emplace_back(v0, clippingRectangle, get<0>(atlas_rect.textureCoords), color);
    vertices.emplace_back(v1, clippingRectangle, get<1>(atlas_rect.textureCoords), color);
    vertices.emplace_back(v2, clippingRectangle, get<2>(atlas_rect.textureCoords), color);
    vertices.emplace_back(v3, clippingRectangle, get<3>(atlas_rect.textureCoords), color);
    return glyph_was_added;
}

bool DeviceShared::_placeVertices(vspan<Vertex> &vertices, AttributedGlyph const &attr_glyph, mat transform, aarect clippingRectangle, f32x4 color) noexcept
{
    if (!is_visible(attr_glyph.general_category)) {
        return false;
    }

    // Adjust bounding box by adding a border based on 1EM.
    ttlet bounding_box = transform * attr_glyph.boundingBox(scaledDrawBorder);

    return _placeVertices(vertices, attr_glyph.glyphs, bounding_box, color, clippingRectangle);
}

bool DeviceShared::_placeVertices(vspan<Vertex> &vertices, AttributedGlyph const &attr_glyph, mat transform, aarect clippingRectangle) noexcept
{
    return _placeVertices(vertices, attr_glyph, transform, clippingRectangle, attr_glyph.style.color);
}

void DeviceShared::placeVertices(vspan<Vertex> &vertices, FontGlyphIDs const &glyphs, rect box, f32x4 color, aarect clippingRectangle) noexcept
{
    if (_placeVertices(vertices, glyphs, box, color, clippingRectangle)) {
        prepareAtlasForRendering();
    }
}

void DeviceShared::placeVertices(vspan<Vertex> &vertices, ShapedText const &text, mat transform, aarect clippingRectangle) noexcept
{
    auto atlas_was_updated = false;

    for (ttlet &attr_glyph: text) {
        ttlet glyph_added = _placeVertices(vertices, attr_glyph, transform, clippingRectangle);
        atlas_was_updated = atlas_was_updated || glyph_added;
    }

    if (atlas_was_updated) {
        prepareAtlasForRendering();
    }
}

void DeviceShared::placeVertices(vspan<Vertex> &vertices, ShapedText const &text, mat transform, aarect clippingRectangle, f32x4 color) noexcept
{
    auto atlas_was_updated = false;

    for (ttlet &attr_glyph: text) {
        ttlet glyph_added = _placeVertices(vertices, attr_glyph, transform, clippingRectangle, color);
        atlas_was_updated = atlas_was_updated || glyph_added;
    }

    if (atlas_was_updated) {
        prepareAtlasForRendering();
    }
}

void DeviceShared::drawInCommandBuffer(vk::CommandBuffer &commandBuffer)
{
    commandBuffer.bindIndexBuffer(device.quadIndexBuffer, 0, vk::IndexType::eUint16);
}

void DeviceShared::buildShaders()
{
    specializationConstants.SDF8maxDistance = SDF8::max_distance;
    specializationConstants.atlasImageWidth = atlasImageWidth;

    fragmentShaderSpecializationMapEntries = SpecializationConstants::specializationConstantMapEntries();
    fragmentShaderSpecializationInfo = specializationConstants.specializationInfo(fragmentShaderSpecializationMapEntries);

    vertexShaderModule = device.loadShader(URL("resource:GUI/PipelineSDF.vert.spv"));
    fragmentShaderModule = device.loadShader(URL("resource:GUI/PipelineSDF.frag.spv"));


    shaderStages = {
        {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main"},
        {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main", &fragmentShaderSpecializationInfo}
    };
}

void DeviceShared::teardownShaders(gui_device_vulkan * vulkanDevice)
{
    tt_axiom(vulkanDevice);

    vulkanDevice->destroy(vertexShaderModule);
    vulkanDevice->destroy(fragmentShaderModule);
}

void DeviceShared::addAtlasImage()
{
    //ttlet currentImageIndex = std::ssize(atlasTextures);

    // Create atlas image
    vk::ImageCreateInfo const imageCreateInfo = {
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format::eR8Snorm,
        vk::Extent3D(atlasImageWidth, atlasImageHeight, 1),
        1, // mipLevels
        1, // arrayLayers
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::SharingMode::eExclusive,
        0, nullptr,
        vk::ImageLayout::eUndefined
    };
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    ttlet [atlasImage, atlasImageAllocation] = device.createImage(imageCreateInfo, allocationCreateInfo);

    ttlet clearValue = vk::ClearColorValue{std::array{-1.0f, -1.0f, -1.0f, -1.0f}};
    ttlet clearRange = std::array{
        vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1
        }
    };
    device.clearColorImage(atlasImage, vk::ImageLayout::eTransferDstOptimal, clearValue, clearRange);

    ttlet atlasImageView = device.createImageView({
        vk::ImageViewCreateFlags(),
        atlasImage,
        vk::ImageViewType::e2D,
        imageCreateInfo.format,
        vk::ComponentMapping(),
        {
            vk::ImageAspectFlagBits::eColor,
            0, // baseMipLevel
            1, // levelCount
            0, // baseArrayLayer
            1 // layerCount
        }
    });

    atlasTextures.push_back({ atlasImage, atlasImageAllocation, atlasImageView });

    // Build image descriptor info.
    for (int i = 0; i < std::ssize(atlasDescriptorImageInfos); i++) {
        // Point the descriptors to each imageView,
        // repeat the first imageView if there are not enough.
        atlasDescriptorImageInfos.at(i) = {
            vk::Sampler(),
            i < atlasTextures.size() ? atlasTextures.at(i).view : atlasTextures.at(0).view,
            vk::ImageLayout::eShaderReadOnlyOptimal
        };
    }

}

void DeviceShared::buildAtlas()
{
    // Create staging image
    vk::ImageCreateInfo const imageCreateInfo = {
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        vk::Format::eR8Snorm,
        vk::Extent3D(stagingImageWidth, stagingImageHeight, 1),
        1, // mipLevels
        1, // arrayLayers
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eLinear,
        vk::ImageUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive,
        0, nullptr,
        vk::ImageLayout::ePreinitialized
    };
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    ttlet [image, allocation] = device.createImage(imageCreateInfo, allocationCreateInfo);
    ttlet data = device.mapMemory<SDF8>(allocation);

    stagingTexture = {
        image,
        allocation,
        vk::ImageView(),
        tt::PixelMap<SDF8>{data.data(), ssize_t{imageCreateInfo.extent.width}, ssize_t{imageCreateInfo.extent.height}}
    };

    vk::SamplerCreateInfo const samplerCreateInfo = {
        vk::SamplerCreateFlags(),
        vk::Filter::eLinear, // magFilter
        vk::Filter::eLinear, // minFilter
        vk::SamplerMipmapMode::eNearest, // mipmapMode
        vk::SamplerAddressMode::eClampToEdge, // addressModeU
        vk::SamplerAddressMode::eClampToEdge, // addressModeV
        vk::SamplerAddressMode::eClampToEdge, // addressModeW
        0.0, // mipLodBias
        VK_FALSE, // anisotropyEnable
        0.0, // maxAnisotropy
        VK_FALSE, // compareEnable
        vk::CompareOp::eNever,
        0.0, // minLod
        0.0, // maxLod
        vk::BorderColor::eFloatTransparentBlack,
        VK_FALSE // unnormazlizedCoordinates
    };
    atlasSampler = device.createSampler(samplerCreateInfo);

    atlasSamplerDescriptorImageInfo = {
        atlasSampler,
        vk::ImageView(),
        vk::ImageLayout::eUndefined
    };

    // There needs to be at least one atlas image, so the array of samplers can point to
    // the single image.
    addAtlasImage();
}

void DeviceShared::teardownAtlas(gui_device_vulkan *vulkanDevice)
{
    tt_axiom(vulkanDevice);

    vulkanDevice->destroy(atlasSampler);

    for (const auto &atlasImage: atlasTextures) {
        vulkanDevice->destroy(atlasImage.view);
        vulkanDevice->destroyImage(atlasImage.image, atlasImage.allocation);
    }
    atlasTextures.clear();

    vulkanDevice->unmapMemory(stagingTexture.allocation);
    vulkanDevice->destroyImage(stagingTexture.image, stagingTexture.allocation);
}

}
