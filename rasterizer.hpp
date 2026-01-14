#pragma once

#include <cstdint>
#include <vector>

#include "math.hpp"
#include "mdl_model.hpp"

enum class BackgroundPreset : uint32_t
{
    Blue = 0,
    Green = 1,
    Transparent = 2,
};

struct RenderOptions
{
    int width{256};
    int height{256};
    BackgroundPreset background{BackgroundPreset::Blue};
};

struct RenderStats
{
    size_t triangles{};
    size_t degenerateTriangles{};
    size_t pixelsWritten{};
};

bool RenderThumbnailRgba(const StudioModelCpu& model, const RenderOptions& options, std::vector<uint8_t>& outRgba, RenderStats* stats = nullptr);
