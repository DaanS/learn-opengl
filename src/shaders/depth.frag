#version 330 core

struct material_type {
    bool has_opacity_map;
    sampler2D opacity;
};

in vec2 frag_tex_coords;

uniform material_type material;

void main() {
    if (material.has_opacity_map) {
        vec4 tex_color = texture(material.opacity, frag_tex_coords);
        if (tex_color.r < 0.1) discard;
    }

    // just update the depth buffer
}