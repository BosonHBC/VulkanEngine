#version 450
layout(location = 0) out vec4 outColor;

layout (location = 0) in float elapsedTime;
layout (location = 1) in float lifeTime;
void main(){

    // particle that are not supposed to render
    if(elapsedTime <= 0)
    {
        discard;
        return;
    }
    float alpha = 0.5;

    float lifePercentage = elapsedTime / lifeTime;

    outColor = vec4(1, 0, 0, clamp(1.0 - lifePercentage, 0.0, 1.0));
}