#include "asset/readers/prefab_reader.h"
#include "asset/core/math_types.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

namespace asset {

namespace {

// Row-major 4x4 multiply: out = a * b
void Mat4Mul(const float* a, const float* b, float* out) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out[row * 4 + col] = 0.f;
            for (int k = 0; k < 4; ++k)
                out[row * 4 + col] += a[row * 4 + k] * b[k * 4 + col];
        }
    }
}

// Parse "  - name: \"...\"" or "    - name: \"...\""
bool ParseName(const std::string& line, std::string& name) {
    size_t i = line.find("name:");
    if (i == std::string::npos) return false;
    i = line.find('\"', i);
    if (i == std::string::npos) return false;
    size_t j = line.find('\"', i + 1);
    if (j == std::string::npos) return false;
    name = line.substr(i + 1, j - (i + 1));
    return true;
}

// Parse "    transform: [ 1, 0, 0, ... ]"
bool ParseTransform(const std::string& line, float* m) {
    size_t i = line.find('[');
    if (i == std::string::npos) return false;
    std::string rest = line.substr(i + 1);
    int idx = 0;
    for (;;) {
        size_t comma = rest.find(',');
        std::string num = (comma == std::string::npos) ? rest : rest.substr(0, comma);
        float v = 0.f;
        try { v = std::stof(num); } catch (...) { return false; }
        if (idx < 16) m[idx++] = v;
        if (comma == std::string::npos) break;
        rest = rest.substr(comma + 1);
    }
    return idx == 16;
}

// Parse "    mesh: \"...\""
bool ParseMesh(const std::string& line, std::string& meshPath) {
    size_t i = line.find("mesh:");
    if (i == std::string::npos) return false;
    i = line.find('\"', i);
    if (i == std::string::npos) return false;
    size_t j = line.find('\"', i + 1);
    if (j == std::string::npos) return false;
    meshPath = line.substr(i + 1, j - (i + 1));
    return true;
}

// Parse "    meshIndex: 12"
bool ParseMeshIndex(const std::string& line, int& meshIndex) {
    size_t i = line.find("meshIndex:");
    if (i == std::string::npos) return false;
    i += std::strlen("meshIndex:");
    const size_t start = line.find_first_not_of(" \t", i);
    if (start == std::string::npos) return false;
    try {
        meshIndex = std::stoi(line.substr(start));
    } catch (...) {
        return false;
    }
    return true;
}

int GetIndent(const std::string& line) {
    int n = 0;
    for (char c : line) {
        if (c == ' ') n++;
        else if (c == '\t') n++;
        else break;
    }
    return n;
}

// Recursive parse for hierarchy: read one entity into a PrefabNode and advance idx past it and its descendants.
void ParseEntityNode(const std::vector<std::string>& lines, size_t& idx, int baseIndent, PrefabNode& out) {
    if (idx >= lines.size()) return;
    out = PrefabNode{};
    if (lines[idx].find("- name:") != std::string::npos)
        ParseName(lines[idx], out.name);
    idx++;
    for (int i = 0; i < 16; ++i) out.localTransform[i] = (i % 5 == 0) ? 1.f : 0.f;
    while (idx < lines.size()) {
        int ind = GetIndent(lines[idx]);
        if (ind <= baseIndent && lines[idx].find("- name:") != std::string::npos)
            break;
        if (lines[idx].find("transform:") != std::string::npos)
            ParseTransform(lines[idx], out.localTransform);
        else if (lines[idx].find("mesh:") != std::string::npos)
            ParseMesh(lines[idx], out.meshPath);
        else if (lines[idx].find("meshIndex:") != std::string::npos)
            ParseMeshIndex(lines[idx], out.meshIndex);
        else if (lines[idx].find("children:") != std::string::npos) {
            idx++;
            while (idx < lines.size() && GetIndent(lines[idx]) > baseIndent && lines[idx].find("- name:") != std::string::npos) {
                PrefabNode child;
                ParseEntityNode(lines, idx, GetIndent(lines[idx]), child);
                out.children.push_back(std::move(child));
            }
            continue;
        }
        idx++;
    }
}

// Recursive parse: from line index, read one entity (name, transform, mesh, children) and advance line index.
void ParseEntity(const std::vector<std::string>& lines, size_t& idx, const float parentWorld[16], std::vector<PrefabDrawable>& out) {
    if (idx >= lines.size()) return;
    int baseIndent = GetIndent(lines[idx]);

    std::string name;
    if (lines[idx].find("- name:") != std::string::npos)
        ParseName(lines[idx], name);
    idx++;

    float local[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::string meshPath;

    while (idx < lines.size()) {
        int ind = GetIndent(lines[idx]);
        if (ind <= baseIndent && (lines[idx].find("- name:") != std::string::npos))
            break;
        if (lines[idx].find("transform:") != std::string::npos)
            ParseTransform(lines[idx], local);
        else if (lines[idx].find("mesh:") != std::string::npos)
            ParseMesh(lines[idx], meshPath);
        else if (lines[idx].find("children:") != std::string::npos) {
            idx++;
            float world[16];
            Mat4Mul(parentWorld, local, world);
            while (idx < lines.size() && GetIndent(lines[idx]) > baseIndent && lines[idx].find("- name:") != std::string::npos) {
                ParseEntity(lines, idx, world, out);
            }
            // Keep idx on the next sibling/entity line; do not skip it.
            continue;
        }
        idx++;
    }

    float world[16];
    Mat4Mul(parentWorld, local, world);
    if (!meshPath.empty()) {
        PrefabDrawable d;
        d.meshPath = meshPath;
        for (int i = 0; i < 16; ++i) d.worldTransform.m[i] = world[i];
        out.push_back(d);
    }
}

} // namespace

bool PrefabReader::Read(const std::string& path, std::vector<PrefabDrawable>& outDrawables) {
    std::ifstream file(path);
    if (!file) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    return ReadFromText(ss.str(), outDrawables);
}

bool PrefabReader::ReadFromText(const std::string& text, std::vector<PrefabDrawable>& outDrawables) {
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
        lines.push_back(line);

    outDrawables.clear();
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    size_t idx = 0;
    while (idx < lines.size()) {
        if (lines[idx].find("entities:") != std::string::npos) {
            idx++;
            while (idx < lines.size() && lines[idx].find("- name:") != std::string::npos) {
                ParseEntity(lines, idx, identity, outDrawables);
            }
            break;
        }
        idx++;
    }
    return true;
}

bool PrefabReader::ReadHierarchyFromText(const std::string& text, std::vector<PrefabNode>& outRoots) {
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
        lines.push_back(line);
    outRoots.clear();
    size_t idx = 0;
    while (idx < lines.size()) {
        if (lines[idx].find("entities:") != std::string::npos) {
            idx++;
            while (idx < lines.size() && lines[idx].find("- name:") != std::string::npos) {
                PrefabNode root;
                ParseEntityNode(lines, idx, GetIndent(lines[idx]), root);
                outRoots.push_back(std::move(root));
            }
            return true;
        }
        idx++;
    }
    return false;
}

} // namespace asset
