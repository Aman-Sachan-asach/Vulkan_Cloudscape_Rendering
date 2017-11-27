#pragma once

#include "BufferUtils.h"
#define VMA_IMPLEMENTATION
#include "../../external/vk_mem_alloc.h"

void BufferUtils::CreateBuffer(VulkanDevice* device, VkBufferUsageFlags allowedUsage, uint32_t size,
								VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// Create buffer
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;	// Size in bytes
	bufferCreateInfo.usage = allowedUsage;	// Which purpose the data in the buffer will be used 
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //VK_SHARING_MODE_EXCLUSIVE specifies that access to any range or 
															  //image subresource of the object will be exclusive to a single queue family at a time.

	if (vkCreateBuffer(device->GetVkDevice(), &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create buffer");
	}

	// Query buffer's memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device->GetVkDevice(), buffer, &memRequirements);

	// Allocate memory for the buffer in the device
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device->GetVkDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate device memory for the buffer");
	}

	// Associate allocated memory with vertex buffer
	vkBindBufferMemory(device->GetVkDevice(), buffer, bufferMemory, 0);
}

void BufferUtils::CreateBufferFromData(VulkanDevice* device, VkCommandPool commandPool, void* bufferData, VkDeviceSize bufferSize,
									VkBufferUsageFlags bufferUsage, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// Create the staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkBufferUsageFlags stagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkMemoryPropertyFlags stagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	BufferUtils::CreateBuffer(device, bufferSize, stagingUsage, stagingProperties, stagingBuffer, stagingBufferMemory);

	// Fill the staging buffer
	void *data;
	vkMapMemory(device->GetVkDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, bufferData, static_cast<size_t>(bufferSize));
	vkUnmapMemory(device->GetVkDevice(), stagingBufferMemory);

	// Create the buffer
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage;
	VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	BufferUtils::CreateBuffer(device, bufferSize, usage, flags, buffer, bufferMemory);

	// Copy data from staging buffer to the actual buffer
	BufferUtils::CopyBuffer(device, commandPool, stagingBuffer, buffer, bufferSize);

	// Destroy the staging buffer and free its memory
	vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), stagingBufferMemory, nullptr);
}

//Copy data from Source Buffer to Destination Buffer
void BufferUtils::CopyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(device, commandPool, commandBuffer);
}

// Allocates device memory of a certain size
VkDeviceMemory BufferUtils::CreateDeviceMemory(VulkanDevice* device, uint32_t size, uint32_t types, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = size;
	memoryAllocateInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(types, propertyFlags);

	VkDeviceMemory deviceMemory;
	if (vkAllocateMemory(device->GetVkDevice(), &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate device memory!");
	}

	return deviceMemory;
}

void BufferUtils::createBufferVMA(VmaAllocator& g_vma_Allocator, VkBufferUsageFlags allowedUsage, uint32_t size,
								  VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc)
{
	VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	vbInfo.size = size;
	vbInfo.usage = allowedUsage;
	vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vbAllocCreateInfo = {};
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vbAllocCreateInfo.flags = 0;

	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &vbInfo, &vbAllocCreateInfo, &buffer, &g_vma_BufferAlloc, nullptr));
}

void BufferUtils::createVertexandIndexBuffersVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator, 
												const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
												VkBuffer& vertexBuffer, VkBuffer& indexBuffer, 
												VmaAllocation& g_vma_VertexBufferAlloc, VmaAllocation& g_vma_IndexBufferAlloc)
{
	// Vertex Buffer
	VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	vbInfo.size = vertices.size() * sizeof(vertices[0]);
	vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vbAllocCreateInfo = {};
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	vbAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingVertexBufferAlloc = VK_NULL_HANDLE;
	VmaAllocationInfo stagingVertexBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &vbInfo, &vbAllocCreateInfo, &stagingVertexBuffer, &stagingVertexBufferAlloc, &stagingVertexBufferAllocInfo));
	
	memcpy(stagingVertexBufferAllocInfo.pMappedData, vertices.data(), vbInfo.size);

	// No need to flush stagingVertexBuffer memory because CPU_ONLY memory is always HOST_COHERENT.

	vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vbAllocCreateInfo.flags = 0;
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &vbInfo, &vbAllocCreateInfo, &vertexBuffer, &g_vma_VertexBufferAlloc, nullptr));

	// Index Buffer
	VkBufferCreateInfo ibInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	ibInfo.size = indices.size() * sizeof(indices[0]);
	ibInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	ibInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo ibAllocCreateInfo = {};
	ibAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	ibAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingIndexBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingIndexBufferAlloc = VK_NULL_HANDLE;
	VmaAllocationInfo stagingIndexBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &ibInfo, &ibAllocCreateInfo, &stagingIndexBuffer, &stagingIndexBufferAlloc, &stagingIndexBufferAllocInfo));

	memcpy(stagingIndexBufferAllocInfo.pMappedData, indices.data(), ibInfo.size);

	// No need to flush stagingIndexBuffer memory because CPU_ONLY memory is always HOST_COHERENT.

	ibInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	ibAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	ibAllocCreateInfo.flags = 0;
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &ibInfo, &ibAllocCreateInfo, &indexBuffer, &g_vma_IndexBufferAlloc, nullptr));

	// Copy buffers
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);
	
	VkBufferCopy vbCopyRegion = {};
	vbCopyRegion.srcOffset = 0;
	vbCopyRegion.dstOffset = 0;
	vbCopyRegion.size = vbInfo.size;
	vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, vertexBuffer, 1, &vbCopyRegion);

	VkBufferCopy ibCopyRegion = {};
	ibCopyRegion.srcOffset = 0;
	ibCopyRegion.dstOffset = 0;
	ibCopyRegion.size = ibInfo.size;
	vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, indexBuffer, 1, &ibCopyRegion);

	endSingleTimeCommands(device, commandPool, commandBuffer);

	vmaDestroyBuffer(g_vma_Allocator, stagingIndexBuffer, stagingIndexBufferAlloc);
	vmaDestroyBuffer(g_vma_Allocator, stagingVertexBuffer, stagingVertexBufferAlloc);
}

void BufferUtils::createBufferFromDataVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
										void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
										VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc)
{
	// Staging Buffer
	VkBufferCreateInfo stagingBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingBufferInfo.size = bufferSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo stagingBufferAllocCreateInfo = {};
	stagingBufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	stagingBufferAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingBufferAlloc = VK_NULL_HANDLE;
	VmaAllocationInfo stagingBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &stagingBufferInfo, &stagingBufferAllocCreateInfo, 
									&stagingBuffer, &stagingBufferAlloc, &stagingBufferAllocInfo));

	//copy data over to staging buffer
	memcpy(stagingBufferAllocInfo.pMappedData, bufferData, bufferSize);

	// No need to flush stagingBuffer memory because CPU_ONLY memory is always HOST_COHERENT.

	//create actual buffer
	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo bufferAllocCreateInfo = {};
	bufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	bufferAllocCreateInfo.flags = 0;
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &bufferInfo, &bufferAllocCreateInfo, &buffer, &g_vma_BufferAlloc, nullptr));

	// Copy data of staging buffer into actual buffer that was passed in
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

	endSingleTimeCommands(device, commandPool, commandBuffer);

	vmaDestroyBuffer(g_vma_Allocator, stagingBuffer, stagingBufferAlloc);
}