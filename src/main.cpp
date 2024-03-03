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
    state->pipelineLayout = {};
    if(state->pool)
    {
        vkDestroyDescriptorPool(device, state->pool, nullptr);
        state->pool = {};
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










bool sDraw(State& state)
{
    CarpVk& carpVk = state.carpVk;
    int width = SCREEN_WIDTH;
    int height = SCREEN_HEIGHT;

    VkCommandBuffer commandBuffer = getVkCommandBuffer();
    {
        VkImageMemoryBarrier2 imgBarrier[] = {
            imageBarrier(state.image, 
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        };
        VkDependencyInfoKHR dep2 = {};
        dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        dep2.imageMemoryBarrierCount = ARRAYSIZES(imgBarrier);
        dep2.pImageMemoryBarriers = imgBarrier;

        vkCmdPipelineBarrier2(commandBuffer, &dep2);

    }

    // New structures are used to define the attachments used in dynamic rendering
    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = state.image.view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

    /*
    // A single depth stencil attachment info can be used, but they can also be specified separately.
    // When both are specified separately, the only requirement is that the image view is identical.
    VkRenderingAttachmentInfoKHR depthStencilAttachment{};
    depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
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
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.pipeline);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // End dynamic rendering
    vkCmdEndRendering(commandBuffer);
    return true;
}

VkSurfaceKHR createSurface(VkInstance instance, void* ptr)
{
    VkSurfaceKHR surface;
    State *state = (State *)ptr;
    if(SDL_Vulkan_CreateSurface(state->window, getVkInstance(), nullptr, &surface) == SDL_FALSE)
    {
        printf("Failed to create surface\n");
        return {};
    }
    if(surface == 0)
    {
        printf("Failed to create surface\n");
        return {};
    }
    return surface;
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
    if(!surface)
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
    const VkFormat colorFormats[] = { swapchainFormats.defaultColorFormat };

    VkPipelineColorBlendAttachmentState blendStates[] = {
        {
            .blendEnable = VK_FALSE,
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
        .pipelineLayout = state.pipelineLayout,
        .stageInfoCount = ARRAYSIZES(stageInfos),
        .colorFormatCount = ARRAYSIZES(colorFormats),
        .blendChannelCount = ARRAYSIZES(blendStates),
    };
    state.pipeline = createGraphicsPipeline(gpBuilder, "Triangle graphics pipeline");

    if(!state.pipeline)
    {
        printf("Failed to create graphics pipeline\n");
        return 8;
    }

    if(!createImage(SCREEN_WIDTH, SCREEN_HEIGHT, swapchainFormats.defaultColorFormat,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        "Render target", state.image))
    {
        printf("Failed to create image\n");
        return 8;
    }

    char tmpBuf[256] = {};
    int updateTick = 0;
    uint64_t lastTicks = SDL_GetTicksNS();

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
        }
        beginFrame();
        sDraw(state);
        presentImage(state.image);
        static int MaxTicks = 100;
        if(++updateTick >= MaxTicks)
        {
            uint64_t newTicks = SDL_GetTicksNS();
            double diff = newTicks - lastTicks;
            diff = diff > 0 ? diff / MaxTicks : 1;
            lastTicks = newTicks;

            SDL_snprintf(tmpBuf, 255, "FPS: %f - %fms", 1'000'000'000.0 / diff, diff / 1'000'000.0);

            SDL_SetWindowTitle(state.window, tmpBuf);
            updateTick = 0;
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
