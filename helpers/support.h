#include <stdio.h>
#include <vulkan/vulkan.h>

void printUUID(uint8_t *uuid)
{
	uint8_t map[16] = {
	  '0', '1', '2', '3',
	  '4', '5', '6', '7',
	  '8', '9', 'a', 'b',
	  'c', 'd', 'e', 'f'};

	for (uint8_t i = 0; i < VK_UUID_SIZE; i++)
	{
		if (i == 4 || i == 6 || i == 8 || i == 10)
			printf("-");
		printf("%c%c", map[uuid[i] >> 4 & 0xF], map[uuid[i] & 0xF]);
	}
}

void printPhysicalDeviceType(VkPhysicalDeviceType type)
{
	printf("deviceType: ");
	if (type == VK_PHYSICAL_DEVICE_TYPE_CPU)
		printf("CPU\n");
	else if (type == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
		printf("Virtual GPU\n");
	else if (type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		printf("Discrete GPU\n");
	else if (type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		printf("Integrated GPU\n");
	else
		printf("Other\n");
}

void printPhysicalDeviceLimits(VkPhysicalDeviceLimits limits)
{
	printf("maxImageDimension1D: %u\n", limits.maxImageDimension1D);
	printf("maxImageDimension2D: %u\n", limits.maxImageDimension2D);
	printf("maxImageDimension3D: %u\n", limits.maxImageDimension3D);
	printf("maxImageDimensionCube: %u\n", limits.maxImageDimensionCube);
	printf("maxImageArrayLayers: %u\n", limits.maxImageArrayLayers);
	printf("maxTexelBufferElements: %u\n", limits.maxTexelBufferElements);
	printf("maxUniformBufferRange: %u\n", limits.maxUniformBufferRange);
	printf("maxStorageBufferRange: %u\n", limits.maxStorageBufferRange);
	printf("maxPushConstantsSize: %u\n", limits.maxPushConstantsSize);
	printf("maxMemoryAllocationCount: %u\n", limits.maxMemoryAllocationCount);
	printf("maxSamplerAllocationCount: %u\n", limits.maxSamplerAllocationCount);
	printf("bufferImageGranularity: %lu\n", limits.bufferImageGranularity);
	printf("sparseAddressSpaceSize: %lu\n", limits.sparseAddressSpaceSize);
	printf("maxBoundDescriptorSets: %u\n", limits.maxBoundDescriptorSets);
	printf("maxPerStageDescriptorSamplers: %u\n", limits.maxPerStageDescriptorSamplers);
	printf("maxPerStageDescriptorUniformBuffers: %u\n", limits.maxPerStageDescriptorUniformBuffers);
	printf("maxPerStageDescriptorStorageBuffers: %u\n", limits.maxPerStageDescriptorStorageBuffers);
	printf("maxPerStageDescriptorSampledImages: %u\n", limits.maxPerStageDescriptorSampledImages);
	printf("maxPerStageDescriptorStorageImages: %u\n", limits.maxPerStageDescriptorStorageImages);
	printf("maxPerStageDescriptorInputAttachments: %u\n", limits.maxPerStageDescriptorInputAttachments);
	printf("maxPerStageResources: %u\n", limits.maxPerStageResources);
	printf("maxDescriptorSetSamplers: %u\n", limits.maxDescriptorSetSamplers);
	printf("maxDescriptorSetUniformBuffers: %u\n", limits.maxDescriptorSetUniformBuffers);
	printf("maxDescriptorSetUniformBuffersDynamic: %u\n", limits.maxDescriptorSetUniformBuffersDynamic);
	printf("maxDescriptorSetStorageBuffers: %u\n", limits.maxDescriptorSetStorageBuffers);
	printf("maxDescriptorSetStorageBuffersDynamic: %u\n", limits.maxDescriptorSetStorageBuffersDynamic);
	printf("maxDescriptorSetSampledImages: %u\n", limits.maxDescriptorSetSampledImages);
	printf("maxDescriptorSetStorageImages: %u\n", limits.maxDescriptorSetStorageImages);
	printf("maxDescriptorSetInputAttachments: %u\n", limits.maxDescriptorSetInputAttachments);
	printf("maxVertexInputAttributes: %u\n", limits.maxVertexInputAttributes);
	printf("maxVertexInputBindings: %u\n", limits.maxVertexInputBindings);
	printf("maxVertexInputAttributeOffset: %u\n", limits.maxVertexInputAttributeOffset);
	printf("maxVertexInputBindingStride: %u\n", limits.maxVertexInputBindingStride);
	printf("maxVertexOutputComponents: %u\n", limits.maxVertexOutputComponents);
	printf("maxTessellationGenerationLevel: %u\n", limits.maxTessellationGenerationLevel);
	printf("maxTessellationPatchSize: %u\n", limits.maxTessellationPatchSize);
	printf("maxTessellationControlPerVertexInputComponents: %u\n", limits.maxTessellationControlPerVertexInputComponents);
	printf("maxTessellationControlPerVertexOutputComponents: %u\n", limits.maxTessellationControlPerVertexOutputComponents);
	printf("maxTessellationControlPerPatchOutputComponents: %u\n", limits.maxTessellationControlPerPatchOutputComponents);
	printf("maxTessellationControlTotalOutputComponents: %u\n", limits.maxTessellationControlTotalOutputComponents);
	printf("maxTessellationEvaluationInputComponents: %u\n", limits.maxTessellationEvaluationInputComponents);
	printf("maxTessellationEvaluationOutputComponents: %u\n", limits.maxTessellationEvaluationOutputComponents);
	printf("maxGeometryShaderInvocations: %u\n", limits.maxGeometryShaderInvocations);
	printf("maxGeometryInputComponents: %u\n", limits.maxGeometryInputComponents);
	printf("maxGeometryOutputComponents: %u\n", limits.maxGeometryOutputComponents);
	printf("maxGeometryOutputVertices: %u\n", limits.maxGeometryOutputVertices);
	printf("maxGeometryTotalOutputComponents: %u\n", limits.maxGeometryTotalOutputComponents);
	printf("maxFragmentInputComponents: %u\n", limits.maxFragmentInputComponents);
	printf("maxFragmentOutputAttachments: %u\n", limits.maxFragmentOutputAttachments);
	printf("maxFragmentDualSrcAttachments: %u\n", limits.maxFragmentDualSrcAttachments);
	printf("maxFragmentCombinedOutputResources: %u\n", limits.maxFragmentCombinedOutputResources);
	printf("maxComputeSharedMemorySize: %u\n", limits.maxComputeSharedMemorySize);
	printf("maxComputeWorkGroupCount0: %u\n", limits.maxComputeWorkGroupCount[0]);
	printf("maxComputeWorkGroupCount1: %u\n", limits.maxComputeWorkGroupCount[1]);
	printf("maxComputeWorkGroupCount2: %u\n", limits.maxComputeWorkGroupCount[2]);
	printf("maxComputeWorkGroupInvocations: %u\n", limits.maxComputeWorkGroupInvocations);
	printf("maxComputeWorkGroupSize0: %u\n", limits.maxComputeWorkGroupSize[0]);
	printf("maxComputeWorkGroupSize1: %u\n", limits.maxComputeWorkGroupSize[1]);
	printf("maxComputeWorkGroupSize2: %u\n", limits.maxComputeWorkGroupSize[2]);
	printf("subPixelPrecisionBits: %u\n", limits.subPixelPrecisionBits);
	printf("subTexelPrecisionBits: %u\n", limits.subTexelPrecisionBits);
	printf("mipmapPrecisionBits: %u\n", limits.mipmapPrecisionBits);
	printf("maxDrawIndexedIndexValue: %u\n", limits.maxDrawIndexedIndexValue);
	printf("maxDrawIndirectCount: %u\n", limits.maxDrawIndirectCount);
	printf("maxSamplerLodBias: %g\n", limits.maxSamplerLodBias);
	printf("maxSamplerAnisotropy: %g\n", limits.maxSamplerAnisotropy);
	printf("maxViewports: %u\n", limits.maxViewports);
	printf("maxViewportDimensions0: %u\n", limits.maxViewportDimensions[0]);
	printf("maxViewportDimensions1: %u\n", limits.maxViewportDimensions[1]);
	printf("viewportBoundsRange0: %g\n", limits.viewportBoundsRange[0]);
	printf("viewportBoundsRange1: %g\n", limits.viewportBoundsRange[1]);
	printf("viewportSubPixelBits: %u\n", limits.viewportSubPixelBits);
	printf("minMemoryMapAlignment: %lu\n", limits.minMemoryMapAlignment);
	printf("minTexelBufferOffsetAlignment: %lu\n", limits.minTexelBufferOffsetAlignment);
	printf("minUniformBufferOffsetAlignment: %lu\n", limits.minUniformBufferOffsetAlignment);
	printf("minStorageBufferOffsetAlignment: %lu\n", limits.minStorageBufferOffsetAlignment);
	printf("minTexelOffset: %d\n", limits.minTexelOffset);
	printf("maxTexelOffset: %u\n", limits.maxTexelOffset);
	printf("minTexelGatherOffset: %d\n", limits.minTexelGatherOffset);
	printf("maxTexelGatherOffset: %u\n", limits.maxTexelGatherOffset);
	printf("minInterpolationOffset: %g\n", limits.minInterpolationOffset);
	printf("maxInterpolationOffset: %g\n", limits.maxInterpolationOffset);
	printf("subPixelInterpolationOffsetBits: %u\n", limits.subPixelInterpolationOffsetBits);
	printf("maxFramebufferWidth: %u\n", limits.maxFramebufferWidth);
	printf("maxFramebufferHeight: %u\n", limits.maxFramebufferHeight);
	printf("maxFramebufferLayers: %u\n", limits.maxFramebufferLayers);
	printf("framebufferColorSampleCounts: %x\n", limits.framebufferColorSampleCounts);
	printf("framebufferDepthSampleCounts: %x\n", limits.framebufferDepthSampleCounts);
	printf("framebufferStencilSampleCounts: %x\n", limits.framebufferStencilSampleCounts);
	printf("framebufferNoAttachmentsSampleCounts: %x\n", limits.framebufferNoAttachmentsSampleCounts);
	printf("maxColorAttachments: %u\n", limits.maxColorAttachments);
	printf("sampledImageColorSampleCounts: %x\n", limits.sampledImageColorSampleCounts);
	printf("sampledImageIntegerSampleCounts: %x\n", limits.sampledImageIntegerSampleCounts);
	printf("sampledImageDepthSampleCounts: %x\n", limits.sampledImageDepthSampleCounts);
	printf("sampledImageStencilSampleCounts: %x\n", limits.sampledImageStencilSampleCounts);
	printf("storageImageSampleCounts: %x\n", limits.storageImageSampleCounts);
	printf("maxSampleMaskWords: %u\n", limits.maxSampleMaskWords);
	printf("timestampComputeAndGraphics: %d\n", limits.timestampComputeAndGraphics);
	printf("timestampPeriod: %g\n", limits.timestampPeriod);
	printf("maxClipDistances: %u\n", limits.maxClipDistances);
	printf("maxCullDistances: %u\n", limits.maxCullDistances);
	printf("maxCombinedClipAndCullDistances: %u\n", limits.maxCombinedClipAndCullDistances);
	printf("discreteQueuePriorities: %u\n", limits.discreteQueuePriorities);
	printf("pointSizeRange0: %g\n", limits.pointSizeRange[0]);
	printf("pointSizeRange1: %g\n", limits.pointSizeRange[1]);
	printf("lineWidthRange0: %g\n", limits.lineWidthRange[0]);
	printf("lineWidthRange1: %g\n", limits.lineWidthRange[1]);
	printf("pointSizeGranularity: %g\n", limits.pointSizeGranularity);
	printf("lineWidthGranularity: %g\n", limits.lineWidthGranularity);
	printf("strictLines: %d\n", limits.strictLines);
	printf("standardSampleLocations: %d\n", limits.standardSampleLocations);
	printf("optimalBufferCopyOffsetAlignment: %lu\n", limits.optimalBufferCopyOffsetAlignment);
	printf("optimalBufferCopyRowPitchAlignment: %lu\n", limits.optimalBufferCopyRowPitchAlignment);
	printf("nonCoherentAtomSize: %lu\n", limits.nonCoherentAtomSize);
}

void printPhysicalDeviceSparseProperties(VkPhysicalDeviceSparseProperties properties)
{
	printf("residencyStandard2DBlockShape: %d\n", properties.residencyStandard2DBlockShape);
	printf("residencyStandard2DMultisampleBlockShape: %d\n", properties.residencyStandard2DMultisampleBlockShape);
	printf("residencyStandard3DBlockShape: %d\n", properties.residencyStandard3DBlockShape);
	printf("residencyAlignedMipSize: %d\n", properties.residencyAlignedMipSize);
	printf("residencyNonResidentStrict: %d\n", properties.residencyNonResidentStrict);
}

void printPhysicalDeviceProperties(VkPhysicalDeviceProperties properties)
{
	printf("apiVersion: %d.%d.%d\n", VK_VERSION_MAJOR(properties.apiVersion),
	  VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
	printf("driverVersion: %d.%d.%d\n", VK_VERSION_MAJOR(properties.driverVersion),
	  VK_VERSION_MINOR(properties.driverVersion), VK_VERSION_PATCH(properties.driverVersion));
	printf("vendorID: %d\n", properties.vendorID);
	printf("deviceID: %d\n", properties.deviceID);
	printPhysicalDeviceType(properties.deviceType);
	printf("deviceName: %s\n", properties.deviceName);
	printf("pipelineCacheUUID: ");
	printUUID(properties.pipelineCacheUUID);
	printf("\n");
	printPhysicalDeviceLimits(properties.limits);
	printPhysicalDeviceSparseProperties(properties.sparseProperties);
}

void printPhysicalDeviceFeatures(VkPhysicalDeviceFeatures features)
{
	printf("robustBufferAccess: %d\n", features.robustBufferAccess);
	printf("fullDrawIndexUint32: %d\n", features.fullDrawIndexUint32);
	printf("imageCubeArray: %d\n", features.imageCubeArray);
	printf("independentBlend: %d\n", features.independentBlend);
	printf("geometryShader: %d\n", features.geometryShader);
	printf("tessellationShader: %d\n", features.tessellationShader);
	printf("sampleRateShading: %d\n", features.sampleRateShading);
	printf("dualSrcBlend: %d\n", features.dualSrcBlend);
	printf("logicOp: %d\n", features.logicOp);
	printf("multiDrawIndirect: %d\n", features.multiDrawIndirect);
	printf("drawIndirectFirstInstance: %d\n", features.drawIndirectFirstInstance);
	printf("depthClamp: %d\n", features.depthClamp);
	printf("depthBiasClamp: %d\n", features.depthBiasClamp);
	printf("fillModeNonSolid: %d\n", features.fillModeNonSolid);
	printf("depthBounds: %d\n", features.depthBounds);
	printf("wideLines: %d\n", features.wideLines);
	printf("largePoints: %d\n", features.largePoints);
	printf("alphaToOne: %d\n", features.alphaToOne);
	printf("multiViewport: %d\n", features.multiViewport);
	printf("samplerAnisotropy: %d\n", features.samplerAnisotropy);
	printf("textureCompressionETC2: %d\n", features.textureCompressionETC2);
	printf("textureCompressionASTC_LDR: %d\n", features.textureCompressionASTC_LDR);
	printf("textureCompressionBC: %d\n", features.textureCompressionBC);
	printf("occlusionQueryPrecise: %d\n", features.occlusionQueryPrecise);
	printf("pipelineStatisticsQuery: %d\n", features.pipelineStatisticsQuery);
	printf("vertexPipelineStoresAndAtomics: %d\n", features.vertexPipelineStoresAndAtomics);
	printf("fragmentStoresAndAtomics: %d\n", features.fragmentStoresAndAtomics);
	printf("shaderTessellationAndGeometryPointSize: %d\n", features.shaderTessellationAndGeometryPointSize);
	printf("shaderImageGatherExtended: %d\n", features.shaderImageGatherExtended);
	printf("shaderStorageImageExtendedFormats: %d\n", features.shaderStorageImageExtendedFormats);
	printf("shaderStorageImageMultisample: %d\n", features.shaderStorageImageMultisample);
	printf("shaderStorageImageReadWithoutFormat: %d\n", features.shaderStorageImageReadWithoutFormat);
	printf("shaderStorageImageWriteWithoutFormat: %d\n", features.shaderStorageImageWriteWithoutFormat);
	printf("shaderUniformBufferArrayDynamicIndexing: %d\n", features.shaderUniformBufferArrayDynamicIndexing);
	printf("shaderSampledImageArrayDynamicIndexing: %d\n", features.shaderSampledImageArrayDynamicIndexing);
	printf("shaderStorageBufferArrayDynamicIndexing: %d\n", features.shaderStorageBufferArrayDynamicIndexing);
	printf("shaderStorageImageArrayDynamicIndexing: %d\n", features.shaderStorageImageArrayDynamicIndexing);
	printf("shaderClipDistance: %d\n", features.shaderClipDistance);
	printf("shaderCullDistance: %d\n", features.shaderCullDistance);
	printf("shaderFloat64: %d\n", features.shaderFloat64);
	printf("shaderInt64: %d\n", features.shaderInt64);
	printf("shaderInt16: %d\n", features.shaderInt16);
	printf("shaderResourceResidency: %d\n", features.shaderResourceResidency);
	printf("shaderResourceMinLod: %d\n", features.shaderResourceMinLod);
	printf("sparseBinding: %d\n", features.sparseBinding);
	printf("sparseResidencyBuffer: %d\n", features.sparseResidencyBuffer);
	printf("sparseResidencyImage2D: %d\n", features.sparseResidencyImage2D);
	printf("sparseResidencyImage3D: %d\n", features.sparseResidencyImage3D);
	printf("sparseResidency2Samples: %d\n", features.sparseResidency2Samples);
	printf("sparseResidency4Samples: %d\n", features.sparseResidency4Samples);
	printf("sparseResidency8Samples: %d\n", features.sparseResidency8Samples);
	printf("sparseResidency16Samples: %d\n", features.sparseResidency16Samples);
	printf("sparseResidencyAliased: %d\n", features.sparseResidencyAliased);
	printf("variableMultisampleRate: %d\n", features.variableMultisampleRate);
	printf("inheritedQueries: %d\n", features.inheritedQueries);
}

void printExtensionProperties(VkExtensionProperties *extensions, uint32_t count)
{
	printf("Extension Count: %u\n", count);
	for (uint32_t i = 0; i < count; i++)
		printf("%s %u\n", extensions[i].extensionName, extensions[i].specVersion);
}
