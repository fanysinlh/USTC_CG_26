#version 430 core

uniform vec2 iResolution;
uniform sampler2D colorTexture;
uniform sampler2D positionTexture;
uniform sampler2D depthTexture;
uniform sampler2D normalTexture;

layout(location = 0) out vec4 Color;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

vec2 rotate(vec2 v, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution;
    vec3 baseColor = texture(colorTexture, uv).rgb;
    vec3 centerPos = texture(positionTexture, uv).xyz;
    vec3 normal = normalize(texture(normalTexture, uv).xyz);

    if (length(normal) < 1e-6) {
        Color = vec4(baseColor, 1.0);
        return;
    }

    float centerDepth = texture(depthTexture, uv).x;
    float radius = 12.0;
    float bias = 0.02;
    float occlusion = 0.0;
    const int sampleCount = 12;
    float angleJitter = hash(gl_FragCoord.xy) * 6.2831853;

    for (int i = 0; i < sampleCount; ++i) {
        float t = (float(i) + 0.5) / float(sampleCount);
        float angle = t * 6.2831853 + angleJitter;
        vec2 dir = vec2(cos(angle), sin(angle));
        vec2 offset = rotate(dir, angleJitter) * radius * t / iResolution;
        vec2 sampleUv = clamp(uv + offset, vec2(0.0), vec2(1.0));

        vec3 samplePos = texture(positionTexture, sampleUv).xyz;
        vec3 sampleVec = samplePos - centerPos;
        float distance2 = dot(sampleVec, sampleVec);
        if (distance2 < 1e-6) {
            continue;
        }

        vec3 sampleDir = normalize(sampleVec);
        float angular = max(dot(normal, sampleDir), 0.0);
        float sampleDepth = texture(depthTexture, sampleUv).x;
        float depthDelta = centerDepth - sampleDepth;
        float rangeWeight = 1.0 / (1.0 + 12.0 * distance2);
        occlusion += step(bias, depthDelta) * angular * rangeWeight;
    }

    float ao = 1.0 - clamp(occlusion / float(sampleCount), 0.0, 1.0);
    Color = vec4(baseColor * ao, 1.0);
}
