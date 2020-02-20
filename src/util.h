#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <fstream>
#include <iostream>

// load a file into a single string
template<typename PathType>
std::string load_string(PathType path) {
    std::ifstream ifs(path);
    if (!ifs) std::cerr << "ERROR opening " << path << std::endl;
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

// stream output operator for glm::vec3
std::ostream& operator<<(std::ostream& os, glm::vec3 v) {
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}

// helper templates to combine multiple hashes
void hash_combine(size_t&) {}

template<typename T, typename... Ts>
void hash_combine(size_t& h, T t, Ts... ts) {
    h ^= std::hash<T>()(t) + 0x9e3779b9 + (h << 6 ) + (h >> 2);
    hash_combine(h, ts...);
}

template<typename... Ts>
size_t hash_values(Ts... ts) {
    size_t res{0};
    hash_combine(res, ts...);
    return res;
}

#endif
