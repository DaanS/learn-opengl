#version 330 core

struct material_type {
    float shininess;
    float refraction;
    float opacity_value;

    vec3 color_ambient;
    vec3 color_diffuse;
    vec3 color_specular;
    vec3 color_emissive;
    vec3 color_transport;

    sampler2D diffuse;
    sampler2D specular;
    sampler2D emissive;
    sampler2D bump;
    sampler2D normal;
    sampler2D opacity;

    bool has_diffuse_map;
    bool has_specular_map;
    bool has_emissive_map;
    bool has_bump_map;
    bool has_normal_map;
    bool has_opacity_map;
};

in vec2 frag_tex_coords;
in vec3 frag_pos;
in vec4 frag_pos_light_space;
in vec3 frag_normal;
in mat3 tbn;

layout (location = 0) out vec3 pos;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec3 diffuse;
layout (location = 3) out vec3 specular;
layout (location = 4) out vec3 emissive;
layout (location = 5) out vec3 misc; // r = gloss;

// TODO possible PBR layout
// 0 pos
// 1 normal
// 2 albedo
// 3 emissive
// 4 ao roughness metallic
// 5 height

uniform material_type material;

uniform bool use_frag_tbn;

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

void main() {
    if (material.has_opacity_map) {
        vec4 tex_color = texture(material.opacity, frag_tex_coords);
        if (tex_color.r < 0.1) discard;
    } else {
        vec4 tex_color = texture(material.diffuse, frag_tex_coords);
        if (tex_color.a < 0.1) discard;
    }

    pos = frag_pos;

    if (material.has_normal_map) {
        normal = vec3(texture(material.normal, frag_tex_coords));
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize((use_frag_tbn ? cotangent_frame(normalize(frag_normal), frag_pos, frag_tex_coords) : tbn) * normal);
    } else {
        normal = normalize(frag_normal);
    }

    diffuse = material.has_diffuse_map ? vec3(texture(material.diffuse, frag_tex_coords)) : material.color_diffuse;
    specular = material.has_specular_map ? vec3(texture(material.specular, frag_tex_coords)) : material.color_specular;
    emissive = material.has_emissive_map ? vec3(texture(material.emissive, frag_tex_coords)) : material.color_emissive;
    misc = vec3(material.shininess, 0.0, 0.0);
}