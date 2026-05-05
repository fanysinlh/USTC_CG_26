#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(std430, binding = 0) buffer buffer0 {
vec2 data[];
}
aTexcoord;

out vec3 vertexPosition;
out vec3 vertexNormal;
out vec2 vTexcoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform sampler2D displacementSampler;

void main() {
vec2 texcoord = aTexcoord.data[gl_VertexID];
texcoord.y = 1.0 - texcoord.y;

float height = texture(displacementSampler, texcoord).r;
float displacementScale = 0.08;
vec3 displacedLocalPos = aPos + aNormal * ((height - 0.5) * displacementScale);

gl_Position = projection * view * model * vec4(displacedLocalPos, 1.0);
vec4 vPosition = model * vec4(displacedLocalPos, 1.0);
vertexPosition = vPosition.xyz / vPosition.w;
vertexNormal = (inverse(transpose(mat3(model))) * aNormal);
vTexcoord = texcoord;
}
