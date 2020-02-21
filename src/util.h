#ifndef UTIL_H
#define UTIL_H

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>

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

// std::hash for std::tuple
namespace std {
    template<typename... Ts>
    struct std::hash<std::tuple<Ts...>> {
        size_t operator()(std::tuple<Ts...> tup) const noexcept {
            return std::apply(hash_values<Ts...>, tup);
        }
    };
}

// shared cache
template<typename ResType, typename... ParamTypes>
struct shared_cache {
    static inline std::unordered_map<std::tuple<ParamTypes...>, std::shared_ptr<ResType>> store;

    static std::shared_ptr<ResType> load(ParamTypes... params) {
        auto found = store.find(std::make_tuple(params...));
        if (found == store.end()) {
            found = store.emplace(std::make_tuple(params...), std::make_shared<ResType>(params...)).first;
        }
        return found->second;
    }
};

#endif
