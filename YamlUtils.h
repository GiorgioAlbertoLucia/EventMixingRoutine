#pragma once

#include <vector>
#include <map>
#include <yaml-cpp/yaml.h>

namespace YamlUtils {
 
    template <typename T1>
    void ReadYamlVector(const YAML::Node& node, std::vector<T1>& vec) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            vec.push_back(it->as<T1>());
        }   
    }

    template <typename T1, typename T2>
    void ReadYamlMap(const YAML::Node& node, std::map<T1, T2>& map) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            assert(it->size() == 1);
            auto sub_it = it->begin();
            map[sub_it->first.as<T1>()] = sub_it->second.as<T2>();
        }
    }

} // namespace YamlUtils
