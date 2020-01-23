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
	float r;
  float l;
  float t;
  float b;
  float fa;
  float n;
} pushConsts;

void main() {          
    mat4 ortho = mat4(2.0/(pushConsts.r - pushConsts.l), 0.0, 0.0, 0.0, 
                      0.0, 2.0/(pushConsts.t - pushConsts.b), 0.0, 0.0, 
                      0.0, 0.0, 2.0/(pushConsts.fa - pushConsts.n), 0.0, 
                      -(pushConsts.r + pushConsts.l)/(pushConsts.r - pushConsts.l), -(pushConsts.t + pushConsts.b)/(pushConsts.t - pushConsts.b), -(pushConsts.fa + pushConsts.n)/(pushConsts.fa - pushConsts.n), 1.0);
                      
    vec4 pos = inPosition;
    pos.w = 1.0;
    
    outUV = uvs;
    outVertColor = inVertColor;
    gl_Position = ortho*vec4(inPosition.xyz, 1.0);
    gl_Position.z = 0.5;
}