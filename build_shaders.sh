glslc assets/shader/shader.vert --target-env=vulkan1.3 -o assets/shader/vert.spv
glslc assets/shader/shader.frag --target-env=vulkan1.3 -o assets/shader/frag.spv
glslc assets/shader/compshader.comp --target-env=vulkan1.3 -o assets/shader/compshader.spv
pause