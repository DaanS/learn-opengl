#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in vec3 geom_normal[];

const float magnitude = 0.4;

void main() {
    for (int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
        gl_Position = gl_in[i].gl_Position + vec4(geom_normal[i], 0.0) * magnitude;
        EmitVertex();
        EndPrimitive();
    }
}