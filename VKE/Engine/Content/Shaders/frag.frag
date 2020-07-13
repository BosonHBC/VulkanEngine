#version 450
// Depends on attachment address
layout(location = 0) out vec4 outColor;

layout (location = 0) in vec3 fragCol;
layout (location = 1) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform sampler2D TextureSampler;

void main()
{
    outColor = vec4(texture(TextureSampler, fragTexCoord).rgb, 1.0);
}