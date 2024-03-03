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

struct State
{
    SDL_Window *window = nullptr;
    CarpVk carpVk;
    VulkanInstanceBuilder builder;

    VkDescriptorPool pool = {};
    VkDescriptorSetLayout descriptorSetLayout = {};

    VkPipelineLayout pipelineLayout = {};
    VkPipeline pipeline = {};

    VkShaderModule shaderModules[2] = {};

    Image image = {};
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
    State *state = (State *) carpVk.destroyBuffersData;

    VkDevice device = getVkDevice();

    destroyImage(state->image);

    destroyShaderModule(state->shaderModules, 2);

    vkDestroyPipeline(device, state->pipeline, nullptr);
    vkDestroyPipelineLayout(device, state->pipelineLayout, nullptr);
    state->pipelineLayout = nullptr;
    if(state->pool)
    {
        vkDestroyDescriptorPool(device, state->pool, nullptr);
        state->pool = nullptr;
    }
}



bool sCreateDescriptorPool(State& state)
{
    VkDevice device = getVkDevice();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 0;
    poolInfo.pPoolSizes = nullptr;
    poolInfo.maxSets = 1024;

    VK_CHECK_CALL(vkCreateDescriptorPool(device, &poolInfo, nullptr, &state.pool));
    //if()

    return true;
}

#if 0
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

    VK_CHECK_CALL(vkCreateDescriptorPool(device, &poolInfo, nullptr, &state.pool));

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = state.pool;
    allocInfo.descriptorSetCount = 0;
    allocInfo.pSetLayouts = &state.layout;

    //VkDescriptorSet descriptorSet = 0;
    //VK_CHECK_CALL(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
    //ASSERT(descriptorSet);
*/

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;

    VK_CHECK_CALL(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &state.pipelineLayout));

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

    VkResult vkResult = vkCreateShadersEXT_T(device, 2, shaderCreateInfos, nullptr, state.shaders);
    if(vkResult != VK_SUCCESS)
    {
        printf("Failed to create shaders\n");
        return false;
    }
    return true;
}
#endif


bool createGraphicsPipeline(State& state, const GPBuilder& builder)
{
    VkDevice device = getVkDevice();

    VkPipelineVertexInputStateCreateInfo vertexInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo assemblyInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    assemblyInfo.topology = builder.topology;
    assemblyInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportInfo.scissorCount = 1;
    viewportInfo.viewportCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterInfo.lineWidth = 1.0f;
    if(builder.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    {
        rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // VK_FRONT_FACE_CLOCKWISE;
        rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    }
    else
    {
        rasterInfo.cullMode = VK_CULL_MODE_NONE;
        // notice VkPhysicalDeviceFeatures .fillModeNonSolid = VK_TRUE required
        //rasterInfo.polygonMode = VK_POLYGON_MODE_LINE;
    }
    VkPipelineMultisampleStateCreateInfo multiSampleInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multiSampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthInfo.depthTestEnable = builder.depthTest ? VK_TRUE : VK_FALSE;
    depthInfo.depthWriteEnable = builder.writeDepth ? VK_TRUE : VK_FALSE;
    depthInfo.depthCompareOp = builder.depthCompareOp;

    VkPipelineColorBlendStateCreateInfo blendInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = builder.blendChannelCount,
        .pAttachments = builder.blendChannels,
    };

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = dynamicStates;
    dynamicInfo.dynamicStateCount = ARRAYSIZES(dynamicStates);


    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    createInfo.stageCount = builder.stageInfoCount;
    createInfo.pStages = builder.stageInfos;
    createInfo.pVertexInputState = &vertexInfo;
    createInfo.pInputAssemblyState = &assemblyInfo;
    createInfo.pViewportState = &viewportInfo;
    createInfo.pRasterizationState = &rasterInfo;
    createInfo.pMultisampleState = &multiSampleInfo;
    createInfo.pDepthStencilState = &depthInfo;
    createInfo.pColorBlendState = &blendInfo;
    createInfo.pDynamicState = &dynamicInfo;
    createInfo.renderPass = VK_NULL_HANDLE;
    createInfo.layout = state.pipelineLayout;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;


    //Needed for dynamic rendering

    const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = builder.colorFormatCount,
        .pColorAttachmentFormats = builder.colorFormats,
        .depthAttachmentFormat = builder.depthFormat,
    };

        //if(outPipeline.renderPass == VK_NULL_HANDLE)
    createInfo.pNext = &pipelineRenderingCreateInfo;


    VkPipeline pipeline = 0;
    VK_CHECK_CALL(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline));
    ASSERT(pipeline);

    /*
    if (!VulkanShader::createDescriptor(outPipeline))
    {
        printf("Failed to create graphics pipeline descriptor\n");
        return false;
    }
     */
    //setObjectName((uint64_t)pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, pipelineName);

    state.pipeline = pipeline;
    return pipeline != VK_NULL_HANDLE;
}








bool sDraw(State& state)
{
    CarpVk& carpVk = state.carpVk;
    int width = SCREEN_WIDTH;
    int height = SCREEN_HEIGHT;

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkCommandBuffer commandBuffer = getVkCommandBuffer();

    vkResetCommandBuffer(commandBuffer, 0);
    VK_CHECK_CALL(vkBeginCommandBuffer(commandBuffer, &beginInfo));







#if 0

    VkImageMemoryBarrier imgBarrier = imageBarrier(state.image,
        0, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdPipelineBarrier(commandBuffer,
        //VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imgBarrier);
#endif

    {
        VkImageMemoryBarrier2 imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        imageBarrier.image = state.image.image;
        imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBarrier.subresourceRange.levelCount = 1;
        imageBarrier.subresourceRange.layerCount = 1;

        VkDependencyInfoKHR dep2 = {};
        dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        dep2.imageMemoryBarrierCount = 1;
        dep2.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(commandBuffer, &dep2);

    }




    //vkCmdResetQueryPool(vulk->commandBuffer, vulk->queryPools[vulk->frameIndex], 0, QUERY_COUNT);
    //vulk->currentStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;


    // New structures are used to define the attachments used in dynamic rendering
    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = state.image.view;
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

    //vkCmdBindDescriptorSets(vulk->commandBuffer, bindPoint, pipelineWithDescriptor.pipelineLayout,
    //    0, 1, &pipelineWithDescriptor.descriptor.descriptorSets[index], 0, NULL);


    VkViewport viewport = { 0.0f, float(height), float(width), -float(height), 0.0f, 1.0f };
    VkRect2D scissors = { { 0, 0 }, { u32(width), u32(height) } };

    vkCmdSetScissor(commandBuffer, 0, 1, &scissors);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(commandBuffer, bindPoint, state.pipeline);

    /*
    // No more pipelines required, everything is bound at command buffer level
    // This also means that we need to explicitly set a lot of the state to be spec compliant

    vkCmdSetViewportWithCountEXT_T(commandBuffer, 1, &viewport);
    vkCmdSetScissorWithCountEXT_T(commandBuffer, 1, &scissors);
    vkCmdSetCullModeEXT_T(commandBuffer, VK_CULL_MODE_BACK_BIT);
    vkCmdSetFrontFaceEXT_T(commandBuffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetDepthTestEnableEXT_T(commandBuffer,  VK_FALSE); //, VK_TRUE);
    vkCmdSetDepthWriteEnableEXT_T(commandBuffer,  VK_FALSE) ; //VK_TRUE);
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
*/



    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // End dynamic rendering
    vkCmdEndRendering(commandBuffer);
    return true;

}


int sRun(State &state)
{
    std::vector<char> fragShaderCode;
    std::vector<char> vertShaderCode;

    if(!sReadFile("assets/shader/vert.spv", vertShaderCode))
    {
        printf("Failed to read vertex shader\n");
        return false;
    }
    if(!sReadFile("assets/shader/frag.spv", fragShaderCode))
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
        SDL_WINDOW_VULKAN
        //SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
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
    if(!instanceBuilderFinish(builder))
    {
        printf("Failed to initialize vulkan.\n");
        return 1;
    }
    printf("Success on creating instance\n");


    VkSurfaceKHR surface;
    if(SDL_Vulkan_CreateSurface(state.window, getVkInstance(), nullptr, &surface) == SDL_FALSE)
    {
        printf("Failed to create surface\n");
        return 2;
    }
    if(surface == nullptr)
    {
        printf("Failed to create surface\n");
        return 2;
    }
    setVkSurface(surface);

    if(!createPhysicalDevice(true))
    {
        printf("Failed to create physical device\n");
        return 3;
    }

    if(!createDeviceWithQueues(builder))
    {
        printf("Failed to create logical device with queues\n");
        return 4;
    }

    if(!createSwapchain(VSyncType::IMMEDIATE_NO_VSYNC, SCREEN_WIDTH, SCREEN_HEIGHT))
    {
        printf("Failed to create swapchain\n");
        return 5;
    }
    if(!finalizeInit(carpVk))
    {
        printf("Failed to finialize initializing vulkan\n");
        return 6;
    }
    VkDevice device = getVkDevice();


    carpVk.destroyBuffers = sDeinitShaders;
    carpVk.destroyBuffersData = &state;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;

    VK_CHECK_CALL(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &state.pipelineLayout));

    if(!createShader(vertShaderCode.data(), vertShaderCode.size(), state.shaderModules[0]))
    {
        printf("Failed to create vert shader!\n");
        return 9;
    }

    if(!createShader(fragShaderCode.data(), fragShaderCode.size(), state.shaderModules[1]))
    {
        printf("Failed to create vert shader!\n");
        return 9;
    }
    const CarpSwapChainFormats& swapchainFormats = getSwapChainFormats();
    const VkFormat colorFormats[] = {
        (VkFormat)swapchainFormats.defaultColorFormat
    };

    VkPipelineColorBlendAttachmentState blendStates[] = {
        {
            .blendEnable = VK_FALSE,
            //.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
        },
    };

    const VkPipelineShaderStageCreateInfo stageInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = state.shaderModules[0],
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = state.shaderModules[1],
            .pName = "main",
        }
    };


    GPBuilder gpBuilder = {
        .stageInfos = stageInfos,
        .colorFormats = colorFormats,
        .blendChannels = blendStates,
        .stageInfoCount = ARRAYSIZES(stageInfos),
        .colorFormatCount = ARRAYSIZES(colorFormats),
        .blendChannelCount = ARRAYSIZES(blendStates),
    };


    if(!createGraphicsPipeline(state, gpBuilder))
    {
        printf("Failed to create graphics pipeline\n");
        return 8;
    }

    if(!createImage(SCREEN_WIDTH, SCREEN_HEIGHT, swapchainFormats.defaultColorFormat,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        state.image))
    {
        printf("Failed to create image\n");
        return 8;
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
            beginFrame();
            sDraw(state);
            presentImage(state.image);
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
