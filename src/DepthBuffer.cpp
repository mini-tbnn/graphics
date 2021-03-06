#include "DepthBuffer.hpp"
#include "Graphics.hpp"
#include "Utils.hpp"

#include <cassert>

DepthBuffer::DepthBuffer()
{
}

DepthBuffer::~DepthBuffer()
{
	vkDestroyImageView(Graphics::g_device, m_view, nullptr);
	vkDestroyImage(Graphics::g_device, m_image, nullptr);
	vkFreeMemory(Graphics::g_device, m_memory, nullptr);
}

void DepthBuffer::create(const std::string& name, uint32_t width, uint32_t height, VkFormat format)
{
	m_format = format;
	m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	Utils::createImage(
		VK_IMAGE_TYPE_2D,
		width, 
		height, 
		1,
		1,
		format, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		m_image, 
		m_memory);

	m_view = Utils::createImageView(
		m_image, 
		format, 
		VK_IMAGE_ASPECT_DEPTH_BIT);

#if 0
	auto vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(Graphics::g_instance, "vkDebugMarkerSetObjectNameEXT");
	{// Name image
		VkDebugMarkerObjectNameInfoEXT debugNameInfo = {};
		debugNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		debugNameInfo.pObjectName = name.c_str();
		debugNameInfo.object = reinterpret_cast<uint64_t>(m_image);
		debugNameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
		vkDebugMarkerSetObjectNameEXT(Graphics::g_device, &debugNameInfo);
	}
	{// Name memory
		VkDebugMarkerObjectNameInfoEXT debugNameInfo = {};
		debugNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		debugNameInfo.pObjectName = name.c_str();
		debugNameInfo.object = reinterpret_cast<uint64_t>(m_memory);
		debugNameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT;
		vkDebugMarkerSetObjectNameEXT(Graphics::g_device, &debugNameInfo);
	}
	{// Name view
		VkDebugMarkerObjectNameInfoEXT debugNameInfo = {};
		debugNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		debugNameInfo.pObjectName = name.c_str();
		debugNameInfo.object = reinterpret_cast<uint64_t>(m_view);
		debugNameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT;
		vkDebugMarkerSetObjectNameEXT(Graphics::g_device, &debugNameInfo);
	}
#endif
}

static bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT
		|| format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void DepthBuffer::transition(VkCommandBuffer cb, VkImageLayout newLayout)
{
	assert(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		|| newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		|| newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	if (m_imageLayout == newLayout)
	{
		return;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = m_imageLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (hasStencilComponent(m_format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (m_imageLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (m_imageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (m_imageLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		cb,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	m_imageLayout = newLayout;
}
