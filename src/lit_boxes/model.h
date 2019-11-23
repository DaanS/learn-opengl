#include <vector>

#include <glad/glad.h>
#include <SDL_opengl.h>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shader.h"
#include "texture.h"

struct vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 tex_coords;
};

struct mesh {
    std::vector<vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<texture> textures;

    GLuint vao;
    GLuint vbo;
    GLuint ebo;

    // TODO figure out ownership semantics for members
    mesh(std::vector<vertex> && vertices, std::vector<GLuint> && indices, std::vector<texture> && textures)
        : vertices{vertices}, indices{indices}, textures{std::move(textures)}
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

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, pos));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, tex_coords));
    }

    void draw(shader_program const & program) const {
        unsigned int diffuse_count = 0;
        unsigned int specular_count = 0;

        for (size_t i = 0; i < textures.size(); ++i) {
            glActiveTexture(GL_TEXTURE0 + i);

            // TODO figure out a way to NOT do this in the render loop
            std::string name = "material";
            switch (textures[i].type) {
                case texture_type::diffuse: name += "diffuse[" + std::to_string(diffuse_count++) + "]"; break;
                case texture_type::specular: name += "specular[" + std::to_string(specular_count++) + "]"; break;
                default: std::cerr << "ERROR determining texture type" << std::endl; break;
            }

            program.set_uniform(name, i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

struct model {
    std::vector<mesh> meshes;
    std::string directory;

    model(char const * path) {
        load_model(path);
    }

    void load_model(std::string const path) {
       Assimp::Importer import;
       aiScene const * scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

       if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
           std::cerr << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
           return;
       }

       directory = path.substr(0, path.find_last_of('/'));

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

    mesh process_mesh(aiMesh const * mesh, aiScene const * scene) {
        std::vector<vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<texture> textures;

        for (size_t i = 0; i < mesh->mNumVertices; ++i) {
            vertices.push_back({
                glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z),
                glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z),
                mesh->mTextureCoords[0]
                ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : glm::vec2(0.0f, 0.0f)                
            });
        }

        for (size_t i = 0; i < mesh->mNumFaces; ++i) {
            for (size_t j = 0; j < mesh->mFaces[i].mNumIndices; ++j) {
                indices.push_back(mesh->mFaces[i].mIndices[j]);
            }
        }

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial const * material = scene->mMaterials[mesh->mMaterialIndex];
            std::vector<texture> diffuse_maps = load_material_textures(material, aiTextureType_DIFFUSE, texture_type::diffuse);
            std::vector<texture> specular_maps = load_material_textures(material, aiTextureType_SPECULAR, texture_type::specular);
            textures.insert(textures.end(), std::make_move_iterator(diffuse_maps.begin()), std::make_move_iterator(diffuse_maps.end()));
            textures.insert(textures.end(), std::make_move_iterator(specular_maps.begin()), std::make_move_iterator(specular_maps.end()));
        }

        return {std::move(vertices), std::move(indices), std::move(textures)};
    }

    std::vector<texture> load_material_textures(aiMaterial const * material, aiTextureType ai_type, texture_type type) {
        std::vector<texture> textures;

        for (size_t i = 0; i < material->GetTextureCount(ai_type); ++i) {
            aiString str;
            material->GetTexture(ai_type, i, &str);
            textures.push_back(texture{str.C_Str(), type});
        }

        return textures;
    }

    void draw(shader_program const& program) {
        for (auto& mesh : meshes) mesh.draw(program);
    }
};