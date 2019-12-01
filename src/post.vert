#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 tex_coords;

out vec2 frag_tex_coords[9];

uniform float width;
uniform float height;

void main() {
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);

    vec2 texel_width = vec2(1.0 / width, 0.0);
    vec2 texel_height = vec2(0.0, 1.0 / height);

    frag_tex_coords[0] = tex_coords - texel_width - texel_height;
    frag_tex_coords[1] = tex_coords               - texel_height;
    frag_tex_coords[2] = tex_coords + texel_width - texel_height;
    frag_tex_coords[3] = tex_coords - texel_width;
    frag_tex_coords[4] = tex_coords;
    frag_tex_coords[5] = tex_coords + texel_width;
    frag_tex_coords[6] = tex_coords - texel_width + texel_height;
    frag_tex_coords[7] = tex_coords               + texel_height;
    frag_tex_coords[8] = tex_coords + texel_width + texel_height;
}