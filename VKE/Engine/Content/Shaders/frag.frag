#version 450
// Depends on attachment address
layout(location = 0) out vec4 outColor;

layout (location = 0) in vec3 fragCol;
layout (location = 1) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform sampler2D TextureSampler;
//layout(set = 1, binding = 1) uniform sampler2D NormalSampler;

void main()
{
    vec3 albedo = texture(TextureSampler, fragTexCoord).rgb;
    //vec3 normal = texture(NormalSampler, fragTexCoord).rgb;

    outColor = vec4(albedo, 1.0);
}