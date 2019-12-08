#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 5) out;

in vec2 geom_tex_coords[];

out vec2 frag_tex_coords;

uniform float time;

vec3 get_normal() {
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
}

vec4 explode(vec4 pos, vec3 normal) {
    float magnitude = 2.0;
    vec3 dir = normal * ((sin(time) + 1.0) / 2.0) * magnitude;
    return pos + vec4(dir, 0.0);
}

void main() {
    vec3 normal = get_normal();

    gl_Position = explode(gl_in[0].gl_Position, normal);
    frag_tex_coords = geom_tex_coords[0];
    EmitVertex();

    gl_Position = explode(gl_in[1].gl_Position, normal);
    frag_tex_coords = geom_tex_coords[1];
    EmitVertex();

    gl_Position = explode(gl_in[2].gl_Position, normal);
    frag_tex_coords = geom_tex_coords[2];
    EmitVertex();

    EndPrimitive();
}