#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "math.hpp"

struct TextureRgba
{
    int width{};
    int height{};
    std::vector<uint8_t> rgba;
};

struct Vertex
{
    Vec3f position{};
    Vec3f normal{};
    Vec2f texCoord{};
    uint8_t bone{};

    bool operator==(const Vertex& other) const
    {
        return position.x == other.position.x && position.y == other.position.y && position.z == other.position.z &&
               normal.x == other.normal.x && normal.y == other.normal.y && normal.z == other.normal.z &&
               texCoord.x == other.texCoord.x && texCoord.y == other.texCoord.y && bone == other.bone;
    }
};

struct Mesh
{
    int textureId{-1};
    std::vector<uint32_t> indices;
};

struct Model
{
    std::vector<Vertex> vertices;
    std::vector<Mesh> meshes;
};

struct BodyPart
{
    std::vector<Model> models;
};

class StudioModelCpu
{
public:
    bool LoadFromFile(const std::filesystem::path& filePath);

    const std::filesystem::path& GetFilePath() const { return filePath_; }
    const std::vector<BodyPart>& GetBodyParts() const { return bodyParts_; }
    const std::vector<TextureRgba>& GetTextures() const { return textures_; }

    Vec3f GetBoundsMin() const { return boundsMin_; }
    Vec3f GetBoundsMax() const { return boundsMax_; }

private:
    std::filesystem::path filePath_{};
    std::vector<uint8_t> fileData_{};

    std::vector<uint8_t> textureFileData_{};
    const uint8_t* base_{};
    const uint8_t* textureBase_{};

    std::vector<BodyPart> bodyParts_{};
    std::vector<TextureRgba> textures_{};

    Vec3f boundsMin_{};
    Vec3f boundsMax_{};
};
