#version 450

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 col;
layout (location = 2) in vec2 texCoord;

// Uniforms buffer
layout(set = 0, binding = 0) uniform sFrameData
{
    mat4 PVMatrix;
	mat4 ProjectionMatrix;
	mat4 InvProj;
	mat4 ViewMatrix;
	mat4 InvView;
};
// Dynamic binding
layout(set = 0, binding = 1) uniform sDrawcallData
{
    mat4 ModelMatrix;
};

layout(push_constant) uniform sPushModel
{
    mat4 MVP;
};

layout (location = 0) out vec3 fragCol;
layout (location = 1) out vec2 fragTexCoord;

void main()
{
    gl_Position = PVMatrix * ModelMatrix * vec4(pos, 1.0);
    fragCol = col;
    fragTexCoord = texCoord;
}