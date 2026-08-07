#include "Renderer.hpp"
#include "Config.hpp"
#include <imgui_impl_vulkan.h>

void Renderer::createCommandPool() {
	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	poolInfo.queueFamilyIndex = graphicsFamilyIndex;
	commandPool = vk::raii::CommandPool(device, poolInfo);
}

void Renderer::createCommandBuffers() {
	commandBuffers.clear();
	commandBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void Renderer::recordCommandBufferDeferred(uint32_t imageIndex) {
	commandBuffers[currentFrame].begin({});

	// transition gBuffers to color attachments
	transition_image_layout(gBuffer.fragPosImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);
	transition_image_layout(gBuffer.normalVectorImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);
	transition_image_layout(gBuffer.albedoColorImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);
	// transition depthImage to depth attachment
	transition_image_layout(depthImage.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,{}, vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

	vk::RenderingAttachmentInfo fragPosAttachmentInfo = {};
	fragPosAttachmentInfo.imageView                   = gBuffer.fragPosImageView;
	fragPosAttachmentInfo.imageLayout                 = vk::ImageLayout::eColorAttachmentOptimal;
	fragPosAttachmentInfo.loadOp                      = vk::AttachmentLoadOp::eClear;
	fragPosAttachmentInfo.storeOp                     = vk::AttachmentStoreOp::eStore;
	fragPosAttachmentInfo.clearValue                  = clearColor;

	vk::RenderingAttachmentInfo normalVectorAttachmentInfo = {};
	normalVectorAttachmentInfo.imageView                   = gBuffer.normalVectorImageView;
	normalVectorAttachmentInfo.imageLayout                 = vk::ImageLayout::eColorAttachmentOptimal;
	normalVectorAttachmentInfo.loadOp                      = vk::AttachmentLoadOp::eClear;
	normalVectorAttachmentInfo.storeOp                     = vk::AttachmentStoreOp::eStore;
	normalVectorAttachmentInfo.clearValue                  = clearColor;

	vk::RenderingAttachmentInfo albedoAttachmentInfo = {};
	albedoAttachmentInfo.imageView                   = gBuffer.albedoColorImageView;
	albedoAttachmentInfo.imageLayout                 = vk::ImageLayout::eColorAttachmentOptimal;
	albedoAttachmentInfo.loadOp                      = vk::AttachmentLoadOp::eClear;
	albedoAttachmentInfo.storeOp                     = vk::AttachmentStoreOp::eStore;
	albedoAttachmentInfo.clearValue                  = clearColor;

	vk::RenderingAttachmentInfo depthAttachmentInfo = {};
	depthAttachmentInfo.imageView                   = depthImageView;
	depthAttachmentInfo.imageLayout                 = vk::ImageLayout::eDepthAttachmentOptimal;
	depthAttachmentInfo.loadOp                      = vk::AttachmentLoadOp::eClear;
	depthAttachmentInfo.storeOp                     = vk::AttachmentStoreOp::eDontCare;
	depthAttachmentInfo.clearValue                  = clearDepth;

	std::vector<vk::RenderingAttachmentInfo> colorAttachments = {fragPosAttachmentInfo, normalVectorAttachmentInfo, albedoAttachmentInfo};

	vk::RenderingInfo geometryRenderingInfo    = {};
	geometryRenderingInfo.renderArea.offset    = vk::Offset2D(0, 0);
	geometryRenderingInfo.renderArea.extent    = swapChainExtent;
	geometryRenderingInfo.layerCount           = 1;
	geometryRenderingInfo.colorAttachmentCount = 3;
	geometryRenderingInfo.pColorAttachments    = colorAttachments.data();
	geometryRenderingInfo.pDepthAttachment     = &depthAttachmentInfo;

	commandBuffers[currentFrame].beginRendering(geometryRenderingInfo);
	commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, geometryPipeline);

	// Set the dynamic states of Scissor and Viewport
	commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
	commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

	// bind descriptors
	commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *geometryPipelineLayout,0, *globalDescriptorSets[currentFrame],{});

	for (const auto& gameObject : gameObjects) {
		commandBuffers[currentFrame].bindVertexBuffers(0, models[gameObject.modelIndex].getVertexBuffer(), {0});
		commandBuffers[currentFrame].bindIndexBuffer(models[gameObject.modelIndex].getIndexBuffer(), 0, vk::IndexType::eUint32);
		commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *geometryPipelineLayout, 1, *gameObject.descriptorSets[currentFrame], {});
		commandBuffers[currentFrame].drawIndexed(models[0].getIndexCount(), 1, 0, 0, 0);
	}

	commandBuffers[currentFrame].endRendering();

	transition_image_layout(gBuffer.fragPosImage.image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eShaderRead, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eFragmentShader, vk::ImageAspectFlagBits::eColor);
	transition_image_layout(gBuffer.normalVectorImage.image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eShaderRead, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eFragmentShader, vk::ImageAspectFlagBits::eColor);
	transition_image_layout(gBuffer.albedoColorImage.image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eShaderRead, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eFragmentShader, vk::ImageAspectFlagBits::eColor);

	transition_image_layout(swapChainImages[imageIndex], vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);

	vk::RenderingAttachmentInfo attachmentInfo;
	attachmentInfo.imageView = swapChainImageViews[imageIndex];
	attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
	attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
	attachmentInfo.clearValue = clearColor;

	vk::RenderingInfo lightingRenderingInfo    = {};
	lightingRenderingInfo.renderArea.offset    = vk::Offset2D(0, 0);
	lightingRenderingInfo.renderArea.extent    = swapChainExtent;
	lightingRenderingInfo.layerCount           = 1;
	lightingRenderingInfo.colorAttachmentCount = 1;
	lightingRenderingInfo.pColorAttachments    = &attachmentInfo;


	commandBuffers[currentFrame].beginRendering(lightingRenderingInfo);
	commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, lightingPipeline);

	// Set the dynamic states of Scissor and Viewport
	commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
	commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

	// bind descriptors
	commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *lightingPipelineLayout,0, *gBufferDescriptorSets[currentFrame],{});

	// set push constants
	commandBuffers[currentFrame].pushConstants<int32_t>(*lightingPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, static_cast<int32_t>(currentDebugState));

	// big trangle trick
	commandBuffers[currentFrame].draw(3, 1, 0, 0);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffers[currentFrame]); // part of lighting pass

	commandBuffers[currentFrame].endRendering();

	transition_image_layout(swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {}, vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe, vk::ImageAspectFlagBits::eColor);

	commandBuffers[currentFrame].end();
}

void Renderer::recordCommandBuffer(uint32_t imageIndex) {
	commandBuffers[currentFrame].begin({});

	// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
	transition_image_layout(
		swapChainImages[imageIndex],
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eColorAttachmentWrite,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::ImageAspectFlagBits::eColor);

	transition_image_layout(
		vk::Image(depthImage.image),
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthAttachmentOptimal,
		{},
		vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::PipelineStageFlagBits2::eTopOfPipe,
		vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

	vk::RenderingAttachmentInfo attachmentInfo = {};
	attachmentInfo.imageView                   = colorImageView;
	attachmentInfo.imageLayout                 = vk::ImageLayout::eColorAttachmentOptimal;
	attachmentInfo.loadOp                      = vk::AttachmentLoadOp::eClear;
	attachmentInfo.storeOp                     = vk::AttachmentStoreOp::eDontCare;
	attachmentInfo.clearValue                  = clearColor;
	attachmentInfo.resolveImageView            = swapChainImageViews[imageIndex];
	attachmentInfo.resolveImageLayout          = vk::ImageLayout::eColorAttachmentOptimal;
	attachmentInfo.resolveMode                 = vk::ResolveModeFlagBits::eAverage;

	vk::RenderingAttachmentInfo depthAttachmentInfo = {};
	depthAttachmentInfo.imageView                   = depthImageView;
	depthAttachmentInfo.imageLayout                 = vk::ImageLayout::eDepthAttachmentOptimal;
	depthAttachmentInfo.loadOp                      = vk::AttachmentLoadOp::eClear;
	depthAttachmentInfo.storeOp                     = vk::AttachmentStoreOp::eDontCare;
	depthAttachmentInfo.clearValue                  = clearDepth;

	vk::RenderingInfo renderingInfo    = {};
	renderingInfo.renderArea.offset    = vk::Offset2D(0, 0);
	renderingInfo.renderArea.extent    = swapChainExtent;
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments    = &attachmentInfo;
	renderingInfo.pDepthAttachment     = &depthAttachmentInfo;

	commandBuffers[currentFrame].beginRendering(renderingInfo);
	commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
	commandBuffers[currentFrame].bindVertexBuffers(0, models[0].getVertexBuffer(), {0});
	commandBuffers[currentFrame].bindIndexBuffer(models[0].getIndexBuffer(), 0, vk::IndexType::eUint32);

	// Set the dynamic states of Scissor and Viewport
	commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
	commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

	// bind descriptors
	commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout,0, *globalDescriptorSets[currentFrame],{});

	for (const auto& gameObject : gameObjects) {
		commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 1, *gameObject.descriptorSets[currentFrame], {});
		commandBuffers[currentFrame].drawIndexed(models[0].getIndexCount(), 1, 0, 0, 0);
	}

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffers[currentFrame]);

	commandBuffers[currentFrame].endRendering();

	transition_image_layout(
		swapChainImages[imageIndex],
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,
		{},
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		vk::PipelineStageFlagBits2::eBottomOfPipe,
		vk::ImageAspectFlagBits::eColor);

	commandBuffers[currentFrame].end();
}

