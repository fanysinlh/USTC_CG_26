#include "path.h"

#include <random>

#include "../surfaceInteraction.h"
RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;

VtValue PathIntegrator::Li(const GfRay& ray, std::default_random_engine& random)
{
    std::uniform_real_distribution<float> uniform_dist(
        0.0f, 1.0f - std::numeric_limits<float>::epsilon());
    std::function<float()> uniform_float = std::bind(uniform_dist, random);

    auto color = EstimateOutGoingRadiance(ray, uniform_float, 0);

    return VtValue(GfVec3f(color[0], color[1], color[2]));
}

GfVec3f PathIntegrator::EstimateOutGoingRadiance(
    const GfRay& ray,
    const std::function<float()>& uniform_float,
    int recursion_depth)
{
    if (recursion_depth >= 50) {
        return {};
    }

    GfVec3f light_hit_pos;
    GfVec3f light_radiance = IntersectLights(ray, light_hit_pos);

    SurfaceInteraction si;
    if (!Intersect(ray, si)) {
        if (recursion_depth == 0 &&
            GfDot(light_radiance, light_radiance) > 0.0f) {
            return light_radiance;
        }
        return recursion_depth == 0 ? IntersectDomeLight(ray) : GfVec3f(0.0f);
    }

    if (recursion_depth == 0 &&
        GfDot(light_radiance, light_radiance) > 0.0f) {
        const float light_depth = (light_hit_pos - GfVec3f(ray.GetStartPoint())).GetLength();
        const float surface_depth = (si.position - GfVec3f(ray.GetStartPoint())).GetLength();
        if (light_depth < surface_depth) {
            return light_radiance;
        }
    }

    // This can be customized : Do we want to see the lights? (Other than dome
    // lights?)
    if (recursion_depth == 0) {
    }

    // Flip the normal if opposite
    if (GfDot(si.shadingNormal, ray.GetDirection()) > 0) {
        si.flipNormal();
        si.PrepareTransforms();
    }

    GfVec3f color{ 0 };
    GfVec3f directLight = EstimateDirectLight(si, uniform_float);

    GfVec3f globalLight = GfVec3f{ 0.f };
    const float rr_prob = recursion_depth < 3 ? 1.0f : 0.8f;
    if (uniform_float() < rr_prob) {
        GfVec3f wi;
        float pdf = 0.0f;
        const GfVec3f brdf = si.Sample(wi, pdf, uniform_float);
        const float cosine = GfDot(si.shadingNormal, wi);

        if (pdf > 1e-6f && cosine > 0.0f) {
            GfRay bounce_ray;
            const GfVec3f offset_normal =
                GfDot(wi, si.geometricNormal) >= 0.0f ? si.geometricNormal
                                                      : -si.geometricNormal;
            bounce_ray.SetPointAndDirection(
                si.position + 0.0001f * offset_normal, wi);
            const GfVec3f incoming = EstimateOutGoingRadiance(
                bounce_ray, uniform_float, recursion_depth + 1);
            globalLight =
                GfCompMult(brdf, incoming) * cosine / (pdf * rr_prob);
        }
    }

    color = directLight + globalLight;

    return color;
}

RUZINO_NAMESPACE_CLOSE_SCOPE
