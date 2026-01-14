#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>
#endif

#include "CrossPlatformMdlExporter/image_writer.hpp"
#include "CrossPlatformMdlExporter/mdl_model.hpp"
#include "CrossPlatformMdlExporter/rasterizer.hpp"

namespace
{
std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool TryParseInt(const std::string& s, int& out)
{
    try
    {
        size_t pos = 0;
        const int v = std::stoi(s, &pos, 10);
        if (pos != s.size())
            return false;
        out = v;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

BackgroundPreset ParseBackgroundPreset(const std::string& s)
{
    const auto v = ToLower(s);
    if (v == "blue" || v == "b" || v == "0")
        return BackgroundPreset::Blue;
    if (v == "green" || v == "g" || v == "1")
        return BackgroundPreset::Green;
    if (v == "transparent" || v == "t" || v == "2")
        return BackgroundPreset::Transparent;
    return BackgroundPreset::Blue;
}
} // namespace

int main(int argc, char** argv)
{
#ifdef _WIN32
    HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif

    if (argc < 3)
    {
        std::cerr << "Usage: CrossPlatformMdlExporter <input.mdl> <output.(png|tga)> [--width N] [--height N] [--background blue|green|transparent]\n";
#ifdef _WIN32
        if (SUCCEEDED(coInit))
            CoUninitialize();
#endif
        return 2;
    }

    const std::filesystem::path inputPath = std::filesystem::u8path(argv[1]);
    const std::filesystem::path outputPath = std::filesystem::u8path(argv[2]);

    RenderOptions options{};
    bool verbose = false;

    for (int i = 3; i < argc; i++)
    {
        const std::string arg = argv[i];
        if (arg == "--verbose")
        {
            verbose = true;
            continue;
        }
        if (arg == "--width" && i + 1 < argc)
        {
            int v = 0;
            if (TryParseInt(argv[++i], v))
                options.width = v;
            continue;
        }
        if (arg == "--height" && i + 1 < argc)
        {
            int v = 0;
            if (TryParseInt(argv[++i], v))
                options.height = v;
            continue;
        }
        if (arg == "--background" && i + 1 < argc)
        {
            options.background = ParseBackgroundPreset(argv[++i]);
            continue;
        }
    }

    StudioModelCpu model;
    if (!model.LoadFromFile(inputPath))
    {
        std::cerr << "Failed to load mdl: " << inputPath.string() << "\n";
#ifdef _WIN32
        if (SUCCEEDED(coInit))
            CoUninitialize();
#endif
        return 1;
    }

    if (verbose)
    {
        size_t bodyParts = model.GetBodyParts().size();
        size_t models = 0;
        size_t meshes = 0;
        size_t vertices = 0;
        size_t indices = 0;
        Vec3f minP{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
        Vec3f maxP{-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
        for (const auto& bp : model.GetBodyParts())
        {
            models += bp.models.size();
            for (const auto& m : bp.models)
            {
                vertices += m.vertices.size();
                meshes += m.meshes.size();
                for (const auto& v : m.vertices)
                {
                    minP.x = std::min(minP.x, v.position.x);
                    minP.y = std::min(minP.y, v.position.y);
                    minP.z = std::min(minP.z, v.position.z);
                    maxP.x = std::max(maxP.x, v.position.x);
                    maxP.y = std::max(maxP.y, v.position.y);
                    maxP.z = std::max(maxP.z, v.position.z);
                }
                for (const auto& me : m.meshes)
                {
                    indices += me.indices.size();
                    std::cerr << "Mesh texId=" << me.textureId << " indices=" << me.indices.size() << "\n";
                }
            }
        }
        std::cerr << "BodyParts=" << bodyParts << " Models=" << models << " Meshes=" << meshes << " Vertices=" << vertices << " Indices=" << indices << " Textures=" << model.GetTextures().size()
                  << " PosMin=(" << minP.x << "," << minP.y << "," << minP.z << ")"
                  << " PosMax=(" << maxP.x << "," << maxP.y << "," << maxP.z << ")"
                  << " HdrMin=(" << model.GetBoundsMin().x << "," << model.GetBoundsMin().y << "," << model.GetBoundsMin().z << ")"
                  << " HdrMax=(" << model.GetBoundsMax().x << "," << model.GetBoundsMax().y << "," << model.GetBoundsMax().z << ")"
                  << "\n";

        for (size_t ti = 0; ti < model.GetTextures().size(); ti++)
        {
            const auto& tex = model.GetTextures()[ti];
            size_t nonZeroAlpha = 0;
            for (size_t p = 3; p < tex.rgba.size(); p += 4)
            {
                if (tex.rgba[p] != 0)
                    nonZeroAlpha++;
            }
            std::cerr << "Texture[" << ti << "] " << tex.width << "x" << tex.height << " alphaNonZero=" << nonZeroAlpha << "/" << (tex.rgba.size() / 4) << "\n";
        }
    }

    std::vector<uint8_t> rgba;
    RenderStats renderStats{};
    if (!RenderThumbnailRgba(model, options, rgba, verbose ? &renderStats : nullptr))
    {
        std::cerr << "Render failed\n";
#ifdef _WIN32
        if (SUCCEEDED(coInit))
            CoUninitialize();
#endif
        return 1;
    }

    if (verbose)
    {
        std::cerr << "Render triangles=" << renderStats.triangles << " degenerate=" << renderStats.degenerateTriangles << " pixelsWritten=" << renderStats.pixelsWritten << "\n";
    }

    if (!WriteImageAuto(outputPath, options.width, options.height, rgba))
    {
        std::cerr << "Write image failed: " << outputPath.string() << "\n";
#ifdef _WIN32
        if (SUCCEEDED(coInit))
            CoUninitialize();
#endif
        return 1;
    }

#ifdef _WIN32
    if (SUCCEEDED(coInit))
        CoUninitialize();
#endif
    return 0;
}
