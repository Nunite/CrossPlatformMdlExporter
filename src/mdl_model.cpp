#include "CrossPlatformMdlExporter/mdl_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <unordered_map>

#include "CrossPlatformMdlExporter/mdl_types.hpp"

namespace
{
std::vector<uint8_t> ReadAllBytes(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return {};

    file.seekg(0, std::ios::end);
    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));
    if (!file)
        return {};
    return buffer;
}

bool VerifyStudioFile(const std::vector<uint8_t>& buffer)
{
    if (buffer.size() < sizeof(StudioHdr))
        return false;

    const auto* header = reinterpret_cast<const StudioHdr*>(buffer.data());
    if (header->id != StudioId_IDST)
        return false;
    if (header->version != StudioVersion)
        return false;
    return true;
}

bool VerifySequenceStudioFile(const std::vector<uint8_t>& buffer)
{
    if (buffer.size() < sizeof(StudioSeqHdr))
        return false;
    const auto* header = reinterpret_cast<const StudioSeqHdr*>(buffer.data());
    if (header->id != StudioId_IDSQ)
        return false;
    if (header->version != StudioVersion)
        return false;
    return true;
}

std::filesystem::path AddSuffixToFileName(const std::filesystem::path& filePath, const std::string& suffix)
{
    const auto stem = filePath.stem().string();
    const auto ext = filePath.extension().string();
    const auto newName = stem + suffix + ext;
    return filePath.parent_path() / newName;
}

template <typename T>
const T* PtrAt(const uint8_t* base, size_t size, int32_t offset)
{
    if (offset < 0)
        return nullptr;
    const size_t off = static_cast<size_t>(offset);
    if (off + sizeof(T) > size)
        return nullptr;
    return reinterpret_cast<const T*>(base + off);
}

template <typename T>
const T* PtrAtUnchecked(const uint8_t* base, int32_t offset)
{
    return reinterpret_cast<const T*>(base + offset);
}

uint32_t InsertVertex(std::vector<Vertex>& vertices, const Vertex& vertex)
{
    auto it = std::find(vertices.begin(), vertices.end(), vertex);
    if (it != vertices.end())
        return static_cast<uint32_t>(it - vertices.begin());
    vertices.push_back(vertex);
    return static_cast<uint32_t>(vertices.size() - 1);
}

struct Quat4f
{
    float x{};
    float y{};
    float z{};
    float w{};
};

Quat4f AngleQuaternion(const Vec3f& anglesRadians)
{
    const float halfZ = anglesRadians.z * 0.5f;
    const float halfY = anglesRadians.y * 0.5f;
    const float halfX = anglesRadians.x * 0.5f;

    const float sy = std::sin(halfZ);
    const float cy = std::cos(halfZ);
    const float sp = std::sin(halfY);
    const float cp = std::cos(halfY);
    const float sr = std::sin(halfX);
    const float cr = std::cos(halfX);

    Quat4f q{};
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    q.w = cr * cp * cy + sr * sp * sy;
    return q;
}

void QuaternionMatrix(const Quat4f& q, float out3x4[3][4])
{
    out3x4[0][0] = 1.0f - 2.0f * q.y * q.y - 2.0f * q.z * q.z;
    out3x4[1][0] = 2.0f * q.x * q.y + 2.0f * q.w * q.z;
    out3x4[2][0] = 2.0f * q.x * q.z - 2.0f * q.w * q.y;

    out3x4[0][1] = 2.0f * q.x * q.y - 2.0f * q.w * q.z;
    out3x4[1][1] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z;
    out3x4[2][1] = 2.0f * q.y * q.z + 2.0f * q.w * q.x;

    out3x4[0][2] = 2.0f * q.x * q.z + 2.0f * q.w * q.y;
    out3x4[1][2] = 2.0f * q.y * q.z - 2.0f * q.w * q.x;
    out3x4[2][2] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y;

    out3x4[0][3] = 0.0f;
    out3x4[1][3] = 0.0f;
    out3x4[2][3] = 0.0f;
}

void ConcatTransforms(const float a[3][4], const float b[3][4], float out[3][4])
{
    out[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0];
    out[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1];
    out[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2];
    out[0][3] = a[0][0] * b[0][3] + a[0][1] * b[1][3] + a[0][2] * b[2][3] + a[0][3];

    out[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0];
    out[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1];
    out[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2];
    out[1][3] = a[1][0] * b[0][3] + a[1][1] * b[1][3] + a[1][2] * b[2][3] + a[1][3];

    out[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0];
    out[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1];
    out[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2];
    out[2][3] = a[2][0] * b[0][3] + a[2][1] * b[1][3] + a[2][2] * b[2][3] + a[2][3];
}

Vec3f TransformPoint3x4(const float m[3][4], const Vec3f& p)
{
    return {
        p.x * m[0][0] + p.y * m[0][1] + p.z * m[0][2] + m[0][3],
        p.x * m[1][0] + p.y * m[1][1] + p.z * m[1][2] + m[1][3],
        p.x * m[2][0] + p.y * m[2][1] + p.z * m[2][2] + m[2][3],
    };
}

Vec3f TransformDirection3x4(const float m[3][4], const Vec3f& d)
{
    return {
        d.x * m[0][0] + d.y * m[0][1] + d.z * m[0][2],
        d.x * m[1][0] + d.y * m[1][1] + d.z * m[1][2],
        d.x * m[2][0] + d.y * m[2][1] + d.z * m[2][2],
    };
}

std::vector<std::array<float, 12>> ComputeDefaultBoneTransforms(const StudioHdr& header, const uint8_t* base)
{
    std::vector<std::array<float, 12>> out;
    if (header.numbones <= 0)
        return out;

    out.resize(static_cast<size_t>(header.numbones));

    const auto* bones = PtrAtUnchecked<MStudioBone>(base, header.boneindex);

    std::vector<std::array<float, 12>> local;
    local.resize(static_cast<size_t>(header.numbones));

    for (int i = 0; i < header.numbones; i++)
    {
        float m[3][4]{};
        const Vec3f angles{bones[i].value[3], bones[i].value[4], bones[i].value[5]};
        const Quat4f q = AngleQuaternion(angles);
        QuaternionMatrix(q, m);
        m[0][3] = bones[i].value[0];
        m[1][3] = bones[i].value[1];
        m[2][3] = bones[i].value[2];

        std::array<float, 12> packed{};
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 4; c++)
                packed[static_cast<size_t>(r * 4 + c)] = m[r][c];
        }
        local[static_cast<size_t>(i)] = packed;
    }

    std::vector<uint8_t> built(static_cast<size_t>(header.numbones), 0);

    auto build = [&](auto&& self, int boneIndex) -> void {
        if (boneIndex < 0 || boneIndex >= header.numbones)
            return;
        if (built[static_cast<size_t>(boneIndex)] != 0)
            return;

        const int parent = bones[boneIndex].parent;
        if (parent < 0)
        {
            out[static_cast<size_t>(boneIndex)] = local[static_cast<size_t>(boneIndex)];
            built[static_cast<size_t>(boneIndex)] = 1;
            return;
        }

        self(self, parent);

        float parentM[3][4]{};
        float localM[3][4]{};
        float worldM[3][4]{};

        const auto& pPacked = out[static_cast<size_t>(parent)];
        const auto& lPacked = local[static_cast<size_t>(boneIndex)];
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                parentM[r][c] = pPacked[static_cast<size_t>(r * 4 + c)];
                localM[r][c] = lPacked[static_cast<size_t>(r * 4 + c)];
            }
        }

        ConcatTransforms(parentM, localM, worldM);

        std::array<float, 12> packed{};
        for (int r = 0; r < 3; r++)
        {
            for (int c = 0; c < 4; c++)
                packed[static_cast<size_t>(r * 4 + c)] = worldM[r][c];
        }
        out[static_cast<size_t>(boneIndex)] = packed;
        built[static_cast<size_t>(boneIndex)] = 1;
    };

    for (int i = 0; i < header.numbones; i++)
        build(build, i);

    return out;
}

TextureRgba LoadTexture(const StudioHdr& textureHeader, const uint8_t* textureBase, const MStudioTexture& tex)
{
    TextureRgba out{};
    out.width = tex.width;
    out.height = tex.height;

    const int size = tex.width * tex.height;
    const auto* indices = PtrAtUnchecked<uint8_t>(textureBase, tex.index);
    const auto* palette = indices + size;

    out.rgba.resize(static_cast<size_t>(size) * 4);
    auto* pixels = out.rgba.data();
    for (int i = 0; i < size; i++)
    {
        const int colorOffset = indices[i] * 3;
        const int pixelOffset = i * 4;
        pixels[pixelOffset + 0] = palette[colorOffset + 0];
        pixels[pixelOffset + 1] = palette[colorOffset + 1];
        pixels[pixelOffset + 2] = palette[colorOffset + 2];
        pixels[pixelOffset + 3] = 0xff;

        if ((tex.flags & STUDIO_NF_MASKED) != 0)
        {
            if (indices[i] == 255)
            {
                pixels[pixelOffset + 0] = 0;
                pixels[pixelOffset + 1] = 0;
                pixels[pixelOffset + 2] = 0;
                pixels[pixelOffset + 3] = 0;
            }
        }
    }

    (void)textureHeader;
    return out;
}

Mesh LoadMesh(const StudioHdr& header,
              const StudioHdr& textureHeader,
              const uint8_t* base,
              const uint8_t* textureBase,
              const MStudioMesh& mesh,
              const MStudioModel& model,
              const std::vector<std::array<float, 12>>& boneTransforms,
              std::vector<Vertex>& vertices)
{
    Mesh out{};
    out.indices.reserve(2048);

    const auto* studioVertices = PtrAtUnchecked<MdlVec3>(base, model.vertindex);
    const auto* studioVertexBones = PtrAtUnchecked<uint8_t>(base, model.vertinfoindex);
    const auto* studioNormals = PtrAtUnchecked<MdlVec3>(base, model.normindex);

    const auto* textures = PtrAtUnchecked<MStudioTexture>(textureBase, textureHeader.textureindex);
    const auto* skinRef = PtrAtUnchecked<uint16_t>(textureBase, textureHeader.skinindex);

    out.textureId = skinRef[mesh.skinref];

    const float s = 1.0f / static_cast<float>(textures[out.textureId].width);
    const float t = 1.0f / static_cast<float>(textures[out.textureId].height);

    std::vector<uint32_t> tempIndices;
    tempIndices.reserve(2048);

    const int16_t* tricmds = PtrAtUnchecked<int16_t>(base, mesh.triindex);
    int16_t i = 0;

    while ((i = *(tricmds++)) != 0)
    {
        bool strip = true;
        if (i < 0)
        {
            i = static_cast<int16_t>(-i);
            strip = false;
        }

        tempIndices.clear();

        for (; i > 0; i--, tricmds += 4)
        {
            Vertex v{};
            const int16_t vertIndex = tricmds[0];
            const int16_t normIndex = tricmds[1];

            const MdlVec3& p = studioVertices[vertIndex];
            const MdlVec3& n = studioNormals[normIndex];

            v.position = {p.x, p.y, p.z};
            v.normal = Normalize({n.x, n.y, n.z});
            v.texCoord = {s * static_cast<float>(tricmds[2]), t * static_cast<float>(tricmds[3])};
            v.bone = studioVertexBones[vertIndex];

            if (!boneTransforms.empty() && v.bone < boneTransforms.size())
            {
                float bm[3][4]{};
                const auto& packed = boneTransforms[static_cast<size_t>(v.bone)];
                for (int r = 0; r < 3; r++)
                {
                    for (int c = 0; c < 4; c++)
                        bm[r][c] = packed[static_cast<size_t>(r * 4 + c)];
                }

                v.position = TransformPoint3x4(bm, v.position);
                v.normal = Normalize(TransformDirection3x4(bm, v.normal));
            }

            tempIndices.push_back(InsertVertex(vertices, v));
        }

        if (strip)
        {
            for (size_t j = 2; j < tempIndices.size(); j++)
            {
                if (j % 2)
                {
                    out.indices.push_back(tempIndices[j - 1]);
                    out.indices.push_back(tempIndices[j - 2]);
                    out.indices.push_back(tempIndices[j]);
                }
                else
                {
                    out.indices.push_back(tempIndices[j - 2]);
                    out.indices.push_back(tempIndices[j - 1]);
                    out.indices.push_back(tempIndices[j]);
                }
            }
        }
        else
        {
            for (size_t j = 2; j < tempIndices.size(); j++)
            {
                out.indices.push_back(tempIndices[0]);
                out.indices.push_back(tempIndices[j - 1]);
                out.indices.push_back(tempIndices[j]);
            }
        }
    }

    (void)header;
    return out;
}

Model LoadModel(const StudioHdr& header,
                const StudioHdr& textureHeader,
                const uint8_t* base,
                const uint8_t* textureBase,
                const MStudioModel& model,
                const std::vector<std::array<float, 12>>& boneTransforms)
{
    Model out{};
    if (model.nummesh <= 0)
        return out;

    out.meshes.reserve(static_cast<size_t>(model.nummesh));
    for (int i = 0; i < model.nummesh; i++)
    {
        const auto* mesh = PtrAtUnchecked<MStudioMesh>(base, model.meshindex) + i;
        out.meshes.push_back(LoadMesh(header, textureHeader, base, textureBase, *mesh, model, boneTransforms, out.vertices));
    }

    return out;
}

BodyPart LoadBodyPart(const StudioHdr& header,
                      const StudioHdr& textureHeader,
                      const uint8_t* base,
                      const uint8_t* textureBase,
                      const MStudioBodyParts& bodyPart,
                      const std::vector<std::array<float, 12>>& boneTransforms)
{
    BodyPart out{};
    if (bodyPart.nummodels <= 0)
        return out;

    out.models.reserve(static_cast<size_t>(bodyPart.nummodels));
    for (int i = 0; i < bodyPart.nummodels; i++)
    {
        const auto* model = PtrAtUnchecked<MStudioModel>(base, bodyPart.modelindex) + i;
        out.models.push_back(LoadModel(header, textureHeader, base, textureBase, *model, boneTransforms));
    }
    return out;
}
} // namespace

bool StudioModelCpu::LoadFromFile(const std::filesystem::path& filePath)
{
    filePath_ = filePath;
    fileData_ = ReadAllBytes(filePath);
    if (fileData_.empty())
        return false;
    if (!VerifyStudioFile(fileData_))
        return false;

    base_ = fileData_.data();
    const auto* header = reinterpret_cast<const StudioHdr*>(base_);

    textureBase_ = base_;
    const StudioHdr* textureHeader = header;

    if (header->numtextures == 0)
    {
        const auto texPath = AddSuffixToFileName(filePath, "T");
        textureFileData_ = ReadAllBytes(texPath);
        if (VerifyStudioFile(textureFileData_))
        {
            textureBase_ = textureFileData_.data();
            textureHeader = reinterpret_cast<const StudioHdr*>(textureBase_);
        }
    }

    boundsMin_ = {header->bbmin.x, header->bbmin.y, header->bbmin.z};
    boundsMax_ = {header->bbmax.x, header->bbmax.y, header->bbmax.z};

    if (header->numseq > 0)
    {
        const auto* seqDescs = PtrAt<MStudioSeqDesc>(base_, fileData_.size(), header->seqindex);
        if (seqDescs)
        {
            boundsMin_ = {seqDescs[0].bbmin[0], seqDescs[0].bbmin[1], seqDescs[0].bbmin[2]};
            boundsMax_ = {seqDescs[0].bbmax[0], seqDescs[0].bbmax[1], seqDescs[0].bbmax[2]};
        }
    }

    const auto defaultBoneTransforms = ComputeDefaultBoneTransforms(*header, base_);

    textures_.clear();
    bodyParts_.clear();

    if (textureHeader->numtextures > 0)
    {
        textures_.reserve(static_cast<size_t>(textureHeader->numtextures));
        const auto* studioTextures = PtrAtUnchecked<MStudioTexture>(textureBase_, textureHeader->textureindex);
        for (int i = 0; i < textureHeader->numtextures; i++)
        {
            textures_.push_back(LoadTexture(*textureHeader, textureBase_, studioTextures[i]));
        }
    }

    if (header->numbodyparts > 0)
    {
        bodyParts_.reserve(static_cast<size_t>(header->numbodyparts));
        const auto* studioBodyParts = PtrAtUnchecked<MStudioBodyParts>(base_, header->bodypartindex);
        for (int i = 0; i < header->numbodyparts; i++)
        {
            bodyParts_.push_back(LoadBodyPart(*header, *textureHeader, base_, textureBase_, studioBodyParts[i], defaultBoneTransforms));
        }
    }

    if (header->numseqgroups > 1)
    {
        for (int i = 1; i < header->numseqgroups; i++)
        {
            char suffix[8]{};
            const int written = std::snprintf(suffix, sizeof(suffix), "%02d", i);
            if (written <= 0)
                continue;
            const auto seqPath = AddSuffixToFileName(filePath, suffix);
            auto buf = ReadAllBytes(seqPath);
            if (!VerifySequenceStudioFile(buf))
                continue;
        }
    }

    return !bodyParts_.empty();
}
