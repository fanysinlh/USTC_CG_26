#include "light.h"

#include <cmath>

#include <spdlog/spdlog.h>

#include "pxr/base/gf/plane.h"
#include "pxr/base/gf/ray.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/imaging/glf/simpleLight.h"
#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hio/image.h"
#include "renderParam.h"
#include "texture.h"
#include "utils/math.hpp"
#include "utils/sampling.hpp"

RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;
void Hd_RUZINO_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    if (!TF_VERIFY(sceneDelegate != nullptr)) {
        return;
    }

    const SdfPath& id = GetId();

    // Change tracking
    HdDirtyBits bits = *dirtyBits;

    // Transform
    if (bits & DirtyTransform) {
        _params[HdTokens->transform] = VtValue(sceneDelegate->GetTransform(id));
    }

    // Lighting Params
    if (bits & DirtyParams) {
        HdChangeTracker& changeTracker =
            sceneDelegate->GetRenderIndex().GetChangeTracker();

        // Remove old dependencies
        VtValue val = Get(HdTokens->filters);
        if (val.IsHolding<SdfPathVector>()) {
            auto lightFilterPaths = val.UncheckedGet<SdfPathVector>();
            for (const SdfPath& filterPath : lightFilterPaths) {
                changeTracker.RemoveSprimSprimDependency(filterPath, id);
            }
        }

        if (_lightType == HdPrimTypeTokens->simpleLight) {
            _params[HdLightTokens->params] =
                sceneDelegate->Get(id, HdLightTokens->params);
        }

        // Add new dependencies
        val = Get(HdTokens->filters);
        if (val.IsHolding<SdfPathVector>()) {
            auto lightFilterPaths = val.UncheckedGet<SdfPathVector>();
            for (const SdfPath& filterPath : lightFilterPaths) {
                changeTracker.AddSprimSprimDependency(filterPath, id);
            }
        }
    }

    *dirtyBits = Clean;
}

HdDirtyBits Hd_RUZINO_Light::GetInitialDirtyBitsMask() const
{
    if (_lightType == HdPrimTypeTokens->simpleLight ||
        _lightType == HdPrimTypeTokens->distantLight) {
        return AllDirty;
    }
    else {
        return (DirtyParams | DirtyTransform);
    }
}

bool Hd_RUZINO_Light::IsDomeLight()
{
    return _lightType == HdPrimTypeTokens->domeLight;
}

void Hd_RUZINO_Light::Finalize(HdRenderParam* renderParam)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    HdLight::Finalize(renderParam);
}

VtValue Hd_RUZINO_Light::Get(const TfToken& token) const
{
    VtValue val;
    TfMapLookup(_params, token, &val);
    return val;
}

Color Hd_RUZINO_Sphere_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,

    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    auto distanceVec = position - pos;

    auto basis = constructONB(-distanceVec.GetNormalized());

    auto distance = distanceVec.GetLength();

    // A sphere light is treated as all points on the surface spreads energy
    // uniformly:
    float sample_pos_pdf;
    // First we sample a point on the hemi sphere:
    auto sampledDir = CosineWeightedDirection(
        GfVec2f(uniform_float(), uniform_float()), sample_pos_pdf);
    auto worldSampledDir = basis * sampledDir;

    auto sampledPosOnSurface = worldSampledDir * radius + position;
    sampled_light_pos = sampledPosOnSurface;

    // Then we can decide the direction.
    dir = (sampledPosOnSurface - pos).GetNormalized();

    // and the pdf (with the measure of solid angle):
    float cosVal = GfDot(-dir, worldSampledDir.GetNormalized());

    sample_light_pdf =
        sample_pos_pdf / radius / radius / cosVal * distance * distance;

    // Finally we calculate the radiance
    if (cosVal < 0) {
        return Color{ 0 };
    }
    return irradiance / M_PI;
}

Color Hd_RUZINO_Sphere_Light::Intersect(const GfRay& ray, float& depth)
{
    double distance;
    if (ray.Intersect(
            GfRange3d{ position - GfVec3d{ radius },
                       position + GfVec3d{ radius } })) {
        if (ray.Intersect(position, radius, &distance)) {
            depth = distance;

            return irradiance / M_PI;
        }
    }
    depth = std::numeric_limits<float>::infinity();
    return { 0, 0, 0 };
}

float Hd_RUZINO_Sphere_Light::PdfLi(const GfVec3f& pos, const GfVec3f& dir)
{
    TF_UNUSED(pos);
    TF_UNUSED(dir);
    return 0.0f;
}

void Hd_RUZINO_Sphere_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    auto id = GetId();

    radius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius)
                 .Get<float>();

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();

    auto intensity =
        sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity)
            .GetWithDefault<float>();
    power = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                .Get<GfVec3f>() *
            diffuse * intensity;

    auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();

    GfVec3d p = transform.ExtractTranslation();
    position = GfVec3f(p[0], p[1], p[2]);

    area = 4 * M_PI * radius * radius;

    irradiance = power / area;
}

Color Hd_RUZINO_Dome_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,
    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    dir = UniformSampleSphere(
        GfVec2f{ uniform_float(), uniform_float() }, sample_light_pdf);
    sampled_light_pos = dir * std::numeric_limits<float>::max() / 100.f;

    return Le(dir);
}

Color Hd_RUZINO_Dome_Light::Intersect(const GfRay& ray, float& depth)
{
    depth = 10000000.f;
    return Le(GfVec3f(ray.GetDirection()).GetNormalized());
}

float Hd_RUZINO_Dome_Light::PdfLi(const GfVec3f& pos, const GfVec3f& dir)
{
    TF_UNUSED(pos);
    TF_UNUSED(dir);
    return 1.0f / (4.0f * M_PI);
}

void Hd_RUZINO_Dome_Light::_PrepareDomeLight(
    SdfPath const& id,
    HdSceneDelegate* sceneDelegate)
{
    const VtValue v =
        sceneDelegate->GetLightParamValue(id, HdLightTokens->textureFile);
    if (!v.IsEmpty()) {
        if (v.IsHolding<SdfAssetPath>()) {
            textureFileName = v.UncheckedGet<SdfAssetPath>();
            texture = std::make_unique<Texture2D>(textureFileName);
            if (!texture->isValid()) {
                texture = nullptr;
            }

            spdlog::info(
                ("Attempting to load file " + textureFileName.GetAssetPath())
                    .c_str());
        }
        else {
            texture = nullptr;
        }
    }
    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    radiance = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                   .Get<GfVec3f>() *
               diffuse;
}

void Hd_RUZINO_Dome_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);

    auto id = GetId();
    _PrepareDomeLight(id, sceneDelegate);
}

Color Hd_RUZINO_Dome_Light::Le(const GfVec3f& dir)
{
    if (texture != nullptr) {
        auto uv = GfVec2f(
            (M_PI + std::atan2(dir[1], dir[0])) / 2.0 / M_PI,
            0.5 - dir[2] * 0.5);

        auto value = texture->Evaluate(uv);

        if (texture->component_conut() >= 3) {
            return GfCompMult(Color{ value[0], value[1], value[2] }, radiance);
        }
    }
    return radiance;
}

void Hd_RUZINO_Dome_Light::Finalize(HdRenderParam* renderParam)
{
    texture = nullptr;
    Hd_RUZINO_Light::Finalize(renderParam);
}

// HW7_TODO: write the following, you should refer to the sphere light.

void Hd_RUZINO_Distant_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    auto id = GetId();
    angle = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle)
                .Get<float>();
    angle = std::clamp(angle, 0.03f, 89.9f) * M_PI / 180.0f;

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    radiance = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                   .Get<GfVec3f>() *
               diffuse / (1 - cos(angle)) / 2.0 / M_PI;

    auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();

    direction =
        GfVec3f(transform.TransformDir(GfVec3f(0, 0, -1)).GetNormalized());
}

Color Hd_RUZINO_Distant_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,
    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    TF_UNUSED(pos);
    const float cosThetaMax = std::cos(angle);
    const float cosTheta =
        1.0f - uniform_float() * (1.0f - cosThetaMax);
    const float sinTheta =
        std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = uniform_float() * 2 * M_PI;

    auto sampled_dir =
        GfVec3f(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

    auto basis = constructONB(-direction);

    dir = basis * sampled_dir;
    sampled_light_pos = pos + dir * 10000000.f;

    sample_light_pdf = 1.0f / (2.0f * M_PI * (1.0f - cosThetaMax));

    return radiance;
}

Color Hd_RUZINO_Distant_Light::Intersect(const GfRay& ray, float& depth)
{
    depth = 10000000.f;

    if (GfDot(ray.GetDirection().GetNormalized(), -direction) > cos(angle)) {
        return radiance;
    }
    return Color(0);
}

float Hd_RUZINO_Distant_Light::PdfLi(const GfVec3f& pos, const GfVec3f& dir)
{
    TF_UNUSED(pos);
    const float cosTheta = GfDot(dir.GetNormalized(), -direction);
    if (cosTheta < std::cos(angle)) {
        return 0.0f;
    }
    return 1.0f / (2.0f * M_PI * (1.0f - std::cos(angle)));
}

Color Hd_RUZINO_Rect_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,
    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    const GfVec3f edge_u = corner2 - corner0;
    const GfVec3f edge_v = corner1 - corner0;
    const GfVec3f normal = GfCross(edge_u, edge_v).GetNormalized();

    const float u = uniform_float();
    const float v = uniform_float();
    sampled_light_pos = corner0 + u * edge_u + v * edge_v;

    GfVec3f to_light = sampled_light_pos - pos;
    const float distance2 = GfDot(to_light, to_light);
    if (distance2 <= 1e-8f) {
        sample_light_pdf = 0.0f;
        return Color(0);
    }

    dir = to_light / std::sqrt(distance2);
    const float cos_light = GfDot(normal, -dir);
    if (cos_light <= 1e-6f || area <= 1e-8f) {
        sample_light_pdf = 0.0f;
        return Color(0);
    }

    sample_light_pdf = distance2 / (cos_light * area);
    return irradiance / M_PI;
}

// HW7_TODO: implement the intersect function for rectangle light, you can refer to the sphere light, but you need to consider the fact that rectangle light is not a point light source.
Color Hd_RUZINO_Rect_Light::Intersect(const GfRay& ray, float& depth)
{
    const GfVec3f edge_u = corner2 - corner0;
    const GfVec3f edge_v = corner1 - corner0;
    const GfVec3f normal = GfCross(edge_u, edge_v).GetNormalized();

    const float denom = GfDot(normal, GfVec3f(ray.GetDirection()));
    if (std::abs(denom) <= 1e-6f || denom >= 0.0f) {
        depth = std::numeric_limits<float>::infinity();
        return Color(0);
    }

    const GfVec3f origin = GfVec3f(ray.GetStartPoint());
    depth = GfDot(corner0 - origin, normal) / denom;
    if (depth <= 0.0f) {
        depth = std::numeric_limits<float>::infinity();
        return Color(0);
    }

    const GfVec3f hit = GfVec3f(ray.GetPoint(depth));
    const GfVec3f local = hit - corner0;
    const float uu = GfDot(edge_u, edge_u);
    const float uv = GfDot(edge_u, edge_v);
    const float vv = GfDot(edge_v, edge_v);
    const float det = uu * vv - uv * uv;
    if (det <= 1e-8f) {
        depth = std::numeric_limits<float>::infinity();
        return Color(0);
    }

    const float lu = GfDot(local, edge_u);
    const float lv = GfDot(local, edge_v);
    const float u = (lu * vv - lv * uv) / det;
    const float v = (lv * uu - lu * uv) / det;
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
        depth = std::numeric_limits<float>::infinity();
        return Color(0);
    }

    return irradiance / M_PI;
}

float Hd_RUZINO_Rect_Light::PdfLi(const GfVec3f& pos, const GfVec3f& dir)
{
    GfRay ray;
    ray.SetPointAndDirection(pos, dir);

    float depth;
    auto radiance = Intersect(ray, depth);
    if (GfDot(radiance, radiance) <= 0.0f ||
        depth == std::numeric_limits<float>::infinity()) {
        return 0.0f;
    }

    const GfVec3f edge_u = corner2 - corner0;
    const GfVec3f edge_v = corner1 - corner0;
    const GfVec3f normal = GfCross(edge_u, edge_v).GetNormalized();
    const GfVec3f hit = GfVec3f(ray.GetPoint(depth));
    const float distance2 = GfDot(hit - pos, hit - pos);
    const float cos_light = GfDot(normal, -dir);
    if (cos_light <= 1e-6f || area <= 1e-8f) {
        return 0.0f;
    }
    return distance2 / (cos_light * area);
}

void Hd_RUZINO_Rect_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);

    auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();

    auto id = GetId();
    width = sceneDelegate->GetLightParamValue(id, HdLightTokens->width)
                .Get<float>();
    height = sceneDelegate->GetLightParamValue(id, HdLightTokens->height)
                 .Get<float>();

    corner0 = GfVec3f(
        transform.TransformAffine(GfVec3f(-0.5 * width, -0.5 * height, 0)));
    corner1 = GfVec3f(
        transform.TransformAffine(GfVec3f(-0.5 * width, 0.5 * height, 0)));
    corner2 = GfVec3f(
        transform.TransformAffine(GfVec3f(0.5 * width, -0.5 * height, 0)));
    corner3 = GfVec3f(
        transform.TransformAffine(GfVec3f(0.5 * width, 0.5 * height, 0)));

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    auto intensity =
        sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity)
            .GetWithDefault<float>();
    power = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                .Get<GfVec3f>() *
            diffuse * intensity;

    area = GfCross(corner2 - corner0, corner1 - corner0).GetLength();
    irradiance = area > 1e-8f ? power / area : GfVec3f(0.0f);
}

RUZINO_NAMESPACE_CLOSE_SCOPE
