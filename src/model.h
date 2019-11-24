#include <vector>
#include <memory>

#include <glad/glad.h>
#include <SDL_opengl.h>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shader.h"
#include "texture.h"

template<typename AiType, typename OurType>
OurType ai_get(aiMaterial const * mat, char const * key, unsigned int type, unsigned int idx) {
    AiType res;
    mat->Get(key, type, idx, res);
    return OurType{res};
}

template<>
std::string ai_get<aiString, std::string>(aiMaterial const * mat, char const * key, unsigned int  type, unsigned int idx) {
    aiString res;
    mat->Get(key, type, idx, res);
    return std::string{res.C_Str()};
}

template<>
glm::vec3 ai_get<aiColor3D, glm::vec3>(aiMaterial const * mat, char const * key, unsigned int type, unsigned int idx) {
    aiColor3D res;
    mat->Get(key, type, idx, res);
    return glm::vec3{res.r, res.g, res.b};
}

struct vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 tex_coords;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

struct material {
    std::string name;

    float shininess;
    float refraction; // not yet used
    float opacity; // not yet used

    glm::vec3 color_ambient; // not yet used
    glm::vec3 color_diffuse; // not yet used
    glm::vec3 color_specular;
    glm::vec3 color_emissive; // not yet used
    glm::vec3 color_transport; // not yet used

    std::shared_ptr<texture> diffuse;
    std::shared_ptr<texture> specular;
    std::shared_ptr<texture> emissive; // not yet used
    std::shared_ptr<texture> bump;
    std::shared_ptr<texture> normal; // not yet used

    static std::unordered_map<std::string, std::shared_ptr<texture>> loaded_textures;

    material(aiMaterial const * ai_material, std::string tex_path_base) {
        name = ai_get<aiString, std::string>(ai_material, AI_MATKEY_NAME);

        shininess = ai_get<float, float>(ai_material, AI_MATKEY_SHININESS);
        refraction = ai_get<float, float>(ai_material, AI_MATKEY_REFRACTI);
        opacity = ai_get<float, float>(ai_material, AI_MATKEY_OPACITY);

        color_ambient = ai_get<aiColor3D, glm::vec3>(ai_material, AI_MATKEY_COLOR_AMBIENT);
        color_diffuse = ai_get<aiColor3D, glm::vec3>(ai_material, AI_MATKEY_COLOR_DIFFUSE);
        color_specular = ai_get<aiColor3D, glm::vec3>(ai_material, AI_MATKEY_COLOR_SPECULAR);
        color_emissive = ai_get<aiColor3D, glm::vec3>(ai_material, AI_MATKEY_COLOR_EMISSIVE);
        color_transport = ai_get<aiColor3D, glm::vec3>(ai_material, AI_MATKEY_COLOR_TRANSPARENT);

        diffuse = load_texture(ai_material, aiTextureType_DIFFUSE, tex_path_base);
        specular = load_texture(ai_material, aiTextureType_SPECULAR, tex_path_base);
        emissive = load_texture(ai_material, aiTextureType_EMISSIVE, tex_path_base);
        bump = load_texture(ai_material, aiTextureType_HEIGHT, tex_path_base);
        normal = load_texture(ai_material, aiTextureType_NORMALS, tex_path_base);
    }

    static std::shared_ptr<texture> load_texture(aiMaterial const * ai_material, aiTextureType ai_type, std::string tex_path_base) {
        size_t count = ai_material->GetTextureCount(ai_type);

        if (count == 0) return nullptr;
        if (count > 1) std::cout << "WARNING: mesh specifies " << count << " textures of type " << ai_type << " but we only support 1" << std::endl;

        aiString path;
        ai_material->GetTexture(ai_type, 0, &path);
        std::string tex_path{tex_path_base + path.C_Str()};
        std::replace(tex_path.begin(), tex_path.end(), '\\', '/');

        auto found{loaded_textures.find(tex_path)};
        if (found != loaded_textures.end()) {
            return found->second;
        } else {
            auto tex = std::make_shared<texture>(tex_path);
            loaded_textures.emplace(std::pair{tex_path, tex});
            return tex;
        }
    }
};

inline std::unordered_map<std::string, std::shared_ptr<texture>> material::loaded_textures;

struct mesh {
    std::vector<vertex> vertices;
    std::vector<GLuint> indices;
    material mat;

    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    // TODO figure out ownership semantics for members
    mesh(std::vector<vertex> && vertices, std::vector<GLuint> && indices, material mat)
        : vertices{std::move(vertices)}, indices{std::move(indices)}, mat{mat}
    {
        setup_gl_data();
    }

    void setup_gl_data() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(decltype(vertices)::value_type), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(decltype(indices)::value_type), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, pos));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, normal));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, tex_coords));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, tangent));
        glEnableVertexAttribArray(3);

        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, bitangent));
        glEnableVertexAttribArray(4);

        glBindVertexArray(0);
    }

    void draw(shader_program const & program) const {
        program.set_uniform("material.color_diffuse", mat.color_diffuse);
        program.set_uniform("material.has_diffuse_map", false);
        if (mat.diffuse) {
            glActiveTexture(GL_TEXTURE0);
            program.set_uniform("material.diffuse", 0);
            program.set_uniform("material.has_diffuse_map", true);
            glBindTexture(GL_TEXTURE_2D, mat.diffuse->id);
        }

        program.set_uniform("material.shininess", mat.shininess);
        program.set_uniform("material.color_specular", mat.color_specular);
        program.set_uniform("material.has_specular_map", false);
        if (mat.specular) {
            glActiveTexture(GL_TEXTURE1);
            program.set_uniform("material.specular", 1);
            program.set_uniform("material.has_specular_map", true);
            glBindTexture(GL_TEXTURE_2D, mat.specular->id);
        }

        program.set_uniform("material.has_bump_map", false);
        if (mat.bump) {
            glActiveTexture(GL_TEXTURE3);
            program.set_uniform("material.bump", 3);
            program.set_uniform("material.has_bump_map", true);
            glBindTexture(GL_TEXTURE_2D, mat.bump->id);
        }

        program.set_uniform("material.has_normal_map", false);
        if (mat.normal) {
            glActiveTexture(GL_TEXTURE4);
            program.set_uniform("material.normal", 4);
            program.set_uniform("material.has_normal_map", true);
            glBindTexture(GL_TEXTURE_2D, mat.normal->id);
        }

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }
};

struct model {
    std::vector<mesh> meshes;
    std::string directory;
    std::unordered_map<std::string, texture> loaded_textures;

    model(char const * path) {
        load_model(path);
    }

    void load_model(std::string const path) {
        Assimp::Importer import;
        aiScene const * scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals | aiProcess_FixInfacingNormals |
        aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData | aiProcess_OptimizeGraph);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
            return;
        }

       directory = path.substr(0, path.find_last_of('/') + 1);

       process_node(scene->mRootNode, scene);
    }

    void process_node(aiNode const * node, aiScene const * scene) {
        for (size_t i = 0; i < node->mNumMeshes; ++i) {
            aiMesh const * mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(process_mesh(mesh, scene));
        }

        for (size_t i = 0; i < node->mNumChildren; ++i) {
            process_node(node->mChildren[i], scene);
        }
    }

    mesh process_mesh(aiMesh const * ai_mesh, aiScene const * scene) {
        std::vector<vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<texture> textures;

        // load vertices
        for (size_t i = 0; i < ai_mesh->mNumVertices; ++i) {
            vertices.push_back({
                glm::vec3(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z),
                glm::vec3(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z),
                ai_mesh->mTextureCoords[0]
                    ? glm::vec2(ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y)
                    : glm::vec2(0.0f, 0.0f),
                glm::vec3(ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z),
                glm::vec3(ai_mesh->mBitangents[i].x, ai_mesh->mBitangents[i].y, ai_mesh->mBitangents[i].z)
            });
        }

        // load indices
        for (size_t i = 0; i < ai_mesh->mNumFaces; ++i) {
            for (size_t j = 0; j < ai_mesh->mFaces[i].mNumIndices; ++j) {
                indices.push_back(ai_mesh->mFaces[i].mIndices[j]);
            }
        }

        // load textures
        aiMaterial const * ai_material;
        if (ai_mesh->mMaterialIndex >= 0) {
            ai_material = scene->mMaterials[ai_mesh->mMaterialIndex];
        } else {
            std::cerr << "ERROR: mesh has no material, using default" << std::endl;
            ai_material = scene->mMaterials[0];
        }

        mesh m{std::move(vertices), std::move(indices), material{ai_material, directory}};
        return m;
    }

    void draw(shader_program const& program) {
        for (auto& mesh : meshes) mesh.draw(program);
    }
};
