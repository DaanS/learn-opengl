#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <fstream>
#include <iostream>

template<typename PathType>
std::string load_string(PathType path) {
    std::ifstream ifs(path);
    if (!ifs) std::cerr << "ERROR opening " << path << std::endl;
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

std::ostream& operator<<(std::ostream& os, glm::vec3 v) {
    os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    return os;
}

#endif
