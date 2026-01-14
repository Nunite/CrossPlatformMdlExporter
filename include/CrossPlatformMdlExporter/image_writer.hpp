#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

bool WriteTgaRgba(const std::filesystem::path& filePath, int width, int height, const std::vector<uint8_t>& rgba);
bool WriteImageAuto(const std::filesystem::path& filePath, int width, int height, const std::vector<uint8_t>& rgba);

