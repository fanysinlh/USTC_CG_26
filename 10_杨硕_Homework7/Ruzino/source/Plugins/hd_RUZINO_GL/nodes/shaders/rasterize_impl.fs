#version 430

layout(location = 0) out vec3 position;
layout(location = 1) out float depth;
layout(location = 2) out vec2 texcoords;
layout(location = 3) out vec3 diffuseColor;
layout(location = 4) out vec2 metallicRoughness;
layout(location = 5) out vec3 normal;

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vTexcoord;
uniform mat4 projection;
uniform mat4 view;

uniform sampler2D diffuseColorSampler;

// This only works for current scenes provided by the TAs 
// because the scenes we provide is transformed from gltf
uniform sampler2D normalMapSampler;
uniform sampler2D metallicRoughnessSampler;

void main() {
    position = vertexPosition;
    vec4 clipPos = projection * view * (vec4(position, 1.0));
    depth = clipPos.z / clipPos.w;
    texcoords = vTexcoord;

    diffuseColor = texture2D(diffuseColorSampler, vTexcoord).xyz;
    metallicRoughness = texture2D(metallicRoughnessSampler, vTexcoord).zy;

    vec3 normalmap_value = texture2D(normalMapSampler, vTexcoord).xyz;
    vec3 geometric_normal = normalize(vertexNormal);
    normal = geometric_normal;

    vec3 dpdx = dFdx(vertexPosition);
    vec3 dpdy = dFdy(vertexPosition);
    vec2 duvdx = dFdx(vTexcoord);
    vec2 duvdy = dFdy(vTexcoord);

    float det = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    vec3 tangent;
    vec3 bitangent;
    if (abs(det) > 1E-8) {
        float invDet = 1.0 / det;
        tangent = (dpdx * duvdy.y - dpdy * duvdx.y) * invDet;
        bitangent = (dpdy * duvdx.x - dpdx * duvdy.x) * invDet;
    } else {
        tangent = dFdx(vertexPosition);
        tangent = tangent - dot(tangent, geometric_normal) * geometric_normal;
        if (length(tangent) < 1E-8) {
            tangent = cross(vec3(0.0, 1.0, 0.0), geometric_normal);
            if (length(tangent) < 1E-8) {
                tangent = cross(vec3(1.0, 0.0, 0.0), geometric_normal);
            }
        }
        bitangent = cross(geometric_normal, tangent);
    }

    tangent = normalize(tangent - dot(tangent, geometric_normal) * geometric_normal);
    bitangent = normalize(bitangent - dot(bitangent, geometric_normal) * geometric_normal);
    bitangent = normalize(bitangent - dot(bitangent, tangent) * tangent);

    vec3 tangent_space_normal = normalize(normalmap_value * 2.0 - 1.0);
    mat3 TBN = mat3(tangent, bitangent, geometric_normal);
    normal = normalize(TBN * tangent_space_normal);

    // Keep the mapped normal in the same hemisphere as the geometry normal.
    // This avoids surfaces such as the floor being flipped by an inconsistent TBN.
    if (dot(normal, geometric_normal) < 0.0) {
        normal = -normal;
    }
}
