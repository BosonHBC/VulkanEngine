#version 450
// Vertex attributes
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexColor;
layout (location = 2) in vec2 texCoord;
// Instanced attributes
layout (location = 3) in vec4 pos;
layout (location = 4) in vec4 vel;
layout (location = 5) in vec4 particleColor;
// volume.x -> size, volume.y -> rotation along Z axis, volume.z -> texture index, volume.z -> tiling
layout (location = 6) in vec4 volume;       

// Uniforms buffer
layout(set = 0, binding = 0) uniform sFrameData
{
    mat4 PVMatrix;
	mat4 ProjectionMatrix;
	mat4 InvProj;
	mat4 ViewMatrix;
	mat4 InvView;
};
// Model matrix for all particles
layout(set = 0, binding = 1) uniform sDrawcallData
{
    mat4 ModelMatrix;
};

// VS to FS
layout (location = 0) out float elapsedTime;
layout (location = 1) out float lifeTime;
layout (location = 2) out vec2 fragTexCoord;
layout (location = 3) out vec4 fragParticleColor;

void main()
{
    // pass particle time to FS
    elapsedTime = pos.w;
    lifeTime = vel.w;
    if(elapsedTime < 0)
    {
        // this particle would be discared
        return;
    }
    int tileID = int(volume.z);      // Which sub texture to use, from left to right, top to bottom
    int tileWidth = int(volume.w);   // how many sub-divisions per texture, tileWidth * tileWidth = totalTiles, assuming tileX == tileY
    
    // shrink then offset(from top left to bottom right)
    float subWidth = (1.0 / float(tileWidth));
    vec2 TexCoord = texCoord / tileWidth + vec2(subWidth * (tileID % tileWidth), subWidth * (tileID / tileWidth));
    
    float scale = volume.x;
    float rotationAlongZ = volume.y;

    fragTexCoord = TexCoord;
    fragParticleColor = particleColor;
    // Model matrix for individual quad
    mat4 Model = mat4(1.0);
    Model[3] = vec4(pos.xyz, 1.0);
    mat4 ModelView = ViewMatrix * Model;
   
    // Bill board effect
    // Remove rotation on X, Y axis, but keep the Z axis rotation if there is one
    // Column 0:
    ModelView[0][0] = cos(rotationAlongZ);
    ModelView[0][1] = sin(rotationAlongZ);
    ModelView[0][2] = 0;
    // Column 1:
    ModelView[1][0] = -sin(rotationAlongZ);
    ModelView[1][1] = cos(rotationAlongZ);
    ModelView[1][2] = 0;
    // Column 2:
    ModelView[2][0] = 0;
    ModelView[2][1] = 0;
    ModelView[2][2] = 1.0;

    gl_Position = ProjectionMatrix * ModelView * vec4(scale * vertexPos, 1.0);
}