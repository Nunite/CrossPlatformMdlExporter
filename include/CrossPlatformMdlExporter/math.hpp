#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

struct Vec2f
{
    float x{};
    float y{};
};

struct Vec3f
{
    float x{};
    float y{};
    float z{};
};

struct Vec4f
{
    float x{};
    float y{};
    float z{};
    float w{};
};

inline Vec3f operator+(const Vec3f& a, const Vec3f& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f operator-(const Vec3f& a, const Vec3f& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f operator*(const Vec3f& v, float s) { return {v.x * s, v.y * s, v.z * s}; }
inline Vec3f operator/(const Vec3f& v, float s) { return {v.x / s, v.y / s, v.z / s}; }

inline float Dot(const Vec3f& a, const Vec3f& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3f Cross(const Vec3f& a, const Vec3f& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float Length(const Vec3f& v) { return std::sqrt(Dot(v, v)); }

inline Vec3f Normalize(const Vec3f& v)
{
    const float len = Length(v);
    if (len <= 0.0f)
        return {0.0f, 0.0f, 0.0f};
    return v / len;
}

struct Mat4f
{
    std::array<float, 16> m{};

    static Mat4f Identity()
    {
        Mat4f r{};
        r.m = {1, 0, 0, 0,
               0, 1, 0, 0,
               0, 0, 1, 0,
               0, 0, 0, 1};
        return r;
    }
};

inline Mat4f Mul(const Mat4f& a, const Mat4f& b)
{
    Mat4f r{};
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
            {
                sum += a.m[row * 4 + k] * b.m[k * 4 + col];
            }
            r.m[row * 4 + col] = sum;
        }
    }
    return r;
}

inline Vec4f Mul(const Mat4f& a, const Vec4f& v)
{
    return {
        a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z + a.m[3] * v.w,
        a.m[4] * v.x + a.m[5] * v.y + a.m[6] * v.z + a.m[7] * v.w,
        a.m[8] * v.x + a.m[9] * v.y + a.m[10] * v.z + a.m[11] * v.w,
        a.m[12] * v.x + a.m[13] * v.y + a.m[14] * v.z + a.m[15] * v.w,
    };
}

inline Mat4f Perspective(float fovYRadians, float aspect, float zNear, float zFar)
{
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4f r{};
    r.m = {f / aspect, 0, 0, 0,
           0, f, 0, 0,
           0, 0, zFar / (zFar - zNear), (-zNear * zFar) / (zFar - zNear),
           0, 0, 1, 0};
    return r;
}

inline Mat4f LookAtLH(const Vec3f& eye, const Vec3f& at, const Vec3f& up)
{
    const Vec3f zaxis = Normalize(at - eye);
    const Vec3f xaxis = Normalize(Cross(up, zaxis));
    const Vec3f yaxis = Cross(zaxis, xaxis);

    Mat4f r = Mat4f::Identity();
    r.m[0] = xaxis.x;
    r.m[1] = xaxis.y;
    r.m[2] = xaxis.z;
    r.m[3] = -Dot(xaxis, eye);

    r.m[4] = yaxis.x;
    r.m[5] = yaxis.y;
    r.m[6] = yaxis.z;
    r.m[7] = -Dot(yaxis, eye);

    r.m[8] = zaxis.x;
    r.m[9] = zaxis.y;
    r.m[10] = zaxis.z;
    r.m[11] = -Dot(zaxis, eye);
    return r;
}

inline uint8_t ClampU8(int v)
{
    return static_cast<uint8_t>(std::clamp(v, 0, 255));
}
