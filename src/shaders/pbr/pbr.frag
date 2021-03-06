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
    bool has_height_map;

    sampler2D albedo_map;
    sampler2D metallic_map;
    sampler2D roughness_map;
    sampler2D ao_map;
    sampler2D normal_map;
    sampler2D height_map;
};

in vec3 frag_pos;
in vec3 frag_normal;
in vec2 frag_tex_coords;
in mat3 vert_tbn;
in mat3 inv_vert_tbn;

in vec3 tan_view_pos;
in vec3 tan_frag_pos;

out vec4 frag_color;

#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform material_type material;
uniform vec3 view_pos;
uniform samplerCube irradiance_map;
uniform samplerCube prefilter_map;
uniform sampler2D brdf_lut;

uniform bool use_ibl;
uniform bool use_lamb;
uniform bool use_par;
uniform bool use_vert_tbn;

vec2 parallax_mapping(vec2 tex_coords, vec3 view_dir) {
    if (!material.has_height_map || !use_par) return tex_coords;

    const float depth_scale = 0.1;
    const float layer_count = 30;
    float layer_depth = 1.0 / layer_count;
    float cur_layer_depth = 0.0;

    view_dir.y = -view_dir.y; // XXX why?
    vec2 P = view_dir.xy * depth_scale;
    vec2 delta_tex_coords = P / layer_count;

    vec2 cur_tex_coords = tex_coords;
    float cur_depth = 1.0 - texture(material.height_map, cur_tex_coords).r;

    while (cur_layer_depth < cur_depth) {
        cur_tex_coords -= delta_tex_coords;
        cur_depth = 1.0 - texture(material.height_map, cur_tex_coords).r;
        cur_layer_depth += layer_depth;
    }

    vec2 prev_tex_coords = cur_tex_coords + delta_tex_coords;
    float after_depth = cur_depth - cur_layer_depth;
    float before_depth = 1.0 - texture(material.height_map, prev_tex_coords).r - cur_layer_depth + layer_depth;

    float weight = after_depth / (after_depth - before_depth);
    vec2 final_tex_coords = prev_tex_coords * weight + cur_tex_coords * (1.0 - weight);

    return final_tex_coords;
}

float parallax_shadow(vec2 tex_coords, vec3 light_dir) {
    if (!material.has_height_map || !use_par) return 1.0;

    float shadow = 0.0;

    const float depth_scale = 0.1;
    const float layer_count = 30;
    float cur_layer_depth = 1.0 - texture(material.height_map, tex_coords).r;
    float layer_height = cur_layer_depth / layer_count;
    vec2 delta_tex_coords = light_dir.xy * depth_scale / layer_count;
    vec2 cur_tex_coords = tex_coords;

    while (cur_layer_depth > 0.0) {
        cur_layer_depth -= layer_height;
        cur_tex_coords += delta_tex_coords;
        float cur_depth = 1.0 - texture(material.height_map, cur_tex_coords).r;

        if (cur_depth < cur_layer_depth) {
            float cur_shadow = (cur_layer_depth - cur_depth) * ((cur_layer_depth / layer_height) / layer_count);
            shadow = max(shadow, cur_shadow);
        }
    }

    return 1.0 - shadow;
}

mat3 cotangent_frame(vec3 normal, vec3 pos, vec2 tex_coords) {
    vec3 dp1 = dFdx(pos);
    vec3 dp2 = dFdy(pos);

    vec2 duv1 = dFdx(tex_coords);
    vec2 duv2 = dFdy(tex_coords);

    float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);
    vec3 T = normalize(f * (duv2.y * dp1 - duv1.y * dp2));
    vec3 B = normalize(f * (duv2.x * dp1 - duv1.x * dp2));

    float flip = 1.0;
    if (dot(cross(T, B), normal) <= 0) flip = -1.0;
    T = normalize(T - dot(T, normal) * normal);
    B = cross(normal, T) * flip;

    return mat3(T, B, normal);
}

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

vec3 calc_pbr_light(point_light_type light, vec3 V, vec3 N, vec3 albedo, float metallic, float roughness) {
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
    float G = geometry_smith_correlated(NdotL, NdotV, roughness);

    vec3 specular = D * G * F;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float LdotH = max(dot(L, H), 0.0);
    vec3 diffuse = (use_lamb ? fr_disney_diffuse(NdotV, NdotL, LdotH, roughness) : 1.0) * albedo / PI;

    return (kD * diffuse + specular) * radiance * NdotL;
}

vec3 calc_ambient(vec3 V, vec3 N, vec3 albedo, float metallic, float roughness, float ao) {
    vec3 ambient = vec3(0.03) * albedo * ao;

    if (use_ibl) {
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 kS = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);
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

    return ambient;
}

void main() {
    mat3 tbn = cotangent_frame(normalize(frag_normal), frag_pos, frag_tex_coords); 
    mat3 inv_tbn = inverse(tbn);
    if (use_vert_tbn) inv_tbn = inv_vert_tbn;
    vec2 tex_coords = parallax_mapping(frag_tex_coords, normalize(inv_tbn * normalize(view_pos - frag_pos)));

    vec3 albedo = material.has_albedo_map ? texture(material.albedo_map, tex_coords).rgb : material.albedo;
    float metallic = material.has_metallic_map ? texture(material.metallic_map, tex_coords).r : material.metallic;
    float roughness = material.has_roughness_map ? texture(material.roughness_map, tex_coords).r : material.roughness;
    float ao = material.has_ao_map ? texture(material.ao_map, tex_coords).r : material.ao;
    vec3 normal = frag_normal;
    if (material.has_normal_map) {
        normal = texture(material.normal_map, tex_coords).rgb;
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(tbn * normal);
    }

    vec3 N = normalize(normal);
    vec3 V = normalize(view_pos - frag_pos);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < POINT_LIGHT_COUNT; ++i) {
        vec3 Lp = calc_pbr_light(point_lights[i], V, N, albedo, metallic, roughness);
        float shadow = parallax_shadow(tex_coords, normalize(inv_tbn * normalize(point_lights[i].pos - frag_pos)));
        Lo += Lp * shadow;
    }

    vec3 ambient = calc_ambient(V, N, albedo, metallic, roughness, ao);

    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, 1.0);
}