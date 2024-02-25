#include <vulkan/vulkan_core.h>
//#include <volk.h>
#include <stdio.h>

#include <carpvk.h>

#include <SDL3/SDL.h>
#include "SDL_vulkan.h"
#include "../external/carpvk/src/carpvk.h"

#include <vector>
#include <fstream>


static const int SCREEN_WIDTH = 1024;
static const int SCREEN_HEIGHT = 768;

struct State
{
    SDL_Window *window = nullptr;
    CarpVk carpVk;
    VulkanInstanceBuilder builder;


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
    for (int i = 0; i < SDL_arraysize(state->shaders); ++i)
    {
        if(state->shaders[i] == nullptr)
        {
            continue;
        }
        vkDestroyShaderEXT(carpVk.device, state->shaders[i], nullptr);
        state->shaders[i] = nullptr;
    }
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



    // The window is open: could enter program loop here (see SDL_PollEvent())

    SDL_Delay(1000);  // Pause execution for 3000 milliseconds, for example
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
