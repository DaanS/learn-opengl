#version 330 core

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

in vec3 frag_pos;
in vec4 frag_pos_light_space;
in vec3 frag_normal;
in vec2 frag_tex_coords;
in mat3 tbn;

in vec3 deb;

out vec4 frag_color;

uniform vec3 view_pos;

uniform dir_light_type dir_light;
#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];
uniform spot_light_type spot_light;

uniform material_type material;

uniform float far;

float shadow_strength_dir(sampler2DShadow shadow_map, vec3 light_dir) {
    float bias = clamp(0.001 * tan(acos(dot(normalize(frag_normal), light_dir))), 0, 0.005);

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
        vec2 sample = vec2(-3.5 + 2.0 * (i % 4), -3.5 + 2.0 * (i / 4));
        shadow += texture(shadow_map, proj_coords + vec3((offset + sample) * texel_size, 0));
    }
    shadow /= 16.0;

    return shadow;
}

float shadow_strength_point(samplerCubeShadow shadow_cube, vec3 frag_pos, vec3 light_pos) {
    vec3 light_to_frag = frag_pos - light_pos;

    float cur_depth = length(light_to_frag);
    float bias = 0.001;
    float shadow = texture(shadow_cube, vec4(frag_pos - light_pos, (cur_depth / far) - bias));

    return shadow;
}

vec3 calc_base_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 light_dir, float shadow) {
    vec3 normal;
    if (material.has_normal_map) {
        normal = vec3(texture(material.normal, frag_tex_coords));
        normal = normalize(normal * 2.0 - 1.0);
        normal = normalize(tbn * normal);
    } else {
        normal = normalize(frag_normal);
    }
    //return normal * 0.5 + 0.5;

    vec3 ambient_color = ambient * (material.has_diffuse_map ? vec3(texture(material.diffuse, frag_tex_coords)) : material.color_ambient);

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * diffuse * (material.has_diffuse_map ? vec3(texture(material.diffuse, frag_tex_coords)) : material.color_diffuse);

    vec3 view_dir = normalize(view_pos - frag_pos);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec_strength = pow(max(dot(normal, halfway_dir), 0.0), 2 * material.shininess);
    vec3 spec_color = spec_strength * specular * (material.has_specular_map ? vec3(texture(material.specular, frag_tex_coords)) : material.color_specular);

    vec3 em_color = (material.has_emissive_map ? vec3(texture(material.emissive, frag_tex_coords)) : vec3(0.0));

    return max(em_color, ambient_color + (1.0 - shadow) * (diff_color + spec_color));
}

vec3 calc_dir_light(dir_light_type light) {
    vec3 light_dir = normalize(-light.dir);
    float shadow = shadow_strength_dir(light.shadow_map, light_dir);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, shadow);

    return result;
}

vec3 calc_point_light(point_light_type light) {
    vec3 light_dir = normalize(light.pos - frag_pos);
    float shadow = shadow_strength_point(light.shadow_cube, frag_pos, light.pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, shadow);

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    result *= attenuation;
    return result;
}

vec3 calc_spot_light(spot_light_type light) {
    vec3 light_dir = normalize(light.pos - frag_pos);
    vec3 result = calc_base_light(light.ambient, light.diffuse, light.specular, light_dir, 0.0);

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    float theta = dot(light_dir, normalize(-light.dir));
    float intensity = clamp((theta - light.outer_cutoff) / (light.cutoff - light.outer_cutoff), 0.0, 1.0);

    result *= attenuation * intensity;
    return result;
}

void main() {
    if (material.has_opacity_map) {
        vec4 tex_color = texture(material.opacity, frag_tex_coords);
        if (tex_color.r < 0.1) discard;
    } else {
        vec4 tex_color = texture(material.diffuse, frag_tex_coords);
        //if (tex_color.a < 0.1) discard;
    }

    vec3 result = calc_dir_light(dir_light);

    for (int i  = 0; i < POINT_LIGHT_COUNT; ++i) result += calc_point_light(point_lights[i]);

    result += calc_spot_light(spot_light);

    frag_color = vec4(result, 1.0);
}
