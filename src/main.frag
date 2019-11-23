#version 330 core

struct dir_light_type {
    vec3 dir;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct point_light_type {
    vec3 pos;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
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
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

in vec3 frag_pos;
in vec3 vert_normal;
in vec2 tex_coords;

out vec4 frag_color;

uniform vec3 view_pos;
uniform sampler2D material_diffuse[1];
uniform sampler2D material_specular[1];
uniform float shininess;

uniform dir_light_type dir_light;

#define POINT_LIGHT_COUNT 4
uniform point_light_type point_lights[POINT_LIGHT_COUNT];

uniform spot_light_type spot_light;

vec3 calc_dir_light(dir_light_type light, vec3 normal, vec3 view_dir) {
    vec3 light_dir = normalize(-light.dir);

    vec3 ambient_color = light.ambient * vec3(texture(material_diffuse[0], tex_coords));

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * light.diffuse * vec3(texture(material_diffuse[0], tex_coords));

    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec_strength = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);
    vec3 spec_color = spec_strength * light.specular * vec3(texture(material_specular[0], tex_coords));

    return ambient_color + diff_color + spec_color;
}

vec3 calc_point_light(point_light_type light, vec3 normal, vec3 view_dir, vec3 frag_pos) {
    vec3 light_dir = normalize(light.pos - frag_pos);

    vec3 ambient_color = light.ambient * vec3(texture(material_diffuse[0], tex_coords));

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * light.diffuse * vec3(texture(material_diffuse[0], tex_coords));

    vec3 reflect_dir = reflect(-light_dir, normal);
    vec4 spec = texture(material_specular[0], tex_coords);
    float spec_strength = pow(max(dot(view_dir, reflect_dir), 0.0), spec.a * 32.0);
    vec3 spec_color = spec_strength * light.specular * spec.rgb;

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 result = attenuation * (ambient_color + diff_color + spec_color);
    return vec3(spec.r, spec.g, spec.b) + 0.01 * result;
}

vec3 calc_spot_light(spot_light_type light, vec3 normal, vec3 view_dir, vec3 frag_pos) {
    vec3 light_dir = normalize(light.pos - frag_pos);

    vec3 ambient_color = light.ambient * vec3(texture(material_diffuse[0], tex_coords));

    float diff_strength = max(dot(normal, light_dir), 0.0);
    vec3 diff_color = diff_strength * light.diffuse * vec3(texture(material_diffuse[0], tex_coords));

    vec3 reflect_dir = reflect(-light_dir, normal);
    vec4 spec = texture(material_specular[0], tex_coords);
    float spec_strength = pow(max(dot(view_dir, reflect_dir), 0.0), spec.a * 32.0);
    vec3 spec_color = spec_strength * light.specular * spec.rgb;

    float dist = length(light.pos - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    float theta = dot(light_dir, normalize(-light.dir));
    float intensity = clamp((theta - light.outer_cutoff) / (light.cutoff - light.outer_cutoff), 0.0, 1.0);

    return intensity * attenuation * (ambient_color + diff_color + spec_color);
}

void main() {
    vec4 tex_color = texture(material_diffuse[0], tex_coords);
    if (tex_color.a < 0.5) discard;

    vec3 normal = normalize(vert_normal);
    vec3 view_dir = normalize(view_pos - frag_pos);

    vec3 result = calc_dir_light(dir_light, normal, view_dir);

    for (int i  = 0; i < POINT_LIGHT_COUNT; ++i) {
        result += calc_point_light(point_lights[i], normal, view_dir, frag_pos);
    }

    result += 0.01 * calc_spot_light(spot_light, normal, view_dir, frag_pos);

    frag_color = vec4(result, 1.0);
}
