#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float p0;
    float p1;
};
layout(binding = 1) uniform sampler2D maskSource;
void main() {
    fragColor = texture(maskSource, qt_TexCoord0) * smoothstep(qt_TexCoord0.x, p0, p1) * qt_Opacity;
}

