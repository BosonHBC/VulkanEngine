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

// square
vec2 positions[6] = vec2[](
    vec2(-0.1, 0.1),
    vec2(-0.1, -0.1),
    vec2(0.1, -0.1),
    vec2(0.1, -0.1),
    vec2(0.1, 0.1),
    vec2(-0.1, 0.1)
);

void main()
{
    mat4 Model = mat4(1.0);
    Model[3] = pos;
    mat4 ModelView = ViewMatrix * Model;
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
    gl_Position = ProjectionMatrix * ModelView * vec4(positions[gl_VertexIndex], 0.0, 1.0);
}