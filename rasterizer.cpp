#include "rasterizer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>

namespace
{
struct VertexOut
{
    float x{};
    float y{};
    float z{};
    float invW{};
    Vec2f uvOverW{};
    Vec3f normalOverW{};
};

Vec4f ToVec4(const Vec3f& v, float w) { return {v.x, v.y, v.z, w}; }

std::array<uint8_t, 4> GetBackground(BackgroundPreset preset)
{
    switch (preset)
    {
        case BackgroundPreset::Green:
            return {0, 255, 0, 255};
        case BackgroundPreset::Transparent:
            return {0, 0, 0, 0};
        case BackgroundPreset::Blue:
        default:
            return {51, 102, 204, 255};
    }
}

Vec3f ComputeCenter(const Vec3f& a, const Vec3f& b) { return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f}; }

float MaxComponentAbs(const Vec3f& v) { return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)}); }

bool TryComputeVertexBounds(const StudioModelCpu& model, Vec3f& outMin, Vec3f& outMax)
{
    bool any = false;
    Vec3f minP{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    Vec3f maxP{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};

    for (const auto& bp : model.GetBodyParts())
    {
        for (const auto& m : bp.models)
        {
            for (const auto& v : m.vertices)
            {
                any = true;
                minP.x = std::min(minP.x, v.position.x);
                minP.y = std::min(minP.y, v.position.y);
                minP.z = std::min(minP.z, v.position.z);
                maxP.x = std::max(maxP.x, v.position.x);
                maxP.y = std::max(maxP.y, v.position.y);
                maxP.z = std::max(maxP.z, v.position.z);
            }
        }
    }

    if (!any)
        return false;
    outMin = minP;
    outMax = maxP;
    return true;
}

std::array<uint8_t, 4> SampleTexture(const TextureRgba* texture, float u, float v)
{
    if (!texture || texture->width <= 0 || texture->height <= 0 || texture->rgba.empty())
        return {200, 200, 200, 255};

    u = u - std::floor(u);
    v = v - std::floor(v);

    auto wrap = [](int i, int size) -> int {
        if (size <= 0)
            return 0;
        i %= size;
        if (i < 0)
            i += size;
        return i;
    };

    const float x = u * static_cast<float>(texture->width) - 0.5f;
    const float y = v * static_cast<float>(texture->height) - 0.5f;

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    auto fetch = [&](int xi, int yi, int c) -> float {
        const int sx = wrap(xi, texture->width);
        const int sy = wrap(yi, texture->height);
        const size_t idx = (static_cast<size_t>(sy) * static_cast<size_t>(texture->width) + static_cast<size_t>(sx)) * 4 + static_cast<size_t>(c);
        return static_cast<float>(texture->rgba[idx]);
    };

    std::array<uint8_t, 4> out{};
    for (int c = 0; c < 4; c++)
    {
        const float c00 = fetch(x0, y0, c);
        const float c10 = fetch(x1, y0, c);
        const float c01 = fetch(x0, y1, c);
        const float c11 = fetch(x1, y1, c);

        const float cx0 = c00 + (c10 - c00) * tx;
        const float cx1 = c01 + (c11 - c01) * tx;
        const float cf = cx0 + (cx1 - cx0) * ty;
        out[c] = ClampU8(static_cast<int>(std::lround(cf)));
    }
    return out;
}

void BlendOver(uint8_t* dst, const std::array<uint8_t, 4>& src)
{
    const float srcA = static_cast<float>(src[3]) / 255.0f;
    const float dstA = static_cast<float>(dst[3]) / 255.0f;
    const float outA = srcA + dstA * (1.0f - srcA);

    auto blendChannel = [&](int c) -> uint8_t {
        const float s = static_cast<float>(src[c]) / 255.0f;
        const float d = static_cast<float>(dst[c]) / 255.0f;
        float out = 0.0f;
        if (outA > 0.0f)
            out = (s * srcA + d * dstA * (1.0f - srcA)) / outA;
        return ClampU8(static_cast<int>(std::lround(out * 255.0f)));
    };

    dst[0] = blendChannel(0);
    dst[1] = blendChannel(1);
    dst[2] = blendChannel(2);
    dst[3] = ClampU8(static_cast<int>(std::lround(outA * 255.0f)));
}

float EdgeFunction(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}
} // namespace

bool RenderThumbnailRgba(const StudioModelCpu& model, const RenderOptions& options, std::vector<uint8_t>& outRgba, RenderStats* stats)
{
    const int width = std::max(1, options.width);
    const int height = std::max(1, options.height);

    outRgba.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
    if (stats)
        *stats = {};

    const auto bg = GetBackground(options.background);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            outRgba[idx + 0] = bg[0];
            outRgba[idx + 1] = bg[1];
            outRgba[idx + 2] = bg[2];
            outRgba[idx + 3] = bg[3];
        }
    }

    std::vector<float> depth(static_cast<size_t>(width) * static_cast<size_t>(height), 1.0f);

    Vec3f boundsMin = model.GetBoundsMin();
    Vec3f boundsMax = model.GetBoundsMax();
    const Vec3f headerExtents = boundsMax - boundsMin;
    if (MaxComponentAbs(headerExtents) < 0.001f)
        (void)TryComputeVertexBounds(model, boundsMin, boundsMax);
    const Vec3f center = ComputeCenter(boundsMin, boundsMax);
    const float widthMeters = boundsMax.x - boundsMin.x;
    float heightMeters = boundsMax.z - boundsMin.z;
    if (widthMeters > heightMeters)
        heightMeters = widthMeters;

    auto guessGunModel = [&]() -> bool {
        const std::string stem = model.GetFilePath().stem().string();
        std::string fileName = stem;
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return fileName.rfind("v_", 0) == 0 || fileName.rfind("pv-", 0) == 0;
    };

    auto scaling = [](float sx, float sy, float sz) -> Mat4f {
        Mat4f r{};
        r.m = {sx, 0, 0, 0,
               0, sy, 0, 0,
               0, 0, sz, 0,
               0, 0, 0, 1};
        return r;
    };

    Mat4f world = scaling(-1.0f, 1.0f, 1.0f);

    Vec3f eye{-50.0f, 0.0f, 0.0f};
    Vec3f at{center.x, center.y, center.z};
    const Vec3f up{0.0f, 0.0f, 1.0f};

    float fovDeg = 65.0f;
    float cameraDistance = (heightMeters * 0.5f) / std::tan(fovDeg * 0.5f) * 4.0f;
    if (cameraDistance < 0.0f)
        cameraDistance = 50.0f;

    if (guessGunModel())
    {
        eye = {-1.0f, 1.4f, 1.0f};
        at = {-5.0f, 1.4f, 1.0f};
        fovDeg = 90.0f;
    }
    else
    {
        eye = {-cameraDistance, 0.0f, cameraDistance * 0.5f};
    }

    const Mat4f view = LookAtLH(eye, at, up);
    const float fovRad = fovDeg * (3.14159265f / 180.0f);
    const Mat4f projection = Perspective(fovRad, static_cast<float>(width) / static_cast<float>(height), 0.01f, 1000.0f);
    const Mat4f mvp = Mul(projection, Mul(view, world));

    const auto& textures = model.GetTextures();

    for (const auto& bodyPart : model.GetBodyParts())
    {
        if (bodyPart.models.empty())
            continue;
        const Model& m = bodyPart.models[0];

        for (const auto& mesh : m.meshes)
        {
            const TextureRgba* tex = nullptr;
            if (mesh.textureId >= 0 && mesh.textureId < static_cast<int>(textures.size()))
                tex = &textures[static_cast<size_t>(mesh.textureId)];

            for (size_t idx = 0; idx + 2 < mesh.indices.size(); idx += 3)
            {
                if (stats)
                    stats->triangles++;

                const Vertex& v0 = m.vertices[mesh.indices[idx + 0]];
                const Vertex& v1 = m.vertices[mesh.indices[idx + 1]];
                const Vertex& v2 = m.vertices[mesh.indices[idx + 2]];

                auto project = [&](const Vertex& v) -> VertexOut {
                    const Vec4f clip = Mul(mvp, ToVec4(v.position, 1.0f));

                    VertexOut o{};
                    if (clip.w == 0.0f)
                        return o;

                    o.invW = 1.0f / clip.w;
                    const float ndcX = clip.x * o.invW;
                    const float ndcY = clip.y * o.invW;
                    const float ndcZ = clip.z * o.invW;

                    o.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width - 1);
                    o.y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height - 1);
                    o.z = ndcZ;
                    return o;
                };

                VertexOut o0 = project(v0);
                VertexOut o1 = project(v1);
                VertexOut o2 = project(v2);

                o0.uvOverW = {v0.texCoord.x * o0.invW, v0.texCoord.y * o0.invW};
                o1.uvOverW = {v1.texCoord.x * o1.invW, v1.texCoord.y * o1.invW};
                o2.uvOverW = {v2.texCoord.x * o2.invW, v2.texCoord.y * o2.invW};

                const float area = EdgeFunction(o0.x, o0.y, o1.x, o1.y, o2.x, o2.y);
                if (area == 0.0f)
                {
                    if (stats)
                        stats->degenerateTriangles++;
                    continue;
                }

                if (area <= 0.0f)
                    continue;

                const int minX = std::clamp(static_cast<int>(std::floor(std::min({o0.x, o1.x, o2.x}))), 0, width - 1);
                const int maxX = std::clamp(static_cast<int>(std::ceil(std::max({o0.x, o1.x, o2.x}))), 0, width - 1);
                const int minY = std::clamp(static_cast<int>(std::floor(std::min({o0.y, o1.y, o2.y}))), 0, height - 1);
                const int maxY = std::clamp(static_cast<int>(std::ceil(std::max({o0.y, o1.y, o2.y}))), 0, height - 1);

                for (int y = minY; y <= maxY; y++)
                {
                    for (int x = minX; x <= maxX; x++)
                    {
                        const float px = static_cast<float>(x) + 0.5f;
                        const float py = static_cast<float>(y) + 0.5f;

                        const float w0 = EdgeFunction(o1.x, o1.y, o2.x, o2.y, px, py);
                        const float w1 = EdgeFunction(o2.x, o2.y, o0.x, o0.y, px, py);
                        const float w2 = EdgeFunction(o0.x, o0.y, o1.x, o1.y, px, py);

                        const bool hasNeg = (w0 < 0.0f) || (w1 < 0.0f) || (w2 < 0.0f);
                        const bool hasPos = (w0 > 0.0f) || (w1 > 0.0f) || (w2 > 0.0f);
                        if (hasNeg && hasPos)
                            continue;

                        const float invArea = 1.0f / area;
                        const float b0 = w0 * invArea;
                        const float b1 = w1 * invArea;
                        const float b2 = w2 * invArea;

                        const float invW = b0 * o0.invW + b1 * o1.invW + b2 * o2.invW;
                        if (invW <= 0.0f)
                            continue;
                        const float w = 1.0f / invW;

                        const float depthZ = b0 * o0.z + b1 * o1.z + b2 * o2.z;
                        const size_t di = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                        if (depthZ < 0.0f || depthZ > 1.0f)
                            continue;
                        if (depthZ >= depth[di])
                            continue;

                        const Vec2f uv = {(b0 * o0.uvOverW.x + b1 * o1.uvOverW.x + b2 * o2.uvOverW.x) * w,
                                          (b0 * o0.uvOverW.y + b1 * o1.uvOverW.y + b2 * o2.uvOverW.y) * w};

                        auto texel = SampleTexture(tex, uv.x, uv.y);
                        if (texel[3] == 0)
                            continue;

                        uint8_t* dst = &outRgba[di * 4];
                        BlendOver(dst, texel);
                        depth[di] = depthZ;
                        if (stats)
                            stats->pixelsWritten++;
                    }
                }
            }
        }
    }

    return true;
}
