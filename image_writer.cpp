#include "image_writer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#endif

namespace
{
std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

#ifdef _WIN32
bool WritePngWic(const std::filesystem::path& filePath, int width, int height, const std::vector<uint8_t>& rgba)
{
    if (width <= 0 || height <= 0)
        return false;
    if (rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4)
        return false;

    std::vector<uint8_t> bgra(rgba.size());
    for (size_t i = 0; i + 3 < rgba.size(); i += 4)
    {
        bgra[i + 0] = rgba[i + 2];
        bgra[i + 1] = rgba[i + 1];
        bgra[i + 2] = rgba[i + 0];
        bgra[i + 3] = rgba[i + 3];
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr))
        return false;

    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
    if (FAILED(hr))
        return false;

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(stream.GetAddressOf());
    if (FAILED(hr))
        return false;

    hr = stream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr))
        return false;

    hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return false;

    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(frame.GetAddressOf(), props.GetAddressOf());
    if (FAILED(hr))
        return false;

    hr = frame->Initialize(props.Get());
    if (FAILED(hr))
        return false;

    hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    if (FAILED(hr))
        return false;

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&format);
    if (FAILED(hr))
        return false;

    const UINT stride = static_cast<UINT>(width) * 4;
    const UINT imageSize = stride * static_cast<UINT>(height);

    hr = frame->WritePixels(static_cast<UINT>(height), stride, imageSize, const_cast<BYTE*>(bgra.data()));
    if (FAILED(hr))
        return false;

    hr = frame->Commit();
    if (FAILED(hr))
        return false;

    hr = encoder->Commit();
    if (FAILED(hr))
        return false;

    return true;
}
#endif
} // namespace

bool WriteTgaRgba(const std::filesystem::path& filePath, int width, int height, const std::vector<uint8_t>& rgba)
{
    if (width <= 0 || height <= 0)
        return false;
    if (rgba.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4)
        return false;

    std::ofstream out(filePath, std::ios::binary);
    if (!out)
        return false;

    std::array<uint8_t, 18> header{};
    header[2] = 2;
    header[12] = static_cast<uint8_t>(width & 0xff);
    header[13] = static_cast<uint8_t>((width >> 8) & 0xff);
    header[14] = static_cast<uint8_t>(height & 0xff);
    header[15] = static_cast<uint8_t>((height >> 8) & 0xff);
    header[16] = 32;
    header[17] = 0x20 | 8;

    out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!out)
        return false;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            const uint8_t r = rgba[idx + 0];
            const uint8_t g = rgba[idx + 1];
            const uint8_t b = rgba[idx + 2];
            const uint8_t a = rgba[idx + 3];
            const uint8_t bgra[4] = {b, g, r, a};
            out.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }

    return static_cast<bool>(out);
}

bool WriteImageAuto(const std::filesystem::path& filePath, int width, int height, const std::vector<uint8_t>& rgba)
{
    const auto ext = ToLower(filePath.extension().string());
    if (ext == ".tga")
        return WriteTgaRgba(filePath, width, height, rgba);

#ifdef _WIN32
    if (ext == ".png")
        return WritePngWic(filePath, width, height, rgba);
#else
    if (ext == ".png")
        return false;
#endif

    return WriteTgaRgba(filePath, width, height, rgba);
}
