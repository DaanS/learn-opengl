#version 330 core

#define PI 3.14159265

struct point_light_type {
    vec3 pos;

    vec3 color;
};

struct material_type {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;

    bool has_albedo_map;
    bool has_metallic_map;
    bool has_roughness_map;
    bool has_ao_map;
    bool has_normal_map;

    sampler2D albedo_map;
    sampler2D metallic_map;
    sampler2D roughness_map;
    sampler2D ao_map;
    sampler2D normal_map;
};

in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_tex_coords;

out vec4 frag_color;

#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform material_type material;
uniform vec3 view_pos;
uniform samplerCube irradiance_map;
uniform samplerCube prefilter_map;
uniform sampler2D brdf_lut;

uniform bool use_ibl;
uniform bool use_fsr;
uniform bool use_corr;
uniform bool use_lamb;

vec3 albedo = material.has_albedo_map ? texture(material.albedo_map, frag_tex_coords).rgb : material.albedo;
float metallic = material.has_metallic_map ? texture(material.metallic_map, frag_tex_coords).r : material.metallic;
float roughness = material.has_roughness_map ? texture(material.roughness_map, frag_tex_coords).r : material.roughness;
float ao = material.has_ao_map ? texture(material.ao_map, frag_tex_coords).r : material.ao;
vec3 normal = material.has_normal_map ? texture(material.normal_map, frag_tex_coords).rgb : frag_normal;

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

vec3 fresnel_schlick_90(float cos_theta, vec3 F0, float F90) {
    return F0 + (F90 - F0) * pow(1.0 - cos_theta, 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cos_theta, 5.0);
}

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom_base = (NdotH2 * (a2 - 1.0) + 1.0);
    float denom = PI * denom_base * denom_base;

    return num / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometry_smith(float NdotV, float NdotL, float roughness) {
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

float geometry_smith_correlated(float NdotV, float NdotL, float roughness) {
    float a2 = roughness * roughness;
    float NdotV2 = NdotV * NdotV;
    float NdotL2 = NdotL * NdotL;

    float lambda_v = (-1 + sqrt(a2 * (1 - NdotL2) / NdotL2 + 1)) * 0.5f;
    float lambda_l = (-1 + sqrt(a2 * (1 - NdotV2) / NdotV2 + 1)) * 0.5f;

    float G = 1 / (1 + lambda_v + lambda_l);
    float V = G / max(4.0 * NdotL * NdotV, 0.001);

    return V;
}

float fr_disney_diffuse(float NdotV, float NdotL, float LdotH, float roughness) {
    float a = roughness * roughness;
    float linear_roughness = a * a;

    float energy_bias = mix(0, 0.5, linear_roughness);
    float energy_factor = mix(1.0, 1.0 / 1.51, linear_roughness);
    float Fd90 = energy_bias + 2.0 * LdotH * LdotH * linear_roughness;
    vec3 F0 = vec3(1.0);
    float light_scatter = fresnel_schlick_90(NdotL, F0, Fd90).r;
    float view_scatter = fresnel_schlick_90(NdotV, F0, Fd90).r;

    return light_scatter * view_scatter * energy_factor;
}

vec3 calc_pbr_light(point_light_type light, vec3 V, vec3 N) {
    vec3 L = normalize(light.pos - frag_pos);
    vec3 H = normalize(V + L);

    vec3 Lw = light.pos - frag_pos;
    float attenuation = 1.0 / dot(Lw, Lw);
    vec3 radiance = light.color * attenuation;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.00001);

    float D = distribution_ggx(N, H, roughness);
    float G = use_corr ? geometry_smith_correlated(NdotL, NdotV, roughness) : geometry_smith(NdotL, NdotV, roughness);

    vec3 num = D * G * F;
    float denom = use_corr ? 1 : 4.0 * NdotV * NdotL;
    vec3 specular = num / max(denom, 0.001);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float LdotH = max(dot(L, H), 0.0);
    vec3 diffuse = (use_lamb ? fr_disney_diffuse(NdotV, NdotL, LdotH, roughness) : 1.0) * albedo / PI;

    return (kD * diffuse + specular) * radiance * NdotL;
}

void main() {
    vec3 N = normalize(frag_normal);
    vec3 V = normalize(view_pos - frag_pos);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < POINT_LIGHT_COUNT; ++i) {
        Lo += calc_pbr_light(point_lights[i], V, N);
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    if (use_ibl) {
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 kS = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, (use_fsr ? roughness : 0.0));
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        vec3 irradiance = texture(irradiance_map, N).rgb;
        vec3 diffuse = irradiance * albedo;

        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefiltererd_color = textureLod(prefilter_map, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefiltererd_color * (F0 * brdf.x + brdf.y);

        ambient = (kD * diffuse + specular) * ao;
    }

    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}