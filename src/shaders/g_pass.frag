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

uniform material_type material;

void main() {
    pos = frag_pos;

    if (material.has_normal_map) {
        normal = vec3(texture(material.normal, frag_tex_coords));
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(tbn * normal);
    } else {
        normal = normalize(frag_normal);
    }

    diffuse = material.has_diffuse_map ? vec3(texture(material.diffuse, frag_tex_coords)) : material.color_diffuse;
    specular = material.has_specular_map ? vec3(texture(material.specular, frag_tex_coords)) : material.color_specular;
}