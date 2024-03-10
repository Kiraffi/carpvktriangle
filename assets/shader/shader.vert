#version 450
#define MATRIX_ORDER row_major
//#define MATRIX_ORDER column_major

vec2 positions[3] = vec2[](
    vec2(0.5, -0.5),
    vec2(0.0, 0.5),
    vec2(-0.5, -0.5)
);


struct VData
{
    vec3 pos;
    uint color;
};

layout (set = 0, binding = 0, MATRIX_ORDER) restrict readonly buffer VertexDataBuffer
{
    VData vData[];
} VertData;

layout (location = 0) out vec4 colOut;


void main()
{
    vec3 pos = VertData.vData[gl_VertexIndex].pos;
    uint col = VertData.vData[gl_VertexIndex].color;
    gl_Position = vec4(pos, 1.0);

    colOut = vec4(
        (col & 255u),
        ((col >> 8u) & 255u),
        ((col >> 16u) & 255u),
        ((col >> 25u) & 255u));

    colOut /= 255.0;
}
