#version 450
// Vertex attributes
layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexColor;
layout (location = 2) in vec2 texCoord;
// Instanced attributes
layout (location = 3) in vec4 pos;
layout (location = 4) in vec4 vel;

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

void main()
{
    float scale = 0.2;
    // pass particle time to FS
    elapsedTime = pos.w;
    lifeTime = vel.w;
    fragTexCoord = texCoord;

    // Model matrix for individual quad
    mat4 Model = mat4(1.0);
    Model[3] = vec4(pos.xyz, 1.0);
    mat4 ModelView = ViewMatrix * ModelMatrix * Model;
   /*
    // Remove rotation to have a billboard effect
    // Column 0:
    ModelView[0][0] = 1;
    ModelView[0][1] = 0;
    ModelView[0][2] = 0;
    // Column 1:
    ModelView[1][0] = 0;
    ModelView[1][1] = 1;
    ModelView[1][2] = 0;
    // Column 2:
    ModelView[2][0] = 0;
    ModelView[2][1] = 0;
    ModelView[2][2] = 1;
*/
    gl_Position = ProjectionMatrix * ModelView * vec4(scale * vertexPos, 1.0);
}