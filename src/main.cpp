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

    VkPipelineLayout graphicsPipelineLayout = {};
    VkPipeline graphicsPipeline = {};

    VkPipelineLayout computePipelineLayout = {};
    VkPipeline computePipeline = {};

    VkShaderModule shaderModules[3] = {};

    VkDescriptorSet descriptorSet = {};
    Buffer modelVerticesBuffer = {};
    UniformBuffer uniformBuffer = {};
    Image image = {};

    uint64_t ticksAtStart = {};
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

void sDeinitShaders(void* userData)
{
    State *state = (State *) userData;

    VkDevice device = getVkDevice();

    destroyImage(state->image);

    destroyShaderModule(state->shaderModules, ARRAYSIZES(state->shaderModules));
    destroyBuffer(state->modelVerticesBuffer);
    vkDestroyDescriptorSetLayout(device, state->descriptorSetLayout, nullptr);
    vkDestroyPipeline(device, state->graphicsPipeline, nullptr);
    vkDestroyPipeline(device, state->computePipeline, nullptr);
    vkDestroyPipelineLayout(device, state->graphicsPipelineLayout, nullptr);
    state->graphicsPipelineLayout = {};

    vkDestroyPipelineLayout(device, state->computePipelineLayout, nullptr);
    state->computePipelineLayout = {};

}
void sGetWindowSize(int32_t* widthOut, int32_t* heightOut, void* userData)
{
    State* state = (State*) userData;
    SDL_GetWindowSize(state->window, widthOut, heightOut);
}


bool sCreateImage(State& state)
{
    int width = 0;
    int height = 0;
    sGetWindowSize(&width, &height, &state);
    if(width == state.image.width && height == state.image.height)
    {
        return true;
    }

    destroyImage(state.image);


    const CarpSwapChainFormats& swapchainFormats = getSwapChainFormats();
    if(!createImage(width, height, swapchainFormats.defaultColorFormat,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        "Render target", state.image))
    {
        printf("Failed to recreate render target image\n");
        return false;
    }
    return true;
}

void sResized(void* userData)
{
    State* state = (State*) userData;
    sCreateImage(*state);
}

static VkSurfaceKHR sCreateSurface(VkInstance instance, void* userData)
{
    State* state = (State*)userData;
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



bool sDraw(State& state)
{
    int width = state.image.width;
    int height = state.image.height;

    VkCommandBuffer commandBuffer = getVkCommandBuffer();

    // Compute
    {
        struct UpdateStruct
        {
            uint32_t totalTimeMs;
            uint32_t padding[63];
        };

        UpdateStruct data = {
            .totalTimeMs = uint32_t((SDL_GetTicksNS() - state.ticksAtStart) / 1000),
        };
        BufferCopyRegion region = uploadToScratchbuffer(
            &data, state.uniformBuffer.offset, sizeof(UpdateStruct));
        uploadScratchBufferToGpuUniformBuffer(state.uniformBuffer, region,
            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT);

        VkBufferMemoryBarrier2 barriers[] = {
            bufferBarrier(state.modelVerticesBuffer,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
            bufferBarrier(state.uniformBuffer,
                VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
        };


        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.bufferMemoryBarrierCount = ARRAYSIZES(barriers);
        dependencyInfo.pBufferMemoryBarriers = barriers;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);


        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.computePipelineLayout,
    0, 1, &state.descriptorSet,
    0, NULL);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.computePipeline);
        vkCmdDispatch(commandBuffer, 1, 1, 1);
    }



    {
        VkImageMemoryBarrier2 imgBarrier[] = {
            imageBarrier(state.image, 
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        };
        VkBufferMemoryBarrier2 bufBar = bufferBarrier(state.modelVerticesBuffer,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);

        VkDependencyInfo dep2 = {};
        dep2.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep2.imageMemoryBarrierCount = ARRAYSIZES(imgBarrier);
        dep2.pImageMemoryBarriers = imgBarrier;
        dep2.bufferMemoryBarrierCount = 1;
        dep2.pBufferMemoryBarriers = &bufBar;

        vkCmdPipelineBarrier2(commandBuffer, &dep2);


    }

    // New structures are used to define the attachments used in dynamic rendering
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = state.image.view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { 0.0f,0.0f,0.0f,0.0f };

    /*
    // A single depth stencil attachment info can be used, but they can also be specified separately.
    // When both are specified separately, the only requirement is that the image view is identical.
    VkRenderingAttachmentInfo depthStencilAttachment{};
    depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthStencilAttachment.imageView = depthStencil.view;
    depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthStencilAttachment.clearValue.depthStencil = { 1.0f,  0 };
    */

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { 0, 0, u32(width), u32(height) };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    //renderingInfo.pDepthAttachment = &depthStencilAttachment;
    //renderingInfo.pStencilAttachment = &depthStencilAttachment;

    // Begin dynamic rendering
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.graphicsPipelineLayout,
        0, 1, &state.descriptorSet,
        0, NULL);


    VkViewport viewport = { 0.0f, float(height), float(width), -float(height), 0.0f, 1.0f };
    VkRect2D scissors = { { 0, 0 }, { u32(width), u32(height) } };

    vkCmdSetScissor(commandBuffer, 0, 1, &scissors);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.graphicsPipeline);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);

    // End dynamic rendering
    vkCmdEndRendering(commandBuffer);
    return true;
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

















/////////
///
/// SRun
///
/////////
int sRun(State &state)
{
    std::vector<char> fragShaderCode;
    std::vector<char> vertShaderCode;
    std::vector<char> compShaderCode;

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

    if(!sReadFile("assets/shader/compshader.spv", compShaderCode))
    {
        printf("Failed to read compute shader\n");
        return false;
    }
    // Create window
    SDL_Init(SDL_INIT_VIDEO);              // Initialize SDL3

    // Create an application window with the following settings:
    state.window = SDL_CreateWindow(
        "An SDL3 window",
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
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
    int width = SCREEN_WIDTH;
    int height = SCREEN_HEIGHT;
    sGetWindowSize(&width, &height, &state);

    VulkanInstanceParams vkInstanceParams = {
        .getExtraExtensionsFn = SDL_Vulkan_GetInstanceExtensions,
        .createSurfaceFn = sCreateSurface,
        .destroyBuffersFn = sDeinitShaders,
        .getWindowSizeFn = sGetWindowSize,
        .resizedFn = sResized,
        .userData = &state,
#if !NDEBUG
        .extensions = extensions,
        .extensionCount = ARRAYSIZES(extensions),
#endif
        .width = width,
        .height = height,
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

    if(!createBuffer(1024u * 1024u * 16u,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Model Vercies data buffer",
        state.modelVerticesBuffer))
    {
        printf("Failed to create model vertice data buffer\n");
        return 2;
    }


    static constexpr DescriptorSetLayout graphicsDescriptorLayouts[] =
    {
        {
            .bindingIndex = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT
        },
        {
            .bindingIndex = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        },

    };

    state.descriptorSetLayout =
        createSetLayout(graphicsDescriptorLayouts, ARRAYSIZES(graphicsDescriptorLayouts));

    if(!createDescriptorSet(state.descriptorSetLayout, &state.descriptorSet))
    {
        printf("Failed to create descriptorset\n");
        return 2;
    }


    state.uniformBuffer = createUniformBuffer(256);


    state.graphicsPipelineLayout = createPipelineLayout(state.descriptorSetLayout);
    state.computePipelineLayout = createPipelineLayout(state.descriptorSetLayout);

    static const DescriptorInfo descritorSetInfos[] = {
        DescriptorInfo(state.modelVerticesBuffer, 0, state.modelVerticesBuffer.size),
        DescriptorInfo(state.uniformBuffer),
    };

    if(!updateBindDescriptorSet(state.descriptorSet,
        graphicsDescriptorLayouts, descritorSetInfos, ARRAYSIZES(descritorSetInfos)))
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
        printf("Failed to create frag shader!\n");
        return 9;
    }

    if(!createShader(compShaderCode.data(), compShaderCode.size(), state.shaderModules[2]))
    {
        printf("Failed to create comp shader!\n");
        return 9;
    }

    const VkFormat colorFormats[] = { getSwapChainFormats().defaultColorFormat };

    const VkPipelineShaderStageCreateInfo stageInfos[] = {
        createDefaultVertexInfo(state.shaderModules[0]),
        createDefaultFragmentInfo(state.shaderModules[1]),
    };

    GPBuilder gpBuilder = {
        .stageInfos = stageInfos,
        .colorFormats = colorFormats,
        .blendChannels = &cDefaultBlendState,
        .pipelineLayout = state.graphicsPipelineLayout,
        .stageInfoCount = ARRAYSIZES(stageInfos),
        .colorFormatCount = ARRAYSIZES(colorFormats),
        .blendChannelCount = 1,
    };
    state.graphicsPipeline = createGraphicsPipeline(gpBuilder, "Triangle graphics pipeline");

    if(!state.graphicsPipeline)
    {
        printf("Failed to create graphics pipeline\n");
        return 8;
    }

    CPBuilder cpBuilder = {
        .stageInfo = createDefaultComputeInfo(state.shaderModules[2]),
        .pipelineLayout = state.computePipelineLayout,
    };

    state.computePipeline = createComputePipeline(cpBuilder, "Compute pipeline");
    if(!state.computePipeline)
    {
        printf("Failed to create compute pipeline\n");
        return 2;
    }

    if(!sCreateImage(state))
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

            {0.0, 0.5f, 0.5f, sGetColor(0.0f, 1.0f, 0.0f, 1.0f)},
            {0.5, -0.5f, 0.5f, sGetColor(0.0f, 0.0f, 1.0f, 1.0f)},
            {0.5, 0.5f, 0.5f, sGetColor(1.0f, 0.0f, 0.0f, 1.0f)},

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
    state.ticksAtStart = lastTicks;
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
