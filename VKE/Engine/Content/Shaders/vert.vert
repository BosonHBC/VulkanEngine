#version 450

layout(location = 0) out vec3 fragColor;

const vec3 Position[3] = vec3[] 
(
    vec3(0.0, -0.4, 0.0),
    vec3(0.4, 0.4, 0.0),
    vec3(-0.4, 0.4, 0.0)
);

const vec3 Color[3] = vec3[]
(
    vec3(0.5, 0.0, 0.0),
    vec3(0.0, 0.5, 0.0),
    vec3(0.0, 0.0, 0.5)
);


void main()
{
    gl_Position = vec4(Position[gl_VertexIndex], 1.0);
    fragColor = Color[gl_VertexIndex];
}