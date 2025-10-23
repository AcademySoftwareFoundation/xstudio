#version 440

#define M_PI 3.1415926535897932384626433832795
#define M_PI_2 (2.0 * M_PI)

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float vvv;
    float rim;
};

vec3 hsv_to_rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {

    highp vec2 coord = qt_TexCoord0 - vec2(0.5);
    highp float r = length(coord)*2.02f;
    highp float h = atan(coord.x, coord.y);

    vec3 rgb = hsv_to_rgb( vec3((h + M_PI) / M_PI_2, r/1.01f, vvv) );
    fragColor = vec4(rgb, 1.0)*smoothstep(1.01,1.0,r)*(0.5+0.5*smoothstep(1.0-rim,1.01-rim,r));
}