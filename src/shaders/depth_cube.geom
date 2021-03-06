#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

in vec2 geom_tex_coords[];

uniform mat4 shadow_transforms[6];

out vec4 frag_pos;
out vec2 frag_tex_coords;

void main() {
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int i = 0; i < gl_in.length(); ++i) {
            frag_pos = gl_in[i].gl_Position;
            frag_tex_coords = geom_tex_coords[i];
            gl_Position = shadow_transforms[face] * frag_pos;
            EmitVertex();
        }
        EndPrimitive();
    }
}