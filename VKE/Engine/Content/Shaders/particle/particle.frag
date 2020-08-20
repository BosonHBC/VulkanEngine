#version 450
layout(location = 0) out vec4 outColor;

layout (location = 0) in float elapsedTime;
layout (location = 1) in float lifeTime;
layout (location = 2) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform sampler2D TextureSampler;

const float FadeInPercent = 0.1;
const float FadeOutPercent = 0.1;

const vec3 AddColor = vec3(0.5,0.25,0.25);
void main(){

    // particle that are not supposed to render
    if(elapsedTime <= 0)
    {
        discard;
        return;
    }
    float alpha = 0.5;

    float lifePercentage = elapsedTime / lifeTime;

    vec4 textureColor = texture(TextureSampler, fragTexCoord);
   // outColor = vec4(textureColor.rgb * AddColor, textureColor.a * clamp(1.0 - lifePercentage, 0.0, 1.0));
    outColor = vec4(textureColor.rgb * AddColor, textureColor.a);
    if (outColor.w < 0.01) { discard; }
}