#version 420 core

in vec2 frag_tex_coords;

out vec4 frag_ssao;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

uniform sampler2D g_bufs[6];
uniform sampler2D noise;

#define SSAO_SAMPLE_SIZE 64
uniform vec3 samples[SSAO_SAMPLE_SIZE];

const float radius = 2.0;
const float bias = 0.3;

void main() {
    vec2 noise_scale = textureSize(g_bufs[0], 0) / 4.0;

    vec3 frag_pos = vec3(view * vec4(texture(g_bufs[0], frag_tex_coords).rgb, 1.0));
    vec3 normal = vec3(view * vec4(texture(g_bufs[1], frag_tex_coords).rgb, 0.0));
    vec3 random = texture(noise, frag_tex_coords * noise_scale).rgb;

    vec3 tangent = normalize(random - normal * dot(random, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < SSAO_SAMPLE_SIZE; ++i) {
        vec3 sample_tmp = tbn * samples[i];
        sample_tmp = frag_pos + sample_tmp * radius;

        vec4 offset = vec4(sample_tmp, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sample_depth = (view * vec4(texture(g_bufs[0], offset.xy).rgb, 1.0)).z;
        float range_check = smoothstep(0.0, 1.0, radius / abs(frag_pos.z - sample_depth));
        occlusion += (sample_depth >= sample_tmp.z + bias ? 1.0 : 0.0) * range_check;
    }

    frag_ssao = vec4(vec3(1.0 - (occlusion / SSAO_SAMPLE_SIZE)), 1.0);
}