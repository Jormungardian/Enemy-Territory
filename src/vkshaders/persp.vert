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
    mat4 perps = mat4(0.99999999, 0.0, 0.0, 0.0,
		      0.0, 1.334261, 0.0, 0.0,
		      0.0, 0.0, -1.00117838, -1.0,
		      0.0, 0.0, -6.00353527, 0.0);

   mat4 mod = mat4(0.070887111, 0.0305976, -0.997014, 0.0,
		   -0.997484, 0.0021744, -0.070853, 0.0,
		  0.0, 0.99952, 0.0306748, 0.0,
		  2671.24568, 340.106140, 1299.35352, 1.0);

    mat4 glCorrection = mat4(1.0, 0.0, 0.0, 0.0, 
                             0.0,-1.0, 0.0, 0.0,
                             0.0, 0.0, 0.5, 0.0,
                             0.0, 0.0, 0.5, 1.0);

    outUV = uvs;
    outVertColor = inVertColor;
    gl_Position = glCorrection*perps*mod*vec4(inPosition.xyz, 1.0);
    gl_Position.z = 1.0;
}