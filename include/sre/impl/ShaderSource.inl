#include <map>
#include <utility>
#include <string>

std::map<std::string, std::string> builtInShaderSource  {
        std::make_pair<std::string,std::string>("light-phong.glsl",R"(
uniform vec3 g_ambientLight;
uniform vec4 g_lightPosType[S_LIGHTS];
uniform vec4 g_lightColorRange[S_LIGHTS];
uniform float specularity;

vec3 computeLight(vec3 wsPos, vec3 wsCameraPos){
    vec3 lightColor = vec3(0.0,0.0,0.0);
    vec3 normal = normalize(vNormal);
    for (int i=0;i<S_LIGHTS;i++){
        bool isDirectional = g_lightPosType[i].w == 0.0;
        bool isPoint       = g_lightPosType[i].w == 1.0;
        vec3 lightDirection;
        float att = 1.0;
        if (isDirectional){
            lightDirection = g_lightPosType[i].xyz;
        } else if (isPoint) {
            vec3 lightVector = g_lightPosType[i].xyz - wsPos;
            float lightVectorLength = length(lightVector);
            float lightRange = g_lightColorRange[i].w;
            lightDirection = lightVector / lightVectorLength; // compute normalized lightDirection (using length)
            if (lightRange <= 0.0){
                att = 1.0;
            } else if (lightVectorLength >= lightRange){
                att = 0.0;
            } else {
                att = pow(1.0 - lightVectorLength / lightRange,1.5); // non physical range based attenuation
            }
        } else {
            continue;
        }

        // diffuse light
        float thisDiffuse = max(0.0,dot(lightDirection, normal));
        if (thisDiffuse > 0.0){
            lightColor += (att * thisDiffuse) * g_lightColorRange[i].xyz;
        }

        // specular light
        if (specularity > 0.0){
            vec3 H = normalize(lightDirection - normalize(wsPos - wsCameraPos));
            float nDotHV = dot(normal, H);
            if (nDotHV > 0.0){
                float pf = pow(nDotHV, specularity);
                lightColor += vec3(att * pf); // white specular highlights
            }
        }
    }
    lightColor = max(g_ambientLight.xyz, lightColor);

    return lightColor;
}
)"),
        std::make_pair<std::string,std::string>("standard_vert.glsl",R"(#version 140
in vec3 position;
in vec3 normal;
in vec4 uv;
#ifdef S_TANGENTS
in vec4 tangent;
out mat3 vTBN;
#else
out vec3 vNormal;
#endif
out vec2 vUV;
out vec3 vLightDir[S_LIGHTS];
out vec3 vWsPos;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;
uniform mat3 g_model_it;
uniform vec4 g_lightPosType[S_LIGHTS];

void main(void) {
    vec4 wsPos = g_model * vec4(position,1.0);
    vWsPos = wsPos.xyz / wsPos.w;
    gl_Position = g_projection * g_view * wsPos;
#ifdef S_TANGENTS
    vec3 wsNormal = normalize(g_model_it * normal);
    vec3 wsTangent = normalize(g_model_it * tangent.xyz);
    vec3 wsBitangent = cross(wsNormal, wsTangent) * tangent.w;
#else
    vNormal = normalize(g_model_it * normal);
#endif
    vUV = uv.xy;

    for (int i=0;i<S_LIGHTS;i++){
        bool isDirectional = g_lightPosType[i].w == 0.0;
        bool isPoint       = g_lightPosType[i].w == 1.0;
        vec3 lightDirection;
        float att = 1.0;
        if (isDirectional){
            vLightDir[i] = g_lightPosType[i].xyz;
        } else if (isPoint) {
            vLightDir[i] = g_lightPosType[i].xyz - vWsPos;
        }
    }
}
)"),
        // implementation based of glTF 2.0's RPB shader
        std::make_pair<std::string,std::string>("standard_frag.glsl",R"(#version 140
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable
out vec4 fragColor;
in vec3 vNormal;
in vec2 vUV;
in vec3 vWsPos;
in vec3 vLightDir[S_LIGHTS];

uniform vec4 g_lightColorRange[S_LIGHTS];
uniform vec4 color;
uniform vec4 metallicRoughness;
uniform vec4 g_cameraPos;
uniform sampler2D tex;
uniform sampler2D mrTex;
#ifdef S_NORMALMAP
uniform sampler2D normalTex;
uniform float normalScale;
#endif

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal()
{
    // Retrieve the tangent space matrix
#ifndef S_TANGENTS
    vec3 pos_dx = dFdx(vWsPos);
    vec3 pos_dy = dFdy(vWsPos);
    vec3 tex_dx = dFdx(vec3(vUV, 0.0));
    vec3 tex_dy = dFdy(vec3(vUV, 0.0));
    vec3 t = (tex_dy.t * pos_dx - tex_dx.t * pos_dy) / (tex_dx.s * tex_dy.t - tex_dy.s * tex_dx.t);

    vec3 ng = normalize(vNormal);

    t = normalize(t - ng * dot(ng, t));
    vec3 b = normalize(cross(ng, t));
    mat3 tbn = mat3(t, b, ng);
#else // S_TANGENTS
    mat3 tbn = v_TBN;
#endif

#ifdef S_NORMALMAP
    vec3 n = texture2D(normalTex, vUV).rgb;
    n = normalize(tbn * ((2.0 * n - 1.0) * vec3(normalScale, normalScale, 1.0)));
#else
    vec3 n = tbn[2].xyz;
#endif

    return n;
}

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

const float M_PI = 3.141592653589793;
const float c_MinRoughness = 0.04;

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 diffuse(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor / M_PI;
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

void main(void)
{
    float perceptualRoughness = metallicRoughness.y;
    float metallic = metallicRoughness.x;

#ifdef S_METALROUGHNESSMAP
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    vec4 mrSample = texture2D(mrTex, vUV);
    perceptualRoughness = mrSample.g * perceptualRoughness;
    metallic = mrSample.b * metallic;
#endif
    perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = perceptualRoughness * perceptualRoughness;

#ifdef S_BASECOLORMAP
    vec4 baseColor = SRGBtoLINEAR(texture(tex, vUV)) * color;
#else
    vec4 baseColor = color;
#endif

    vec3 f0 = vec3(0.04);
    vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
    diffuseColor *= 1.0 - metallic;
    vec3 specularColor = mix(f0, baseColor.rgb, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflectance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;
    vec3 color = vec3(0.0,0.0,0.0);
    for (int i=0;i<S_LIGHTS;i++){
        vec3 n = getNormal();                             // Normal at surface point
        vec3 v = normalize(g_cameraPos.xyz - vWsPos.xyz); // Vector from surface point to camera
        vec3 l = normalize(vLightDir[i]);                 // Vector from surface point to light
        vec3 h = normalize(l+v);                          // Half vector between both l and v
        vec3 reflection = -normalize(reflect(v, n));

        float NdotL = clamp(dot(n, l), 0.001, 1.0);
        float NdotV = abs(dot(n, v)) + 0.001;
        float NdotH = clamp(dot(n, h), 0.0, 1.0);
        float LdotH = clamp(dot(l, h), 0.0, 1.0);
        float VdotH = clamp(dot(v, h), 0.0, 1.0);

        PBRInfo pbrInputs = PBRInfo(
            NdotL,
            NdotV,
            NdotH,
            LdotH,
            VdotH,
            perceptualRoughness,
            metallic,
            specularEnvironmentR0,
            specularEnvironmentR90,
            alphaRoughness,
            diffuseColor,
            specularColor
        );

        // Calculate the shading terms for the microfacet specular shading model
        vec3 F = specularReflection(pbrInputs);
        float G = geometricOcclusion(pbrInputs);
        float D = microfacetDistribution(pbrInputs);

        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
        vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
        color += NdotL * g_lightColorRange[i].xyz * (diffuseContrib + specContrib);

        //color = mix(color, F, u_ScaleFGDSpec.x);
        //color = mix(color, vec3(G), u_ScaleFGDSpec.y);
        //color = mix(color, vec3(D), u_ScaleFGDSpec.z);
        //color = mix(color, specContrib, u_ScaleFGDSpec.w);
        //color = mix(color, diffuseContrib, u_ScaleDiffBaseMR.x);
        //color = mix(color, baseColor.rgb, u_ScaleDiffBaseMR.y);
        //color = mix(color, vec3(metallic), u_ScaleDiffBaseMR.z);
        //color = mix(color, vec3(perceptualRoughness), u_ScaleDiffBaseMR.w);
    }
    fragColor = vec4(pow(color,vec3(1.0/2.2)), baseColor.a); // gamma correction
}
        )"),
        std::make_pair<std::string,std::string>("standard_phong_vert.glsl",R"(#version 140
in vec3 position;
in vec3 normal;
in vec4 uv;
out vec3 vNormal;
out vec2 vUV;
out vec3 vWsPos;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;
uniform mat3 g_model_it;

void main(void) {
    vec4 wsPos = g_model * vec4(position,1.0);
    gl_Position = g_projection * g_view * wsPos;
    vNormal = normalize(g_model_it * normal);
    vUV = uv.xy;
    vWsPos = vWsPos.xyz;
}
)"),
        std::make_pair<std::string,std::string>("standard_phong_frag.glsl",R"(#version 140
out vec4 fragColor;
in vec3 vNormal;
in vec2 vUV;
in vec3 vWsPos;
uniform vec4 g_cameraPos;

uniform vec4 color;
uniform sampler2D tex;

#pragma include "light-phong.glsl"

void main(void)
{
    vec4 c = color * texture(tex, vUV);

    vec3 l = computeLight(vWsPos, g_cameraPos.xyz);

    fragColor = c * vec4(l, 1.0);
}
        )"),
        std::make_pair<std::string,std::string>("unlit_vert.glsl",R"(#version 140
in vec3 position;
in vec3 normal;
in vec4 uv;
out vec2 vUV;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;

void main(void) {
    gl_Position = g_projection * g_view * g_model * vec4(position,1.0);
    vUV = uv.xy;
}
)"),
        std::make_pair<std::string,std::string>("unlit_frag.glsl", R"(#version 140
out vec4 fragColor;
in vec2 vUV;

uniform vec4 color;
uniform sampler2D tex;

void main(void)
{
    fragColor = color * texture(tex, vUV);
}
)"),
        std::make_pair<std::string,std::string>("sprite_vert.glsl",R"(#version 140
in vec3 position;
in vec4 uv;
in vec4 color;
out vec2 vUV;
out vec4 vColor;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;

void main(void) {
    gl_Position = g_projection * g_view * g_model * vec4(position,1.0);
    vUV = uv.xy;
    vColor = color;
}
        )"),
        std::make_pair<std::string,std::string>("sprite_frag.glsl", R"(#version 140
out vec4 fragColor;
in vec2 vUV;
in vec4 vColor;

uniform sampler2D tex;

void main(void)
{
    fragColor = vColor * texture(tex, vUV);
}
        )"),
        std::make_pair<std::string,std::string>("particles_vert.glsl", R"(#version 140
in vec3 position;
in float particleSize;
in vec4 uv;
in vec4 color;
out mat3 vUVMat;
out vec4 vColor;
out vec3 uvSize;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;
uniform vec4 g_viewport;

mat3 translate(vec2 p){
 return mat3(1.0,0.0,0.0,0.0,1.0,0.0,p.x,p.y,1.0);
}

mat3 rotate(float rad){
  float s = sin(rad);
  float c = cos(rad);
 return mat3(c,s,0.0,-s,c,0.0,0.0,0.0,1.0);
}

mat3 scale(float s){
  return mat3(s,0.0,0.0,0.0,s,0.0,0.0,0.0,1.0);
}

void main(void) {
    vec4 pos = vec4( position, 1.0);
    vec4 eyeSpacePos = g_view * g_model * pos;
    gl_Position = g_projection * eyeSpacePos;
    if (g_projection[2][3] != 0.0){ // if perspective projection
        gl_PointSize = (g_viewport.y / 600.0) * particleSize * 1.0 / -eyeSpacePos.z;
    } else {
        gl_PointSize = particleSize*(g_viewport.y / 600.0);
    }

    vUVMat = translate(uv.xy)*scale(uv.z) * translate(vec2(0.5,0.5))*rotate(uv.w) * translate(vec2(-0.5,-0.5));
    vColor = color;
    uvSize = uv.xyz;
}
)"),
        std::make_pair<std::string,std::string>("particles_frag.glsl", R"(#version 140
out vec4 fragColor;
in mat3 vUVMat;
in vec3 uvSize;
in vec4 vColor;
#ifdef GL_ES
uniform precision highp vec4 g_viewport;
else
uniform vec4 g_viewport;
#endif


uniform sampler2D tex;

void main(void)
{
    vec2 uv = (vUVMat * vec3(gl_PointCoord,1.0)).xy;

    if (uv != clamp(uv, uvSize.xy, uvSize.xy + uvSize.zz)){
        discard;
    }
    vec4 c = vColor * texture(tex, uv);
    fragColor = c;
}
)"),
        std::make_pair<std::string,std::string>("debug_uv_vert.glsl", R"(#version 140
in vec3 position;
in vec4 uv;
out vec2 vUV;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;

void main(void) {
    gl_Position = g_projection * g_view * g_model * vec4(position,1.0);
    vUV = uv.xy;
}
)"),
        std::make_pair<std::string,std::string>("debug_uv_frag.glsl", R"(#version 140
in vec2 vUV;
out vec4 fragColor;

void main(void)
{
    fragColor = vec4(vUV,0.0,1.0);
}
)"),
        std::make_pair<std::string,std::string>("debug_normal_vert.glsl", R"(#version 140
in vec3 position;
in vec3 normal;
out vec3 vNormal;

uniform mat4 g_model;
uniform mat4 g_view;
uniform mat4 g_projection;
uniform mat3 g_model_view_it;

void main(void) {
    gl_Position = g_projection * g_view * g_model * vec4(position,1.0);
    vNormal = g_model_view_it * normal;
}
)"),
        std::make_pair<std::string,std::string>("debug_normal_frag.glsl", R"(#version 140
out vec4 fragColor;
in vec3 vNormal;

void main(void)
{
    fragColor = vec4(vNormal*0.5+0.5,1.0);
}
)")
};