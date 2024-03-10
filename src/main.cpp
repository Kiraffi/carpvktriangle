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

    VkDescriptorSetLayout descriptorSetLayout = {};

    VkPipelineLayout pipelineLayout = {};
    VkPipeline pipeline = {};

    VkShaderModule shaderModules[2] = {};

    VkDescriptorSet descriptorSets[16] = {};
    Buffer modelVerticesBuffer = {};
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

void sDeinitShaders(void* data)
{
    State *state = (State *) data;

    VkDevice device = getVkDevice();

    destroyImage(state->image);

    destroyShaderModule(state->shaderModules, 2);
    destroyBuffer(state->modelVerticesBuffer);
    vkDestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
    vkDestroyPipeline(device, state->pipeline, nullptr);
    vkDestroyPipelineLayout(device, state->pipelineLayout, nullptr);
    state->pipelineLayout = {};
}






bool sDraw(State& state)
{
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

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.pipelineLayout,
        0, 1, state.descriptorSets,
        0, NULL);


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

static VkSurfaceKHR sCreateSurface(VkInstance instance, void* ptr)
{
    State* state = (State*)ptr;
    VkSurfaceKHR surface;
    if(SDL_Vulkan_CreateSurface(state->window, getVkInstance(), nullptr, &surface) == SDL_FALSE)
    {
        printf("Failed to create surface\n");
        return {};
    }
    if(!surface)
    {
        printf("Failed to create surface\n");
        return {};
    }
    return surface;
}

float clamp(float v, float minValue, float maxValue)
{
    v = v < minValue ? minValue : v;
    v = v > maxValue ? maxValue : v;
    return v;
}

uint32_t sGetColor(float r, float g, float b, float a)
{
    r = clamp(r, 0.0f, 1.0f);
    g = clamp(g, 0.0f, 1.0f);
    b = clamp(b, 0.0f, 1.0f);
    a = clamp(a, 0.0f, 1.0f);
    uint32_t color = uint32_t(r * 255.0f);
    color |= (uint32_t(g * 255.0f)) << 8u;
    color |= (uint32_t(b * 255.0f)) << 16u;
    color |= (uint32_t(a * 255.0f)) << 24u;
    return color;
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
    static const char *extensions[] =
    {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    VulkanInstanceParams vkInstanceParams = {
        .getExtraExtensionsFn = SDL_Vulkan_GetInstanceExtensions,
        .createSurfaceFn = sCreateSurface,
        .destroyBuffersFn = sDeinitShaders,
        .pData = &state,
#if !NDEBUG
        .extensions = extensions,
        .extensionCount = ARRAYSIZES(extensions),
#endif
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
        .vsyncMode = VSyncType::IMMEDIATE_NO_VSYNC,
#if !NDEBUG
        .useValidation = true,
#endif
        .useIntegratedGpu = false, //true,
    };

    if(!initVulkan(vkInstanceParams))
    {
        printf("Failed to initialize vulkan\n");
        return 1;
    }

    VkDevice device = getVkDevice();
    if(!createBuffer(1024u * 1024u * 16u,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Model Vercies data buffer",
        state.modelVerticesBuffer))
    {
        printf("Failed to create model vertice data buffer\n");
        return 2;
    }

    static constexpr DescriptorSetLayout descriptorLayouts[] =
    {
        {
            .bindingIndex = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    state.descriptorSetLayout = createSetLayout(descriptorLayouts, ARRAYSIZES(descriptorLayouts));
    if(!createDescriptorSet(state.descriptorSetLayout,
        descriptorLayouts, ARRAYSIZES(descriptorLayouts),
        state.descriptorSets))
    {
        printf("Failed to create descriptorset\n");
        return 2;
    }




    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &state.descriptorSetLayout;

    VK_CHECK_CALL(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &state.pipelineLayout));

    static const DescriptorInfo descritorSetInfos[] = {
        DescriptorInfo(state.modelVerticesBuffer, 0, state.modelVerticesBuffer.size),
    };

    if(!updateBindDescriptorSet(state.descriptorSets,
        descriptorLayouts, descritorSetInfos, ARRAYSIZES(descritorSetInfos)))
    {
        printf("Failed to update descriptorset\n");
        return 1;
    }


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

    {
        struct VData
        {
            float posX;
            float posY;
            float posZ;
            uint32_t color;
        };

        VData vData[] =
        {
            {0.0, 0.5f, 0.5f, sGetColor(0.0f, 1.0f, 0.0f, 1.0f)},
            {-0.5, -0.5f, 0.5f, sGetColor(1.0f, 0.0f, 0.0f, 1.0f)},
            {0.5, -0.5f, 0.5f, sGetColor(0.0f, 0.0f, 1.0f, 1.0f)},

        };
        beginPreFrame();
        BufferCopyRegion region = uploadToScratchbuffer(vData, 0, sizeof(vData));
        uploadScratchBufferToGpuBuffer(state.modelVerticesBuffer, region,
            VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT);
        endPreFrame();
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

    deinitVulkan();

    // Close and destroy the window
    SDL_DestroyWindow(state.window);

    // Clean up
    SDL_Quit();

    return returnValue;
}
