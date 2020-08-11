#version 450
layout (location = 0) in vec4 pos;

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

void main()
{
    gl_PointSize = 1;
    gl_Position = PVMatrix * pos;
}