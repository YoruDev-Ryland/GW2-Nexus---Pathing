#pragma once
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Minimal inline vector / matrix math for projecting GW2 world positions
// onto the ImGui screen canvas.  No external dependencies.
//
// Coordinate system:  GW2 uses a right-handed system with Y up.
// MumbleLink provides:
//   CameraPosition  — eye position in world space
//   CameraFront     — normalised direction the camera is looking
//   CameraTop       — normalised camera up vector
// MumbleIdent provides:
//   FOV             — vertical field of view in radians
// ─────────────────────────────────────────────────────────────────────────────

namespace Math
{

// ── 3D vector ─────────────────────────────────────────────────────────────────
struct Vec3
{
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s)        const { return {x*s,  y*s,  z*s};  }
    float Dot(const Vec3& o)       const { return x*o.x + y*o.y + z*o.z; }

    Vec3 Cross(const Vec3& o) const
    {
        return { y*o.z - z*o.y,
                 z*o.x - x*o.z,
                 x*o.y - y*o.x };
    }

    float LengthSq() const { return x*x + y*y + z*z; }
    float Length()   const { return std::sqrt(LengthSq()); }

    Vec3 Normalised() const
    {
        float l = Length();
        return l > 1e-8f ? Vec3{x/l, y/l, z/l} : Vec3{};
    }
};

// ── 4×4 column-major matrix ───────────────────────────────────────────────────
struct Mat4
{
    // m[col][row]
    float m[4][4] = {};

    static Mat4 Identity()
    {
        Mat4 r{};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
        return r;
    }

    // Matrix–vector multiply (w = 1)
    void Transform(float ix, float iy, float iz, float& ox, float& oy, float& oz, float& ow) const
    {
        ox = m[0][0]*ix + m[1][0]*iy + m[2][0]*iz + m[3][0];
        oy = m[0][1]*ix + m[1][1]*iy + m[2][1]*iz + m[3][1];
        oz = m[0][2]*ix + m[1][2]*iy + m[2][2]*iz + m[3][2];
        ow = m[0][3]*ix + m[1][3]*iy + m[2][3]*iz + m[3][3];
    }

    Mat4 operator*(const Mat4& b) const
    {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                for (int k = 0; k < 4; ++k)
                    r.m[c][row] += m[k][row] * b.m[c][k];
        return r;
    }
};

// ── View matrix from camera position and basis vectors ───────────────────────
// right, up, forward are expected to be orthonormal.
inline Mat4 LookAt(Vec3 eye, Vec3 forward, Vec3 worldUp)
{
    Vec3 f = forward.Normalised();
    Vec3 r = f.Cross(worldUp).Normalised();
    Vec3 u = r.Cross(f);

    Mat4 v{};
    v.m[0][0] =  r.x; v.m[1][0] =  r.y; v.m[2][0] =  r.z;
    v.m[0][1] =  u.x; v.m[1][1] =  u.y; v.m[2][1] =  u.z;
    v.m[0][2] = -f.x; v.m[1][2] = -f.y; v.m[2][2] = -f.z;
    v.m[3][0] = -r.Dot(eye);
    v.m[3][1] = -u.Dot(eye);
    v.m[3][2] =  f.Dot(eye);
    v.m[3][3] =  1.f;
    return v;
}

// ── Perspective projection ────────────────────────────────────────────────────
// fovY: vertical FOV in radians
// aspect: width/height
// near/far: clip planes
inline Mat4 Perspective(float fovY, float aspect, float nearZ, float farZ)
{
    float tanHalfFov = std::tan(fovY * 0.5f);
    Mat4 p{};
    p.m[0][0] = 1.f / (aspect * tanHalfFov);
    p.m[1][1] = 1.f / tanHalfFov;
    p.m[2][2] = -(farZ + nearZ) / (farZ - nearZ);
    p.m[2][3] = -1.f;
    p.m[3][2] = -(2.f * farZ * nearZ) / (farZ - nearZ);
    return p;
}

// ── World-to-screen projection ────────────────────────────────────────────────
// Returns false if the point is behind the camera or outside the frustum.
// screenX/Y are in ImGui pixel coordinates (top-left origin).
inline bool WorldToScreen(
    const Vec3& worldPos,
    const Mat4& viewProj,
    float screenW, float screenH,
    float& screenX, float& screenY,
    float& depth)
{
    float cx, cy, cz, cw;
    viewProj.Transform(worldPos.x, worldPos.y, worldPos.z, cx, cy, cz, cw);

    if (cw <= 0.f) return false; // behind camera

    float ndcX =  cx / cw;
    float ndcY =  cy / cw;
    // Simple frustum cull — discard if outside [-1.1, 1.1] to allow off-screen
    // labels to still draw partially when near the edge.
    if (ndcX < -1.1f || ndcX > 1.1f || ndcY < -1.1f || ndcY > 1.1f)
        return false;

    screenX = ( ndcX + 1.f) * 0.5f * screenW;
    screenY = (-ndcY + 1.f) * 0.5f * screenH;  // flip Y for screen coords
    depth   = cz / cw;
    return true;
}

// ── Distance squared between two world positions ─────────────────────────────
inline float DistSq(const Vec3& a, const Vec3& b)
{
    float dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
    return dx*dx + dy*dy + dz*dz;
}

// ── Linear remap ─────────────────────────────────────────────────────────────
inline float Remap(float v, float lo, float hi, float outLo, float outHi)
{
    if (hi <= lo) return outLo;
    return outLo + (outHi - outLo) * std::clamp((v - lo) / (hi - lo), 0.f, 1.f);
}

} // namespace Math
