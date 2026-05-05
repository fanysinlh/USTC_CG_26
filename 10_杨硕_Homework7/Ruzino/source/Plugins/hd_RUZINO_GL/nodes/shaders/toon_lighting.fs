#version 430 core

struct Light {
    mat4 light_projection;
    mat4 light_view;
    vec3 position;
    float radius;
    vec3 color;
    int shadow_map_id;
};

layout(binding = 0) buffer lightsBuffer {
Light lights[4];
};

uniform vec2 iResolution;
uniform sampler2D diffuseColorSampler;
uniform sampler2D normalMapSampler;
uniform sampler2D metallicRoughnessSampler;
uniform sampler2DArray shadow_maps;
uniform sampler2D position;
uniform vec3 camPos;
uniform int light_count;

layout(location = 0) out vec4 Color;

float computeShadow(int lightIndex, vec3 pos, vec3 normal, vec3 lightDir) {
    vec4 light_clip = lights[lightIndex].light_projection *
                      lights[lightIndex].light_view *
                      vec4(pos, 1.0);
    vec3 ndc = light_clip.xyz / light_clip.w;
    vec2 shadow_uv = ndc.xy * 0.5 + 0.5;
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
        ndc.z < -1.0 || ndc.z > 1.0) {
        return 1.0;
    }
    float stored = texture(shadow_maps, vec3(shadow_uv, lights[lightIndex].shadow_map_id)).x;
    float bias = max(0.002 * (1.0 - max(dot(normal, lightDir), 0.0)), 0.0005);
    return ndc.z - bias > stored ? 0.0 : 1.0;
}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    vec3 pos = texture(position, uv).xyz;
    vec3 normal = normalize(texture(normalMapSampler, uv).xyz);
    vec3 albedo = texture(diffuseColorSampler, uv).xyz;
    vec3 viewDir = normalize(camPos - pos);

    vec3 result = 0.08 * albedo;
    for (int i = 0; i < light_count; ++i) {
        vec3 lightVec = lights[i].position - pos;
        float dist = length(lightVec);
        vec3 lightDir = normalize(lightVec);
        float ndotl = max(dot(normal, lightDir), 0.0);
        float bands = floor(ndotl * 4.0) / 3.0;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), 48.0);
        spec = step(0.6, spec);
        float shadow = computeShadow(i, pos, normal, lightDir);
        float attenuation = 1.0 / max(dist * dist, 1e-4);
        result += (albedo * bands + vec3(spec) * 0.35) * lights[i].color *
                  attenuation * shadow;
    }

    Color = vec4(result, 1.0);
}
