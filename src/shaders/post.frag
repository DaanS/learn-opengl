#version 420 core
in vec2 frag_tex_coords[9];

out vec4 frag_color;

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

uniform sampler2D tex;
uniform sampler2D bloom;

uniform bool use_gamma;
uniform float gamma;
uniform float exposure;
uniform bool use_bloom;
uniform bool use_filmic;

uniform bool DEBUG;

const float radius = 1.0;
const float offset = 0.5 * (radius / 450.0);

const float weights[] = float[] (0.06136, 0.24477, 0.38774, 0.24477, 0.06136);
const int kernel_size = 5;
const int half_kernel_size = kernel_size / 2;

vec3 bloom_color(vec2 tex_coords) {
    vec2 texel_size = 1.0 / textureSize(bloom, 0);

    vec3 color = vec3(0.0);

    for (int x = -half_kernel_size; x <= half_kernel_size; ++x) {
        for (int y = -half_kernel_size; y <= half_kernel_size; ++y) {
            vec3 bloom_sample = vec3(texture(bloom, tex_coords + texel_size * vec2(x, y)));
            float mult = weights[x + half_kernel_size] * weights[y + half_kernel_size];
            color += mult * bloom_sample;
        }
    }
    return color;
}

vec3 uncharted2_tonemap_partial(vec3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 uncharted2_filmic(vec3 v)
{
    float exposure_bias = 2.0f;
    vec3 curr = uncharted2_tonemap_partial(v * exposure_bias);

    vec3 W = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}

vec3 reinhard_jodie(vec3 v) {
    float l = dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
    vec3 tv = v / (1.0f + v);
    return mix(v / (1.0f + l), tv, tv);
}

void main() {
    vec3 color = texture(tex, frag_tex_coords[4]).rgb;
    if (use_bloom) color += bloom_color(frag_tex_coords[4]);

    // EV
    color = color * pow(2.0, user_ev);

    // tone mapping
    color = vec3(1.0) - exp(-color * exposure);

    // gamma correction
    color = pow(color, vec3(1.0 / gamma));

    frag_color = vec4(color, 1.0);

    //DEBUG
    if (DEBUG) frag_color = vec4(texture(tex, frag_tex_coords[4]).rgb, 1.0);
}