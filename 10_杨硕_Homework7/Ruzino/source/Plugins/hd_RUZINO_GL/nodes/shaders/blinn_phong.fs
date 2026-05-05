#version 430 core

// Define a uniform struct for lights
struct Light {
    // The matrices are used for shadow mapping. You need to fill it according to how we are filling it when building the normal maps (node_render_shadow_mapping.cpp). 
    // Now, they are filled with identity matrix. You need to modify C++ code innode_render_deferred_lighting.cpp.
    // Position and color are filled.
    mat4 light_projection;
    mat4 light_view;
    vec3 position;
    float radius;
    vec3 color; // Just use the same diffuse and specular color.
    int shadow_map_id;
};

layout(binding = 0) buffer lightsBuffer {
Light lights[4];
};

uniform vec2 iResolution;

uniform sampler2D diffuseColorSampler;
uniform sampler2D normalMapSampler; // You should apply normal mapping in rasterize_impl.fs
uniform sampler2D metallicRoughnessSampler;
uniform sampler2DArray shadow_maps;
uniform sampler2D position;

// uniform float alpha;
uniform vec3 camPos;

uniform int light_count;

layout(location = 0) out vec4 Color;

vec3 lightSpaceProject(int lightIndex, vec3 pos) {
    vec4 light_clip = lights[lightIndex].light_projection *
                      lights[lightIndex].light_view *
                      vec4(pos, 1.0);
    return light_clip.xyz / light_clip.w;
}

float shadowDepthSample(int lightIndex, vec2 uv) {
    return texture(shadow_maps, vec3(uv, lights[lightIndex].shadow_map_id)).x;
}

float pcfShadow(int lightIndex, vec2 uv, float currentDepth, float bias, float filterRadius) {
    float visibility = 0.0;
    const int kernelRadius = 2;
    vec2 texelSize = 1.0 / vec2(textureSize(shadow_maps, 0).xy);
    for (int x = -kernelRadius; x <= kernelRadius; ++x) {
        for (int y = -kernelRadius; y <= kernelRadius; ++y) {
            vec2 offset = vec2(x, y) * texelSize * max(filterRadius, 1.0);
            float stored = shadowDepthSample(lightIndex, uv + offset);
            visibility += currentDepth - bias > stored ? 0.0 : 1.0;
        }
    }
    return visibility / float((kernelRadius * 2 + 1) * (kernelRadius * 2 + 1));
}

float computeShadow(int lightIndex, vec3 pos, vec3 normal, vec3 lightDir) {
    vec3 ndc = lightSpaceProject(lightIndex, pos);
    vec2 shadow_uv = ndc.xy * 0.5 + 0.5;
    if (shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
        shadow_uv.y < 0.0 || shadow_uv.y > 1.0 ||
        ndc.z < -1.0 || ndc.z > 1.0) {
        return 1.0;
    }

    float current_depth = ndc.z;
    float bias = max(0.0025 * (1.0 - max(dot(normal, lightDir), 0.0)), 0.0005);

    float avgBlocker = 0.0;
    float blockerCount = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadow_maps, 0).xy);
    float searchRadius = 12.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize * searchRadius;
            float stored = shadowDepthSample(lightIndex, shadow_uv + offset);
            if (stored < current_depth - bias) {
                avgBlocker += stored;
                blockerCount += 1.0;
            }
        }
    }

    if (blockerCount < 0.5) {
        return 1.0;
    }

    avgBlocker /= blockerCount;
    float penumbra = clamp((current_depth - avgBlocker) * 80.0, 1.0, 24.0);
    return pcfShadow(lightIndex, shadow_uv, current_depth, bias, penumbra);
}

void main() {
vec2 uv = gl_FragCoord.xy / iResolution;

vec3 pos = texture2D(position,uv).xyz;
vec3 normal = normalize(texture2D(normalMapSampler,uv).xyz);

if (length(normal) < 1E-6) {
    Color = vec4(0, 0, 0, 1);
    return;
}

vec3 albedo = texture2D(diffuseColorSampler, uv).xyz;

vec4 metalnessRoughness = texture2D(metallicRoughnessSampler,uv);
float metal = metalnessRoughness.x;
float roughness = clamp(metalnessRoughness.y, 0.04, 1.0);

vec3 viewDir = normalize(camPos - pos);
vec3 ambient = 0.03 * albedo;
vec3 result = ambient;

for(int i = 0; i < light_count; i ++) {
    vec3 lightVec = lights[i].position - pos;
    float distanceToLight = length(lightVec);
    vec3 lightDir = normalize(lightVec);
    float attenuation =
        1.0 / (1.0 + 0.35 * distanceToLight + 0.08 * distanceToLight * distanceToLight);

    float ndotl = max(dot(normal, lightDir), 0.0);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float shininess = mix(128.0, 8.0, roughness);
    float specularTerm = pow(max(dot(normal, halfwayDir), 0.0), shininess);

    vec3 diffuse = albedo * ndotl;
    vec3 specularColor = mix(vec3(0.04), albedo, metal);
    vec3 specular = specularColor * specularTerm;

    float shadow = computeShadow(i, pos, normal, lightDir);
    result += (diffuse + specular) * lights[i].color * attenuation * shadow;
}
Color = vec4(result, 1.0);
}
