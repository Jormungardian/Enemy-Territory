#version 460

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inVertColor;
layout(location = 2) in vec2 uvs;

layout ( location = 0) out vec4 outVertColor;
layout ( location = 1) out vec2 outUV;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(push_constant) uniform PushConsts {
  mat4 p;
  mat4 m;
} pushConsts;

void main() {
    mat4 glCorrection = mat4(1.0, 0.0, 0.0, 0.0, 
                             0.0,-1.0, 0.0, 0.0,
                             0.0, 0.0, 0.5, 0.0,
                             0.0, 0.0, 0.5, 1.0);

    outUV = uvs;
    outVertColor = inVertColor;
    gl_Position = glCorrection*pushConsts.p*pushConsts.m*vec4(inPosition.xyz, 1.0);
    gl_Position.z = 0.5;
}