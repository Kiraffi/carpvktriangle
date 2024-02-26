#include <vulkan/vulkan_core.h>
//#include <volk.h>
#include <stdio.h>

#include <carpvk.h>
#include <common.h>

#include <SDL3/SDL.h>
#include "SDL_vulkan.h"
#include "../external/carpvk/src/carpvk.h"

#include <vector>
#include <fstream>

#define VK_CHECK_CALL(call) do { \
    VkResult callResult = call; \
    if(callResult != VkResult::VK_SUCCESS) \
        printf("Result: %i\n", int(callResult)); \
    ASSERT(callResult == VkResult::VK_SUCCESS); \
    } while(0)


static const int SCREEN_WIDTH = 1024;
static const int SCREEN_HEIGHT = 768;





PFN_vkCreateShadersEXT vkCreateShadersEXT_T{ VK_NULL_HANDLE };
PFN_vkDestroyShaderEXT vkDestroyShaderEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT_T{ VK_NULL_HANDLE };
PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT_T{ VK_NULL_HANDLE };

// VK_EXT_shader_objects requires render passes to be dynamic
PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_T{ VK_NULL_HANDLE };
PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_T{ VK_NULL_HANDLE };

PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT_T{ VK_NULL_HANDLE };
PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT_T{ VK_NULL_HANDLE };

PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT_T = {};

PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT_T{ VK_NULL_HANDLE };









struct State
{
    SDL_Window *window = nullptr;
    CarpVk carpVk;
    VulkanInstanceBuilder builder;

    VkDescriptorPool pool = nullptr;
    VkDescriptorSetLayout layout = nullptr;

    VkPipelineLayout pipeline = nullptr;

    VkShaderEXT shaders[2];
};

static bool sReadFile(const char* filename, std::vector<char>& outBuffer)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        printf("Failed to load file: %s\n", filename);
        return false;
    }

    size_t fileSize = (size_t) file.tellg();
    outBuffer.resize(fileSize);
    file.seekg(0);
    file.read(outBuffer.data(), fileSize);
    file.close();

    return true;
}

void sDeinitShaders(CarpVk& carpVk)
{

    PFN_vkDestroyShaderEXT vkDestroyShaderEXT{ VK_NULL_HANDLE };
    vkDestroyShaderEXT = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(carpVk.device, "vkDestroyShaderEXT"));
    if(carpVk.destroyBuffersData == nullptr || vkDestroyShaderEXT == nullptr)
    {
        return;
    }


    State *state = (State *) carpVk.destroyBuffersData;
    vkDestroyPipelineLayout(carpVk.device, state->pipeline, nullptr);
    state->pipeline = nullptr;
    for (int i = 0; i < SDL_arraysize(state->shaders); ++i)
    {
        if(state->shaders[i] == nullptr)
        {
            continue;
        }
        vkDestroyShaderEXT(carpVk.device, state->shaders[i], nullptr);
        state->shaders[i] = nullptr;
    }
    if(state->pool)
    {
        vkDestroyDescriptorPool(carpVk.device, state->pool, nullptr);
        state->pool = nullptr;
    }
}


bool sCreateShaders(State& state,
    const char* vertCode, int vertCodeSize,
    const char* fragCode, int fragCodeSize)
{
/*
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 0;
    poolInfo.pPoolSizes = nullptr;
    poolInfo.maxSets = 1024;

    VK_CHECK_CALL(vkCreateDescriptorPool(state.carpVk.device, &poolInfo, nullptr, &state.pool));

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = state.pool;
    allocInfo.descriptorSetCount = 0;
    allocInfo.pSetLayouts = &state.layout;

    //VkDescriptorSet descriptorSet = 0;
    //VK_CHECK_CALL(vkAllocateDescriptorSets(state.carpVk.device, &allocInfo, &descriptorSet));
    //ASSERT(descriptorSet);
*/

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;

    VK_CHECK_CALL(vkCreatePipelineLayout(state.carpVk.device, &pipelineLayoutCreateInfo, nullptr, &state.pipeline));

    if(vkCreateShadersEXT_T == nullptr)
    {
        printf("Failed to load createShadersExt\n");
        return false;
    }

    VkShaderCreateInfoEXT shaderCreateInfos[2] = {};

    //VS
    shaderCreateInfos[0].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    shaderCreateInfos[0].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
    shaderCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderCreateInfos[0].nextStage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderCreateInfos[0].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    shaderCreateInfos[0].pCode = vertCode;
    shaderCreateInfos[0].codeSize = vertCodeSize;
    shaderCreateInfos[0].pName = "main";
    shaderCreateInfos[0].setLayoutCount = 0;
    shaderCreateInfos[0].pSetLayouts = nullptr; // &state.layout;

    // FS
    shaderCreateInfos[1].sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    shaderCreateInfos[1].flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
    shaderCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderCreateInfos[1].nextStage = 0;
    shaderCreateInfos[1].codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    shaderCreateInfos[1].pCode = fragCode;
    shaderCreateInfos[1].codeSize = fragCodeSize;
    shaderCreateInfos[1].pName = "main";
    shaderCreateInfos[1].setLayoutCount = 0;
    shaderCreateInfos[1].pSetLayouts = nullptr; // &state.layout;

    VkResult vkResult = vkCreateShadersEXT_T(state.carpVk.device, 2, shaderCreateInfos, nullptr, state.shaders);
    if(vkResult != VK_SUCCESS)
    {
        printf("Failed to create shaders\n");
        return false;
    }
    return true;
}
bool sPresent(State& state)
{
    CarpVk& carpVk = state.carpVk;
    int64_t frameIndex = state.carpVk.frameIndex;
    VkCommandBuffer commandBuffer = carpVk.commandBuffers[frameIndex];




    // Prepare image for presenting.


    VkImageMemoryBarrier presentBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    presentBarrier.dstAccessMask = 0;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    //barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    presentBarrier.srcQueueFamilyIndex = carpVk.queueIndex;
    presentBarrier.dstQueueFamilyIndex = carpVk.queueIndex;
    presentBarrier.image = carpVk.swapchainImages[ carpVk.imageIndex ];
    presentBarrier.subresourceRange.baseMipLevel = 0;
    presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // Andoird error?
    presentBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    presentBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    /*
    VkImageMemoryBarrier presentBarrier =
        VulkanResources::imageBarrier(carpVk.swapchainImages[ carpVk.imageIndex ],
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
*/
    vkCmdPipelineBarrier(commandBuffer,
        //VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    VK_CHECK_CALL(vkEndCommandBuffer(commandBuffer));





    // Submit
    {
        //VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; //VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //VK_PIPELINE_STAGE_TRANSFER_BIT;

        vkResetFences(state.carpVk.device, 1, &state.carpVk.fences[frameIndex]);

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &carpVk.acquireSemaphores[frameIndex];
        submitInfo.pWaitDstStageMask = &submitStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &carpVk.releaseSemaphores[frameIndex];
        VK_CHECK_CALL(vkQueueSubmit(carpVk.queue, 1, &submitInfo, carpVk.fences[frameIndex]));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &carpVk.releaseSemaphores[frameIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &carpVk.swapchain;
        presentInfo.pImageIndices = &carpVk.imageIndex;

        VkResult res = ( vkQueuePresentKHR(carpVk.queue, &presentInfo) );
        //if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || vulk->needToResize)
        //{
        //    resizeSwapchain();
        //    vulk->needToResize = false;
        //    VK_CHECK(vkDeviceWaitIdle(vulk->device));
        //}
        //else
        {
            VK_CHECK_CALL(res);
        }
    }

    //VK_CHECK(vkDeviceWaitIdle(vulk->device));

}

bool sDraw(State& state)
{
    int width = SCREEN_WIDTH;
    int height = SCREEN_HEIGHT;

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    int64_t frameIndex = state.carpVk.frameIndex % CarpVk::FramesInFlight;
    VkCommandBuffer commandBuffer = state.carpVk.commandBuffers[frameIndex];

    vkResetCommandBuffer(commandBuffer, 0);
    VK_CHECK_CALL(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    //vkCmdResetQueryPool(vulk->commandBuffer, vulk->queryPools[vulk->frameIndex], 0, QUERY_COUNT);
    //vulk->currentStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;


    // New structures are used to define the attachments used in dynamic rendering
    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = state.carpVk.swapchainImagesViews[frameIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

    // A single depth stencil attachment info can be used, but they can also be specified separately.
    // When both are specified separately, the only requirement is that the image view is identical.
    VkRenderingAttachmentInfoKHR depthStencilAttachment{};
    depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    /*
    depthStencilAttachment.imageView = depthStencil.view;
    depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };
*/
    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = { 0, 0, u32(width), u32(height) };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    //renderingInfo.pDepthAttachment = &depthStencilAttachment;
    //renderingInfo.pStencilAttachment = &depthStencilAttachment;

    // Begin dynamic rendering
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    VkViewport viewport = { 0.0f, float(height), float(width), -float(height), 0.0f, 1.0f };
    VkRect2D scissors = { { 0, 0 }, { u32(width), u32(height) } };

    // No more pipelines required, everything is bound at command buffer level
    // This also means that we need to explicitly set a lot of the state to be spec compliant

    vkCmdSetViewportWithCountEXT_T(commandBuffer, 1, &viewport);
    vkCmdSetScissorWithCountEXT_T(commandBuffer, 1, &scissors);
    vkCmdSetCullModeEXT_T(commandBuffer, VK_CULL_MODE_BACK_BIT);
    vkCmdSetFrontFaceEXT_T(commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetDepthTestEnableEXT_T(commandBuffer, VK_TRUE);
    vkCmdSetDepthWriteEnableEXT_T(commandBuffer, VK_TRUE);
    vkCmdSetDepthCompareOpEXT_T(commandBuffer, VK_COMPARE_OP_LESS_OR_EQUAL);
    vkCmdSetPrimitiveTopologyEXT_T(commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vkCmdSetRasterizerDiscardEnableEXT_T(commandBuffer, VK_FALSE);
    vkCmdSetPolygonModeEXT_T(commandBuffer, VK_POLYGON_MODE_FILL);
    vkCmdSetRasterizationSamplesEXT_T(commandBuffer, VK_SAMPLE_COUNT_1_BIT);
    vkCmdSetAlphaToCoverageEnableEXT_T(commandBuffer, VK_FALSE);
    vkCmdSetDepthBiasEnableEXT_T(commandBuffer, VK_FALSE);
    vkCmdSetStencilTestEnableEXT_T(commandBuffer, VK_FALSE);
    vkCmdSetPrimitiveRestartEnableEXT_T(commandBuffer, VK_FALSE);

    VkColorBlendEquationEXT colorBlend {
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    vkCmdSetColorBlendEquationEXT_T(commandBuffer, 0, 1, &colorBlend);

    const uint32_t sampleMask = 0xFF;
    vkCmdSetSampleMaskEXT_T(commandBuffer, VK_SAMPLE_COUNT_1_BIT, &sampleMask);

    const VkBool32 colorBlendEnables = false;
    const VkColorComponentFlags colorBlendComponentFlags = 0xf;
    const VkColorBlendEquationEXT colorBlendEquation{};
    vkCmdSetColorBlendEnableEXT_T(commandBuffer, 0, 1, &colorBlendEnables);
    vkCmdSetColorWriteMaskEXT_T(commandBuffer, 0, 1, &colorBlendComponentFlags);


    vkCmdSetVertexInputEXT_T(commandBuffer, 0, nullptr, 0, nullptr);

    // must be > 0
    //vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //    state.pipeline, 0, 0, nullptr, 0, nullptr);

    // Binding the shaders
    VkShaderStageFlagBits stages[2] = { VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT };
    vkCmdBindShadersEXT_T(commandBuffer, 2, stages, state.shaders);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // End dynamic rendering
    vkCmdEndRendering(commandBuffer);

}

bool sBeginFrame(State& state)
{
    CarpVk& carpVk = state.carpVk;
    int64_t& frameIndex = carpVk.frameIndex;
    frameIndex = (frameIndex + 1) % CarpVk::FramesInFlight;

    {
        //ScopedTimer aq("Acquire");
        VK_CHECK_CALL(vkWaitForFences(carpVk.device, 1, &carpVk.fences[frameIndex], VK_TRUE, UINT64_MAX));
    }
    if (carpVk.acquireSemaphores[frameIndex] == VK_NULL_HANDLE)
    {
        return false;
    }
    VkResult res = ( vkAcquireNextImageKHR(carpVk.device, carpVk.swapchain, UINT64_MAX,
        carpVk.acquireSemaphores[frameIndex], VK_NULL_HANDLE, &carpVk.imageIndex));

    //vulk->scratchBufferOffset = vulk->frameIndex * 32u * 1024u * 1024u;
    //vulk->scratchBufferLastFlush = vulk->scratchBufferOffset;
    //vulk->scratchBufferMaxOffset = (vulk->frameIndex + 1) * 32u * 1024u * 1024u;

    //if (res == VK_ERROR_OUT_OF_DATE_KHR)
    //{
    //    if (resizeSwapchain())
    //    {
    //        VK_CHECK(vkDeviceWaitIdle(vulk->device));
    //    }
    //    return false;
    //}
    //else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
    //{
    //    VK_CHECK(res);
    //    return false;
    //}

    return true;

}

int sRun(State &state)
{
    std::vector<char> fragShader;
    std::vector<char> vertShader;

    if(!sReadFile("assets/shader/vert.spv", vertShader))
    {
        printf("Failed to read vertex shader\n");
        return false;
    }
    if(!sReadFile("assets/shader/frag.spv", fragShader))
    {
        printf("Failed to read fragment shader\n");
        return false;
    }

    CarpVk& carpVk = state.carpVk;

    // Create window
    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL3

    // Create an application window with the following settings:
    state.window = SDL_CreateWindow(
        "An SDL3 window",
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );

    // Check that the window was successfully created
    if (state.window == NULL) {
        // In the case that the window could not be made...
        printf("Could not create window: %s\n", SDL_GetError());
        return false;
    }


    const char* extensions[256] = {};
    uint32_t extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

    for(int i = 0; i < extensionCount; ++i)
    {
        extensions[i] = sdlExtensions[i];
    }
#if NDEBUG
#else
    extensions[extensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

    //VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    //VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    //VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    //"VK_LAYER_KHRONOS_shader_object",
    // VK_KHR_SURFACE_EXTENSION_NAME,
    //"VK_KHR_xlib_surface",
    //"VK_KHR_xcb_surface",


    //printExtensions();
    //printLayers();


    VulkanInstanceBuilder builder = instaceBuilder();
    instanceBuilderSetExtensions(builder, extensions, extensionCount);
#if NDEBUG
#else
    instanceBuilderUseDefaultValidationLayers(builder);
#endif
    if(!instanceBuilderFinish(builder, carpVk))
    {
        printf("Failed to initialize vulkan.\n");
        return 1;
    }
    printf("Success on creating instance\n");


    VkSurfaceKHR surface;
    if(SDL_Vulkan_CreateSurface(state.window, carpVk.instance, nullptr, &surface) == SDL_FALSE)
    {
        printf("Failed to create surface\n");
        return 2;
    }
    if(surface == nullptr)
    {
        printf("Failed to create surface\n");
        return 2;
    }
    carpVk.surface = surface;

    if(!createPhysicalDevice(carpVk, false))
    {
        printf("Failed to create physical device\n");
        return 3;
    }

    if(!createDeviceWithQueues(carpVk, builder))
    {
        printf("Failed to create logical device with queues\n");
        return 4;
    }

    if(!createSwapchain(carpVk, VSyncType::IMMEDIATE_NO_VSYNC, SCREEN_WIDTH, SCREEN_HEIGHT))
    {
        printf("Failed to create swapchain\n");
        return 5;
    }
    if(!finalizeInit(carpVk))
    {
        printf("Failed to finialize initializing vulkan\n");
        return 6;
    }

    carpVk.destroyBuffers = sDeinitShaders;
    carpVk.destroyBuffersData = &state;

    {

        vkCreateShadersEXT_T = reinterpret_cast<PFN_vkCreateShadersEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCreateShadersEXT"));
        vkDestroyShaderEXT_T = reinterpret_cast<PFN_vkDestroyShaderEXT>(vkGetDeviceProcAddr(carpVk.device, "vkDestroyShaderEXT"));
        vkCmdBindShadersEXT_T = reinterpret_cast<PFN_vkCmdBindShadersEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdBindShadersEXT"));
        vkGetShaderBinaryDataEXT_T = reinterpret_cast<PFN_vkGetShaderBinaryDataEXT>(vkGetDeviceProcAddr(carpVk.device, "vkGetShaderBinaryDataEXT"));

        vkCmdBeginRenderingKHR_T = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(carpVk.device, "vkCmdBeginRenderingKHR"));
        vkCmdEndRenderingKHR_T = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetDeviceProcAddr(carpVk.device, "vkCmdEndRenderingKHR"));

        vkCmdSetAlphaToCoverageEnableEXT_T = reinterpret_cast<PFN_vkCmdSetAlphaToCoverageEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetAlphaToCoverageEnableEXT"));
        vkCmdSetColorBlendEnableEXT_T = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetColorBlendEnableEXT"));
        vkCmdSetColorWriteMaskEXT_T = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetColorWriteMaskEXT"));
        vkCmdSetCullModeEXT_T = reinterpret_cast<PFN_vkCmdSetCullModeEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetCullModeEXT"));
        vkCmdSetDepthBiasEnableEXT_T = reinterpret_cast<PFN_vkCmdSetDepthBiasEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetDepthBiasEnableEXT"));
        vkCmdSetDepthCompareOpEXT_T = reinterpret_cast<PFN_vkCmdSetDepthCompareOpEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetDepthCompareOpEXT"));
        vkCmdSetDepthTestEnableEXT_T = reinterpret_cast<PFN_vkCmdSetDepthTestEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetDepthTestEnableEXT"));
        vkCmdSetDepthWriteEnableEXT_T = reinterpret_cast<PFN_vkCmdSetDepthWriteEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetDepthWriteEnableEXT"));
        vkCmdSetFrontFaceEXT_T = reinterpret_cast<PFN_vkCmdSetFrontFaceEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetFrontFaceEXT"));
        vkCmdSetPolygonModeEXT_T = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetPolygonModeEXT"));
        vkCmdSetPrimitiveRestartEnableEXT_T = reinterpret_cast<PFN_vkCmdSetPrimitiveRestartEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetPrimitiveRestartEnableEXT"));
        vkCmdSetPrimitiveTopologyEXT_T = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetPrimitiveTopologyEXT"));
        vkCmdSetRasterizationSamplesEXT_T = reinterpret_cast<PFN_vkCmdSetRasterizationSamplesEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetRasterizationSamplesEXT"));
        vkCmdSetRasterizerDiscardEnableEXT_T = reinterpret_cast<PFN_vkCmdSetRasterizerDiscardEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetRasterizerDiscardEnableEXT"));
        vkCmdSetSampleMaskEXT_T = reinterpret_cast<PFN_vkCmdSetSampleMaskEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetSampleMaskEXT"));
        vkCmdSetScissorWithCountEXT_T = reinterpret_cast<PFN_vkCmdSetScissorWithCountEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetScissorWithCountEXT"));
        vkCmdSetStencilTestEnableEXT_T = reinterpret_cast<PFN_vkCmdSetStencilTestEnableEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetStencilTestEnableEXT"));
        vkCmdSetVertexInputEXT_T = reinterpret_cast<PFN_vkCmdSetVertexInputEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetVertexInputEXT"));
        vkCmdSetViewportWithCountEXT_T = reinterpret_cast<PFN_vkCmdSetViewportWithCountEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetViewportWithCountEXT"));;
        vkCmdSetColorBlendEquationEXT_T = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(vkGetDeviceProcAddr(carpVk.device, "vkCmdSetColorBlendEquationEXT"));;
    }
    if(!sCreateShaders(state,
        vertShader.data(), vertShader.size(),
        fragShader.data(), fragShader.size()))
    {
        printf("Failed to create shaders\n");
        return 7;
    }


    bool quit = false;
    //While application is running
    while( !quit )
    {
        //Event handler
        SDL_Event e;

        //Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            switch (e.type)
            {
                case SDL_EVENT_KEY_DOWN:
                    if (e.key.keysym.sym == SDLK_ESCAPE)
                    {
                        quit = true;
                    }
                    break;
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                default:
                    break;
            }
            sBeginFrame(state);
            sDraw(state);
            sPresent(state);
        }
        SDL_Delay(1);
    }

    // The window is open: could enter program loop here (see SDL_PollEvent())
    printf("success\n");
    return 0;
}



int main()
{
    State state = {};
    int returnValue = sRun(state);

    deinitCarpVk(state.carpVk);

    // Close and destroy the window
    SDL_DestroyWindow(state.window);

    // Clean up
    SDL_Quit();



    return returnValue;
}
