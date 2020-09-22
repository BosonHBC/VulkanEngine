#version 450
layout(location = 0) out vec4 outColor;

layout (location = 0) in float elapsedTime;
layout (location = 1) in float lifeTime;
layout (location = 2) in vec2 fragTexCoord;
layout (location = 3) in vec4 fragParticleColor;

layout(set = 1, binding = 0) uniform sampler2D TextureSampler;

const float FadeInPercent = 0.1;
const float FadeOutPercent = 0.1;

void main(){

    // particle that are not supposed to render
    if(elapsedTime <= 0)
    {
        discard;
        return;
    }

    float lifePercentage = elapsedTime / lifeTime;

    vec4 textureColor = texture(TextureSampler, fragTexCoord);
    outColor = textureColor * fragParticleColor;
}