//#include <vulkan/vulkan_core.h>
#include <volk.h>
#include <stdio.h>

#include "carpvk.h"

int main()
{
    const char* extensions[] = {
        //VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
#if NDEBUG
#else
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
        //VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        //"VK_LAYER_KHRONOS_shader_object",
        VK_KHR_SURFACE_EXTENSION_NAME,
        //"VK_KHR_xlib_surface",
        "VK_KHR_xcb_surface",
    };


    if(!initVolk())
    {
        printf("Failed to init volk\n");
        return 2;
    }
    //printExtensions();
    //printLayers();

    CarpVk carpVk;

    VulkanInstanceBuilder builder = instaceBuilder();
    //instanceBuilderSetApplicationVersion(builder, 0, 1, 0);
    instanceBuilderSetExtensions(builder, extensions, sizeof(extensions) / sizeof(const char*));
    //instanceBuilderUseDefaultValidationLayers(builder);
    if(!instanceBuilderFinish(builder, carpVk))
    {
        printf("Failed to initialize vulkan.\n");
        return 1;
    }
    printf("Success on creating instance\n");

    return 0;
}
