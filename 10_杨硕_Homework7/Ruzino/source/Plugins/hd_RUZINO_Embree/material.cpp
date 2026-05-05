// #define __GNUC__

#include "material.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "RHI/internal/map.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/usd/usd/tokens.h"
#include "pxr/usdImaging/usdImaging/tokens.h"
#include "renderParam.h"
#include "texture.h"
#include "utils/sampling.hpp"

RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;

namespace {
float Saturate(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

GfVec3f SchlickFresnel(float cosTheta, const GfVec3f& F0)
{
    float oneMinusCos = 1.0f - Saturate(cosTheta);
    float factor = oneMinusCos * oneMinusCos * oneMinusCos * oneMinusCos *
                   oneMinusCos;
    return F0 + (GfVec3f(1.0f) - F0) * factor;
}

float GGXDistribution(float NdotH, float alpha)
{
    float a2 = alpha * alpha;
    float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (M_PI * denom * denom + 1e-6f);
}

float SmithG1(float NdotV, float alpha)
{
    float a = alpha;
    float k = (a + 1.0f) * (a + 1.0f) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k + 1e-6f);
}

float SmithGeometry(float NdotV, float NdotL, float alpha)
{
    return SmithG1(NdotV, alpha) * SmithG1(NdotL, alpha);
}
}

// Here for the cource purpose, we support a very limited set of forms of the
// material. Specifically, we support only UsdPreviewSurface, and each input can
// be either value, or a texture connected to a primvar reader.

HdMaterialNode2 Hd_RUZINO_Material::get_input_connection(
    HdMaterialNetwork2 surfaceNetwork,
    std::map<TfToken, std::vector<HdMaterialConnection2>>::value_type&
        input_connection)
{
    HdMaterialNode2 upstream;
    assert(input_connection.second.size() == 1);
    upstream = surfaceNetwork.nodes[input_connection.second[0].upstreamNode];
    return upstream;
}

Hd_RUZINO_Material::MaterialRecord Hd_RUZINO_Material::SampleMaterialRecord(
    GfVec2f texcoord)
{
    MaterialRecord ret;
    if (diffuseColor.image) {
        auto val4 = diffuseColor.image->Evaluate(texcoord);
        ret.diffuseColor = { val4[0], val4[1], val4[2] };
    }
    else {
        ret.diffuseColor = diffuseColor.value.Get<GfVec3f>();
    }

    if (roughness.image) {
        auto val4 = roughness.image->Evaluate(texcoord);
        ret.roughness = val4[1];
    }
    else {
        ret.roughness = roughness.value.Get<float>();
    }

    if (ior.image) {
        auto val4 = ior.image->Evaluate(texcoord);
        ret.ior = val4[0];
    }
    else {
        ret.ior = ior.value.Get<float>();
    }

    if (metallic.image) {
        auto val4 = metallic.image->Evaluate(texcoord);
        ret.metallic = val4[2];
    }
    else {
        ret.metallic = metallic.value.Get<float>();
    }

    ret.roughness = std::clamp(ret.roughness, 0.04f, 1.0f);
    ret.metallic = std::clamp(ret.metallic, 0.0f, 1.0f);

    return ret;
}

void Hd_RUZINO_Material::TryLoadTexture(
    const char* name,
    InputDescriptor& descriptor,
    HdMaterialNode2& usd_preview_surface)
{
    for (auto&& input_connection : usd_preview_surface.inputConnections) {
        if (input_connection.first == TfToken(name)) {
            spdlog::info(
                "Loading texture: " + input_connection.first.GetString());
            auto texture_node =
                get_input_connection(surfaceNetwork, input_connection);
            assert(texture_node.nodeTypeId == UsdImagingTokens->UsdUVTexture);

            auto assetPath =
                texture_node.parameters[TfToken("file")].Get<SdfAssetPath>();

            HioImage::SourceColorSpace colorSpace;

            if (texture_node.parameters[TfToken("sourceColorSpace")] ==
                TfToken("sRGB")) {
                colorSpace = HioImage::SRGB;
            }
            else {
                colorSpace = HioImage::Raw;
            }

            descriptor.image =
                std::make_unique<Texture2D>(assetPath, colorSpace);
            if (!descriptor.image->isValid()) {
                descriptor.image = nullptr;
            }
            descriptor.wrapS =
                texture_node.parameters[TfToken("wrapS")].Get<TfToken>();
            descriptor.wrapT =
                texture_node.parameters[TfToken("wrapT")].Get<TfToken>();

            HdMaterialNode2 st_read_node;
            for (auto&& st_read_connection : texture_node.inputConnections) {
                st_read_node =
                    get_input_connection(surfaceNetwork, st_read_connection);
            }

            assert(
                st_read_node.nodeTypeId ==
                UsdImagingTokens->UsdPrimvarReader_float2);
            descriptor.uv_primvar_name =
                st_read_node.parameters[TfToken("varname")].Get<TfToken>();
            if (descriptor.uv_primvar_name.empty()) {
                descriptor.uv_primvar_name =
                    st_read_node.parameters[TfToken("varname")]
                        .Get<std::string>();
            }
        }
    }
}

void Hd_RUZINO_Material::TryLoadParameter(
    const char* name,
    InputDescriptor& descriptor,
    HdMaterialNode2& usd_preview_surface)
{
    for (auto&& parameter : usd_preview_surface.parameters) {
        if (parameter.first == name) {
            descriptor.value = parameter.second;
            spdlog::info("Loading parameter: " + parameter.first.GetString());
        }
    }
}

#define INPUT_LIST                                                            \
    diffuseColor, specularColor, emissiveColor, displacement, opacity,        \
        opacityThreshold, roughness, metallic, clearcoat, clearcoatRoughness, \
        occlusion, normal, ior

#define TRY_LOAD(INPUT)                                 \
    TryLoadTexture(#INPUT, INPUT, usd_preview_surface); \
    TryLoadParameter(#INPUT, INPUT, usd_preview_surface);

#define NAME_IT(INPUT) INPUT.input_name = TfToken(#INPUT);

Hd_RUZINO_Material::Hd_RUZINO_Material(const SdfPath& id) : HdMaterial(id)
{
    spdlog::info("Creating material " + id.GetString());
    diffuseColor.value = VtValue(GfVec3f(0.8f));
    roughness.value = VtValue(0.8f);

    metallic.value = VtValue(0.0f);
    normal.value = VtValue(GfVec3f(0.5, 0.5, 1.0));
    ior.value = VtValue(1.5f);

    MACRO_MAP(NAME_IT, INPUT_LIST);
}

void Hd_RUZINO_Material::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    VtValue vtMat = sceneDelegate->GetMaterialResource(GetId());
    if (vtMat.IsHolding<HdMaterialNetworkMap>()) {
        const HdMaterialNetworkMap& hdNetworkMap =
            vtMat.UncheckedGet<HdMaterialNetworkMap>();
        if (!hdNetworkMap.terminals.empty() && !hdNetworkMap.map.empty()) {
            spdlog::info("Loaded a material");

            surfaceNetwork = HdConvertToHdMaterialNetwork2(hdNetworkMap);

            // Here we only support single output material.
            assert(surfaceNetwork.terminals.size() == 1);

            auto terminal =
                surfaceNetwork.terminals[HdMaterialTerminalTokens->surface];

            auto usd_preview_surface =
                surfaceNetwork.nodes[terminal.upstreamNode];
            assert(
                usd_preview_surface.nodeTypeId ==
                UsdImagingTokens->UsdPreviewSurface);

            MACRO_MAP(TRY_LOAD, INPUT_LIST)
        }
    }
    else {
        spdlog::info("Not loaded a material");
    }
    *dirtyBits = Clean;
}

HdDirtyBits Hd_RUZINO_Material::GetInitialDirtyBitsMask() const
{
    return AllDirty;
}

#define requireTexCoord(INPUT)            \
    if (!INPUT.uv_primvar_name.empty()) { \
        return INPUT.uv_primvar_name;     \
    }

std::string Hd_RUZINO_Material::requireTexcoordName()
{
    MACRO_MAP(requireTexCoord, INPUT_LIST)
    return {};
}

void Hd_RUZINO_Material::Finalize(HdRenderParam* renderParam)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    HdMaterial::Finalize(renderParam);
}

Color Hd_RUZINO_Material::Sample(
    const GfVec3f& wo,
    GfVec3f& wi,
    float& pdf,
    GfVec2f texcoord,
    const std::function<float()>& uniform_float)
{
    auto record = SampleMaterialRecord(texcoord);
    const float specular_weight =
        std::clamp(0.25f + 0.5f * record.metallic, 0.05f, 0.95f);
    const float xi = uniform_float();

    if (xi < specular_weight) {
        float half_pdf = 0.0f;
        GfVec3f h = GGXWeightedDirection(
            GfVec2f{ uniform_float(), uniform_float() },
            record.roughness,
            half_pdf);
        if (wo[2] <= 0.0f) {
            pdf = 0.0f;
            wi = GfVec3f(0.0f);
            return GfVec3f(0.0f);
        }
        wi = (2.0f * GfDot(wo, h) * h - wo).GetNormalized();
        if (wi[2] <= 0.0f) {
            pdf = 0.0f;
            return GfVec3f(0.0f);
        }
    }
    else {
        wi = CosineWeightedDirection(
            GfVec2f{ uniform_float(), uniform_float() }, pdf);
    }

    pdf = Pdf(wi, wo, texcoord);
    return Eval(wi, wo, texcoord);
}

Color Hd_RUZINO_Material::Eval(GfVec3f wi, GfVec3f wo, GfVec2f texcoord)
{
    auto record = SampleMaterialRecord(texcoord);
    if (wi[2] <= 0.0f || wo[2] <= 0.0f) {
        return GfVec3f(0.0f);
    }

    GfVec3f diffuseColor = record.diffuseColor;
    float metallic = record.metallic;
    float roughness = record.roughness;
    float alpha = roughness * roughness;

    GfVec3f h = (wi + wo).GetNormalized();
    float NdotL = Saturate(wi[2]);
    float NdotV = Saturate(wo[2]);
    float NdotH = Saturate(h[2]);
    float VdotH = Saturate(GfDot(wo, h));

    GfVec3f F0 = GfVec3f(0.04f) * (1.0f - metallic) + diffuseColor * metallic;
    GfVec3f F = SchlickFresnel(VdotH, F0);
    float D = GGXDistribution(NdotH, alpha);
    float G = SmithGeometry(NdotV, NdotL, alpha);

    GfVec3f specular =
        F * (D * G / std::max(4.0f * NdotV * NdotL, 1e-6f));
    GfVec3f kd = (GfVec3f(1.0f) - F) * (1.0f - metallic);
    constexpr float kInvPi = 1.0f / 3.14159265359f;
    GfVec3f diffuse(
        kd[0] * diffuseColor[0] * kInvPi,
        kd[1] * diffuseColor[1] * kInvPi,
        kd[2] * diffuseColor[2] * kInvPi);

    return diffuse + specular;
}

float Hd_RUZINO_Material::Pdf(GfVec3f wi, GfVec3f wo, GfVec2f texcoord)
{
    auto record = SampleMaterialRecord(texcoord);
    if (wi[2] <= 0.0f || wo[2] <= 0.0f) {
        return 0.0f;
    }

    const float specular_weight =
        std::clamp(0.25f + 0.5f * record.metallic, 0.05f, 0.95f);
    const float diffuse_weight = 1.0f - specular_weight;

    float diffuse_pdf = wi[2] / M_PI;

    float roughness = record.roughness;
    float alpha = roughness * roughness;
    GfVec3f h = (wi + wo).GetNormalized();
    float NdotH = Saturate(h[2]);
    float VdotH = std::max(GfDot(wo, h), 1e-6f);
    float D = GGXDistribution(NdotH, alpha);
    float specular_pdf = D * NdotH / (4.0f * VdotH + 1e-6f);

    return diffuse_weight * diffuse_pdf + specular_weight * specular_pdf;
}

RUZINO_NAMESPACE_CLOSE_SCOPE
