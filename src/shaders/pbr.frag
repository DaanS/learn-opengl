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
};

in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_tex_coords;

out vec4 frag_color;

#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform material_type material;
uniform vec3 view_pos;

vec3 fresnel_schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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

vec3 calc_pbr_light(point_light_type light, vec3 V, vec3 N) {
    vec3 L = normalize(light.pos - frag_pos);
    vec3 H = normalize(V + L);

    vec3 Lw = light.pos - frag_pos;
    float attenuation = 1.0 / dot(Lw, Lw);
    vec3 radiance = light.color * attenuation;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.albedo, material.metallic);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    float NDF = distribution_ggx(N, H, material.roughness);
    float G = geometry_smith(NdotL, NdotV, material.roughness);

    vec3 num = NDF * G * F;
    float denom = 4.0 * NdotV * NdotL;
    vec3 specular = num / max(denom, 0.001);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - material.metallic;

    return (kD * material.albedo / PI + specular) * radiance * NdotL;
}

void main() {
    vec3 N = normalize(frag_normal);
    vec3 V = normalize(view_pos - frag_pos);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < POINT_LIGHT_COUNT; ++i) {
        Lo += calc_pbr_light(point_lights[i], V, N);
    }

    vec3 ambient = vec3(0.03) * material.albedo * material.ao;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}