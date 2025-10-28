#version 440

#define M_PI 3.1415926535897932384626433832795
#define M_PI_2 (2.0 * M_PI)

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float line_width;
    float sz;
    float shdr_radius;
    float fangle;
    vec4 line_colour;
};

void main() {

    highp float xx = qt_TexCoord0.x*M_PI - fangle*M_PI/180.0;
    fragColor = line_colour*cos(xx);

}