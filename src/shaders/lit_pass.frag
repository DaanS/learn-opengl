#version 420 core

struct dir_light_type {
    vec3 dir;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    sampler2DShadow shadow_map;
};

struct point_light_type {
    vec3 pos;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    samplerCubeShadow shadow_cube;
};

struct spot_light_type {
    vec3 pos;
    vec3 dir;

    float cutoff;
    float outer_cutoff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

layout (std140, binding = 0) uniform vp {
    mat4 view;
    mat4 projection;
    float user_ev;
};

out vec4 frag_color;

in vec2 frag_tex_coords;

uniform sampler2D g_bufs[6];
uniform sampler2D ssao;
uniform float far;
uniform mat4 light_space;
uniform bool use_ao;

uniform dir_light_type dir_light;
#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform spot_light_type spot_light;
uniform bool use_spotlight;
uniform int point_light_count;

// Ideas:
//  - precompute inverse(view)
//  - store light_space for each light to make it easier to convert frag_pos to light space

float shadow_strength_dir(sampler2DShadow shadow_map, vec3 light_dir) {
    vec3 frag_normal = texture(g_bufs[1], frag_tex_coords).rgb;
    float bias = clamp(0.005 * tan(acos(dot(normalize(frag_normal), light_dir))), 0.0, 0.005);

    vec3 frag_pos = texture(g_bufs[0], frag_tex_coords).rgb;
    vec4 frag_pos_light_space = light_space * inverse(view) * vec4(frag_pos, 1.0);
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;
    proj_coords.z -= bias;

    if (proj_coords.z > 1.0) return 0.0;

    float shadow = 0.0;

    vec2 texel_size = 1.0 / textureSize(shadow_map, 0);
    vec2 offset = vec2(lessThan(fract((gl_FragCoord.xy - 0.5) * 0.5), vec2(0.25)));
    offset.y += offset.x;
    if (offset.y > 1.1) offset.y = 0;
    for (int i = 0; i < 16; ++i) {
        vec2 sample_pos = vec2(-3.5 + 2.0 * (i % 4), -3.5 + 2.0 * (i / 4));
        shadow += texture(shadow_map, proj_coords + vec3((offset + sample_pos) * texel_size, 0));
    }
    shadow /= 16.0;

    return shadow;
}

float shadow_strength_point(samplerCubeShadow shadow_cube, vec3 frag_pos_view, vec3 light_pos) {
    vec3 frag_pos = vec3(inverse(view) * vec4(frag_pos_view, 1.0));
    vec3 light_to_frag = frag_pos - light_pos;

    float cur_depth = length(light_to_frag);
    float bias = 0.001;
    float shadow = texture(shadow_cube, vec4(frag_pos - light_pos, (cur_depth / far) - bias));

    return shadow;
}

vec3 calc_base_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 light_dir, float shadow) {
    vec3 normal = texture(g_bufs[1], frag_tex_coords).rgb;

    vec3 diffuse_src = texture(g_bufs[2], frag_tex_coords).rgb;
    float ao = use_ao ? texture(ssao, frag_tex_coords).r : 1.0;
    vec3 ambient_color = ambient * diffuse_src * ao;

    float diffuse_strength = max(dot(normal, light_dir), 0.0);
    vec3 diffuse_color = diffuse_strength * diffuse * diffuse_src;

    vec3 frag_pos = texture(g_bufs[0], frag_tex_coords).rgb;
    vec3 view_dir = normalize(-frag_pos);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float gloss = texture(g_bufs[5], frag_tex_coords).r;
    float specular_strength = pow(max(dot(normal, halfway_dir), 0.0), 2 * gloss);
    vec3 specular_src = texture(g_bufs[3], frag_tex_coords).rgb;
    vec3 specular_color = specular_strength * specular * specular_src;

    vec3 emissive_src = texture(g_bufs[4], frag_tex_coords).rgb;
    vec3 em_color = emissive_src * pow(2.0, -user_ev);

    return em_color + ambient_color + (1.0 - shadow) * (diffuse_color + specular_color);
}

vec3 calc_dir_light(dir_light_type light) {
    vec3 light_dir = normalize(vec3(view * vec4(-light.dir, 0.0)));
    float shadow = shadow_strength_dir(light.shadow_map, light_dir);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, shadow);

    return result;
}

vec3 calc_point_light(point_light_type light) {
    vec3 frag_pos = texture(g_bufs[0], frag_tex_coords).rgb;
    vec3 light_pos = vec3(view * vec4(light.pos, 1.0));
    vec3 light_dir = normalize(light_pos - frag_pos);
    float shadow = shadow_strength_point(light.shadow_cube, frag_pos, light.pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, shadow);

    float dist = length(light_pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    result *= attenuation;
    return result;
}

vec3 calc_spot_light(spot_light_type light) {
    vec3 frag_pos = texture(g_bufs[0], frag_tex_coords).rgb;
    vec3 light_pos = vec3(view * vec4(light.pos, 1.0));
    vec3 light_dir = normalize(light_pos - frag_pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, 0.0);

    float dist = length(light_pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    float theta = dot(light_dir, vec3(0.0, 0.0, 1.0));
    float intensity = clamp((theta - light.outer_cutoff) / (light.cutoff - light.outer_cutoff), 0.0, 1.0);

    result *= attenuation * intensity;
    return result;
}

void main() {
    vec3 result = vec3(0.0);
    result += calc_dir_light(dir_light);
    for (int i = 0; i < POINT_LIGHT_COUNT; ++i) {
        result += calc_point_light(point_lights[i]);
    }
    if (use_spotlight) result += calc_spot_light(spot_light);

    frag_color = vec4(result, 1.0);
}