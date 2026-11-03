// Engine-agnostic math for the UE3 <-> Source bridge: unit/handedness conversion, portal
// transport in SE(3), wallrun continuation through portals, roll correction, Skill Roll and
// gel modifiers. Header-only, no engine types; every function is unit-tested.
//
// Conventions
//   * Column vectors: p' = M * p. Mat4 translation lives in m[r][3].
//   * Quaternions are (x, y, z, w) and rotate with the standard Hamilton product. The formulas
//     are purely algebraic, so the same code serves UE3 (left-handed) and Source (right-handed)
//     data as long as one call never mixes the two spaces.
//   * Both engines are Z-up with X forward. UE3 Y points right, Source Y points left, so
//     converting between them mirrors Y: S = diag(1, -1, 1).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "aperture_runner/protocol.h"

namespace aperture_runner {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kDegToRad = kPi / 180.0f;

// 1 m = 100 uu = 39.3700787 su  =>  1 uu = 0.393700787 su, 1 su = 2.54 uu.
inline constexpr float kUuPerMeter = 100.0f;
inline constexpr float kSuPerMeter = 39.3700787f;
inline constexpr float kScaleUe3ToSource = 0.393700787f;
inline constexpr float kScaleSourceToUe3 = 2.54f;

// Portal opening half-extents in Source (PORTAL_HALF_WIDTH / PORTAL_HALF_HEIGHT in the
// Source SDK's prop_portal_shared.h). Width runs along the portal's local Y, height along Z.
inline constexpr float kPortalHalfWidthSu = 32.0f;
inline constexpr float kPortalHalfHeightSu = 54.0f;

// ---------------------------------------------------------------------------------------
// Vectors, matrices, quaternions
// ---------------------------------------------------------------------------------------

struct Vec3 {
    float x = 0, y = 0, z = 0;
    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr explicit Vec3(const Float3& f) : x(f.x), y(f.y), z(f.z) {}
    constexpr Float3 ToFloat3() const { return {x, y, z}; }
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
constexpr Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
constexpr Vec3 operator*(float s, Vec3 a) { return a * s; }
constexpr float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 Cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline Vec3 Normalized(Vec3 a, Vec3 fallback = {0, 0, 0}) {
    const float len = Length(a);
    return len > 1e-8f ? a * (1.0f / len) : fallback;
}
inline Vec3 ClampLength(Vec3 v, float max_len) {
    const float len = Length(v);
    return len > max_len && len > 0.0f ? v * (max_len / len) : v;
}
inline float AngleBetweenRad(Vec3 a, Vec3 b) {
    const float d = Dot(Normalized(a), Normalized(b));
    return std::acos(std::clamp(d, -1.0f, 1.0f));
}

struct Mat3 {
    float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    static constexpr Mat3 Identity() { return {}; }
    static constexpr Mat3 Diagonal(float a, float b, float c) {
        Mat3 r;
        r.m[0][0] = a; r.m[1][1] = b; r.m[2][2] = c;
        return r;
    }
    static constexpr Mat3 FromColumns(Vec3 c0, Vec3 c1, Vec3 c2) {
        Mat3 r;
        r.m[0][0] = c0.x; r.m[0][1] = c1.x; r.m[0][2] = c2.x;
        r.m[1][0] = c0.y; r.m[1][1] = c1.y; r.m[1][2] = c2.y;
        r.m[2][0] = c0.z; r.m[2][1] = c1.z; r.m[2][2] = c2.z;
        return r;
    }
    constexpr Vec3 Column(int c) const { return {m[0][c], m[1][c], m[2][c]}; }
    constexpr Mat3 Transposed() const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = m[j][i];
        return r;
    }
};

constexpr Vec3 operator*(const Mat3& a, Vec3 v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}
constexpr Mat3 operator*(const Mat3& a, const Mat3& b) {
    Mat3 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
    return r;
}

struct Mat4 {
    float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    static constexpr Mat4 Identity() { return {}; }
    static constexpr Mat4 FromRotationTranslation(const Mat3& r, Vec3 t) {
        Mat4 o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) o.m[i][j] = r.m[i][j];
        o.m[0][3] = t.x; o.m[1][3] = t.y; o.m[2][3] = t.z;
        return o;
    }
    static Mat4 FromMatrix4(const Matrix4& src) {
        Mat4 o;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) o.m[i][j] = src.m[i][j];
        return o;
    }
    Matrix4 ToMatrix4() const {
        Matrix4 o;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) o.m[i][j] = m[i][j];
        return o;
    }
    constexpr Mat3 Rotation() const {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = m[i][j];
        return r;
    }
    constexpr Vec3 Translation() const { return {m[0][3], m[1][3], m[2][3]}; }
    constexpr Vec3 Column(int c) const { return {m[0][c], m[1][c], m[2][c]}; }
};

constexpr Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] +
                        a.m[i][3] * b.m[3][j];
    return r;
}
constexpr Vec3 TransformPoint(const Mat4& a, Vec3 p) {
    return {a.m[0][0] * p.x + a.m[0][1] * p.y + a.m[0][2] * p.z + a.m[0][3],
            a.m[1][0] * p.x + a.m[1][1] * p.y + a.m[1][2] * p.z + a.m[1][3],
            a.m[2][0] * p.x + a.m[2][1] * p.y + a.m[2][2] * p.z + a.m[2][3]};
}
constexpr Vec3 TransformVector(const Mat4& a, Vec3 v) { return a.Rotation() * v; }

// Inverse of a rigid transform [R | t] (R orthonormal): [R^T | -R^T t].
constexpr Mat4 RigidInverse(const Mat4& a) {
    const Mat3 rt = a.Rotation().Transposed();
    return Mat4::FromRotationTranslation(rt, -(rt * a.Translation()));
}

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
    constexpr Quat() = default;
    constexpr Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr explicit Quat(const Float4& f) : x(f.x), y(f.y), z(f.z), w(f.w) {}
    constexpr Float4 ToFloat4() const { return {x, y, z, w}; }
    static constexpr Quat Identity() { return {}; }
};

constexpr Quat operator*(Quat a, Quat b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
constexpr Quat Conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
inline Quat Normalized(Quat q) {
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return len > 1e-8f ? Quat{q.x / len, q.y / len, q.z / len, q.w / len} : Quat::Identity();
}
inline Quat QuatFromAxisAngle(Vec3 axis, float radians) {
    const Vec3 a = Normalized(axis, {0, 0, 1});
    const float s = std::sin(radians * 0.5f);
    return {a.x * s, a.y * s, a.z * s, std::cos(radians * 0.5f)};
}
constexpr Vec3 Rotate(Quat q, Vec3 v) {
    // v' = v + 2w(u x v) + 2u x (u x v), u = (x, y, z)
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = Cross(u, v) * 2.0f;
    return v + t * q.w + Cross(u, t);
}
inline Mat3 QuatToMat3(Quat q) {
    q = Normalized(q);
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    Mat3 r;
    r.m[0][0] = 1 - 2 * (yy + zz); r.m[0][1] = 2 * (xy - wz);     r.m[0][2] = 2 * (xz + wy);
    r.m[1][0] = 2 * (xy + wz);     r.m[1][1] = 1 - 2 * (xx + zz); r.m[1][2] = 2 * (yz - wx);
    r.m[2][0] = 2 * (xz - wy);     r.m[2][1] = 2 * (yz + wx);     r.m[2][2] = 1 - 2 * (xx + yy);
    return r;
}
// Shepperd's method; input must be a proper rotation.
inline Quat Mat3ToQuat(const Mat3& r) {
    const float trace = r.m[0][0] + r.m[1][1] + r.m[2][2];
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (r.m[2][1] - r.m[1][2]) / s;
        q.y = (r.m[0][2] - r.m[2][0]) / s;
        q.z = (r.m[1][0] - r.m[0][1]) / s;
    } else if (r.m[0][0] > r.m[1][1] && r.m[0][0] > r.m[2][2]) {
        const float s = std::sqrt(1.0f + r.m[0][0] - r.m[1][1] - r.m[2][2]) * 2.0f;
        q.w = (r.m[2][1] - r.m[1][2]) / s;
        q.x = 0.25f * s;
        q.y = (r.m[0][1] + r.m[1][0]) / s;
        q.z = (r.m[0][2] + r.m[2][0]) / s;
    } else if (r.m[1][1] > r.m[2][2]) {
        const float s = std::sqrt(1.0f + r.m[1][1] - r.m[0][0] - r.m[2][2]) * 2.0f;
        q.w = (r.m[0][2] - r.m[2][0]) / s;
        q.x = (r.m[0][1] + r.m[1][0]) / s;
        q.y = 0.25f * s;
        q.z = (r.m[1][2] + r.m[2][1]) / s;
    } else {
        const float s = std::sqrt(1.0f + r.m[2][2] - r.m[0][0] - r.m[1][1]) * 2.0f;
        q.w = (r.m[1][0] - r.m[0][1]) / s;
        q.x = (r.m[0][2] + r.m[2][0]) / s;
        q.y = (r.m[1][2] + r.m[2][1]) / s;
        q.z = 0.25f * s;
    }
    return Normalized(q);
}

// ---------------------------------------------------------------------------------------
// UE3 <-> Source conversion
// ---------------------------------------------------------------------------------------

inline constexpr Mat3 kMirrorY = Mat3::Diagonal(1.0f, -1.0f, 1.0f);

constexpr Vec3 MirrorY(Vec3 v) { return {v.x, -v.y, v.z}; }

// Anchors p_src0 / p_me0: the same physical point expressed in each world.
struct WorldCalibration {
    Vec3 source_origin;  // su
    Vec3 ue3_origin;     // uu

    static WorldCalibration FromData(const WorldCalibrationData& d) {
        return {Vec3(d.source_origin), Vec3(d.ue3_origin)};
    }
    WorldCalibrationData ToData() const {
        WorldCalibrationData d{};
        d.source_origin = source_origin.ToFloat3();
        d.ue3_origin = ue3_origin.ToFloat3();
        return d;
    }
};

constexpr Vec3 Ue3ToSourceVector(Vec3 v_uu) { return MirrorY(v_uu) * kScaleUe3ToSource; }
constexpr Vec3 SourceToUe3Vector(Vec3 v_su) { return MirrorY(v_su) * kScaleSourceToUe3; }
constexpr Vec3 Ue3ToSourcePoint(Vec3 p_uu, const WorldCalibration& c) {
    return c.source_origin + Ue3ToSourceVector(p_uu - c.ue3_origin);
}
constexpr Vec3 SourceToUe3Point(Vec3 p_su, const WorldCalibration& c) {
    return c.ue3_origin + SourceToUe3Vector(p_su - c.source_origin);
}
// Directions (unit normals) only change handedness, never scale.
constexpr Vec3 Ue3ToSourceDirection(Vec3 d) { return MirrorY(d); }
constexpr Vec3 SourceToUe3Direction(Vec3 d) { return MirrorY(d); }

// Conjugating a rotation by the reflection S gives S R S: axis a -> -S a, same angle.
// For q = (x, y, z, w) that is (-x, y, -z, w). The map is its own inverse.
constexpr Quat MirrorYQuat(Quat q) { return {-q.x, q.y, -q.z, q.w}; }
constexpr Mat3 MirrorYRotation(const Mat3& r) { return kMirrorY * r * kMirrorY; }

// Re-expresses a Source rigid transform (su) in UE3 world (uu).
constexpr Mat4 SourceToUe3Transform(const Mat4& m_su, const WorldCalibration& c) {
    return Mat4::FromRotationTranslation(MirrorYRotation(m_su.Rotation()),
                                         SourceToUe3Point(m_su.Translation(), c));
}

// UE3 FRotator (65536 units per turn) to a rotation matrix whose columns are the rotated
// X/Y/Z axes, following UE3's FRotationMatrix.
inline Mat3 Ue3RotatorToMat3(int32_t pitch, int32_t yaw, int32_t roll) {
    constexpr float kUnitToRad = 2.0f * kPi / 65536.0f;
    const float p = static_cast<float>(pitch) * kUnitToRad;
    const float y = static_cast<float>(yaw) * kUnitToRad;
    const float r = static_cast<float>(roll) * kUnitToRad;
    const float sp = std::sin(p), cp = std::cos(p);
    const float sy = std::sin(y), cy = std::cos(y);
    const float sr = std::sin(r), cr = std::cos(r);
    const Vec3 axis_x{cp * cy, cp * sy, sp};
    const Vec3 axis_y{sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp};
    const Vec3 axis_z{-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp};
    return Mat3::FromColumns(axis_x, axis_y, axis_z);
}
inline Quat Ue3RotatorToQuat(int32_t pitch, int32_t yaw, int32_t roll) {
    return Mat3ToQuat(Ue3RotatorToMat3(pitch, yaw, roll));
}

// ---------------------------------------------------------------------------------------
// Portal transport: T_in->out = M_out * R_pi * inv(M_in)
// ---------------------------------------------------------------------------------------

// Half-turn about the portal's local up (Z): flips local forward and left.
inline constexpr Mat4 kPortalRotatePi = [] {
    Mat4 r;
    r.m[0][0] = -1.0f;
    r.m[1][1] = -1.0f;
    return r;
}();

struct PortalTransform {
    Mat4 transform;  // T_in->out, Source space
    Mat3 rotation;   // Omega_in->out in SO(3)
};

constexpr PortalTransform ComputePortalTransform(const Mat4& m_in, const Mat4& m_out) {
    const Mat4 t = m_out * kPortalRotatePi * RigidInverse(m_in);
    return {t, t.Rotation()};
}

// Velocity/normal transport expressed in UE3 space: S * Omega * S * v.
constexpr Vec3 TransportUe3Vector(const PortalTransform& pt, Vec3 v_uu) {
    return MirrorY(pt.rotation * MirrorY(v_uu));
}

struct PortalCrossing {
    bool crossed = false;
    float fraction = 0.0f;  // along prev -> cur where the plane was hit
    float local_y = 0.0f;   // su, across the opening
    float local_z = 0.0f;   // su, up the opening
};

// The portal frame's column 0 is its outward normal. Faith crosses when her centre moves
// from the front half-space (d >= 0) to behind the plane (d < 0) inside the elliptical opening.
inline PortalCrossing DetectPortalCrossing(Vec3 prev_su, Vec3 cur_su, const Mat4& portal,
                                           float half_width = kPortalHalfWidthSu,
                                           float half_height = kPortalHalfHeightSu) {
    PortalCrossing out;
    const Vec3 origin = portal.Translation();
    const Vec3 n = Normalized(portal.Column(0));
    const float d_prev = Dot(n, prev_su - origin);
    const float d_cur = Dot(n, cur_su - origin);
    if (!(d_prev >= 0.0f && d_cur < 0.0f)) return out;
    const float t = d_prev / (d_prev - d_cur);
    const Vec3 hit = prev_su + (cur_su - prev_su) * t;
    const float ly = Dot(Normalized(portal.Column(1)), hit - origin);
    const float lz = Dot(Normalized(portal.Column(2)), hit - origin);
    const float e = (ly * ly) / (half_width * half_width) + (lz * lz) / (half_height * half_height);
    if (e > 1.0f) return out;
    out.crossed = true;
    out.fraction = t;
    out.local_y = ly;
    out.local_z = lz;
    return out;
}

// True if a wall contact at p (uu) lies inside the hole a linked portal cuts in its wall,
// so the guest must ignore it (the "portal seam" fix).
inline bool IsInsidePortalCarve(const PortalCarve& carve, Vec3 p_uu) {
    if (!carve.active) return false;
    const Mat4 m = Mat4::FromMatrix4(carve.ue3_matrix);
    const Vec3 rel = p_uu - m.Translation();
    const float d = Dot(Normalized(m.Column(0)), rel);
    if (d > 1.0f || d < -carve.depth) return false;  // 1 uu tolerance in front of the plane
    const float ly = Dot(Normalized(m.Column(1)), rel);
    const float lz = Dot(Normalized(m.Column(2)), rel);
    return (ly * ly) / (carve.half_width * carve.half_width) +
               (lz * lz) / (carve.half_height * carve.half_height) <=
           1.0f;
}

// ---------------------------------------------------------------------------------------
// Tuning registry
// ---------------------------------------------------------------------------------------

struct ApertureTuning {
    float max_wallrun_duration_sec = 1.45f;       // [0.8, 5.0]
    float max_wallrun_duration_gel_sec = 3.5f;    // with Propulsion Gel, [0.8, 5.0]
    float skill_roll_window_sec = 0.22f;          // [0.10, 0.35]
    float skill_roll_safe_impact_mps = 9.5f;
    float skill_roll_transfer = 0.65f;
    float hard_landing_stagger_sec = 1.4f;
    float propulsion_speed_mult = 2.40f;          // [1.5, 3.2]
    float propulsion_friction_reduction = 0.85f;  // [0, 1]
    float repulsion_bounce_beta = 1.95f;
    float roll_decay_lambda = 8.5f;               // 1/s
    float wallrun_portal_tolerance_deg = 20.0f;
    float walk_speed_mps = 3.5f;
    float sprint_max_speed_mps = 7.78f;
    float sprint_time_to_max_sec = 2.8f;
    float portal_fling_max_speed_mps = 28.5f;
    float fling_event_min_speed_mps = 9.5f;       // exit speed that counts as a "fling" event
    float wallrun_reacquire_distance_uu = 60.0f;  // guest wall trace length after a teleport
    uint32_t teleport_ack_timeout_ticks = 30;
    uint8_t portal_stencil_recursion = 4;         // [2, 8]
    // #E60012 as sRGB-encoded floats; use RunnerVisionLinear() for shader constants.
    float runner_vision_srgb[3] = {230.0f / 255.0f, 0.0f, 18.0f / 255.0f};

    ApertureTuning Sanitized() const {
        ApertureTuning t = *this;
        t.max_wallrun_duration_sec = std::clamp(t.max_wallrun_duration_sec, 0.8f, 5.0f);
        t.max_wallrun_duration_gel_sec = std::clamp(t.max_wallrun_duration_gel_sec, 0.8f, 5.0f);
        t.skill_roll_window_sec = std::clamp(t.skill_roll_window_sec, 0.10f, 0.35f);
        t.propulsion_speed_mult = std::clamp(t.propulsion_speed_mult, 1.5f, 3.2f);
        t.propulsion_friction_reduction = std::clamp(t.propulsion_friction_reduction, 0.0f, 1.0f);
        t.portal_stencil_recursion = std::clamp<uint8_t>(t.portal_stencil_recursion, 2, 8);
        t.wallrun_portal_tolerance_deg = std::clamp(t.wallrun_portal_tolerance_deg, 1.0f, 89.0f);
        return t;
    }
};

inline float SrgbToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
inline Vec3 RunnerVisionLinear(const ApertureTuning& t) {
    return {SrgbToLinear(t.runner_vision_srgb[0]), SrgbToLinear(t.runner_vision_srgb[1]),
            SrgbToLinear(t.runner_vision_srgb[2])};
}

// ---------------------------------------------------------------------------------------
// Wallrun continuation through a portal
// ---------------------------------------------------------------------------------------

inline constexpr Vec3 kWorldUp{0.0f, 0.0f, 1.0f};

// A wall is runnable only if its normal is within the tolerance of horizontal.
inline bool IsWallrunnableNormal(Vec3 n, float tolerance_deg) {
    return std::fabs(Dot(Normalized(n), kWorldUp)) <= std::sin(tolerance_deg * kDegToRad);
}

// The wall the run should continue on after the portal: n+ = S * Omega * S * n-.
// Returns false when n+ is not a runnable (near-vertical) wall, e.g. the exit is a floor portal.
inline bool ExpectedExitWallNormal(const PortalTransform& pt, Vec3 wall_normal_uu,
                                   float tolerance_deg, Vec3* out_normal_uu) {
    const Vec3 n = Normalized(TransportUe3Vector(pt, wall_normal_uu));
    *out_normal_uu = n;
    return Length(n) > 0.5f && IsWallrunnableNormal(n, tolerance_deg);
}

inline bool WallMatchesExpected(Vec3 found_normal, Vec3 expected_normal, float tolerance_deg) {
    return IsWallrunnableNormal(found_normal, tolerance_deg) &&
           AngleBetweenRad(found_normal, expected_normal) <= tolerance_deg * kDegToRad;
}

// ---------------------------------------------------------------------------------------
// Roll correction: phi(t + dt) = phi(t) * exp(-lambda * dt)
// ---------------------------------------------------------------------------------------

// Signed roll of the camera around its forward axis, measured from "up as close to world up
// as the view direction allows". Zero when looking straight up/down (roll is undefined).
inline float CameraRollRad(Quat cam, Vec3 world_up = kWorldUp) {
    const Vec3 f = Normalized(Rotate(cam, {1, 0, 0}));
    const Vec3 u = Normalized(Rotate(cam, {0, 0, 1}));
    const Vec3 ref = world_up - f * Dot(world_up, f);
    if (Length(ref) < 1e-4f) return 0.0f;
    const Vec3 r = Normalized(ref);
    return std::atan2(Dot(Cross(r, u), f), Dot(r, u));
}

inline Quat DecayCameraRoll(Quat cam, float dt, float lambda, Vec3 world_up = kWorldUp) {
    const float roll = CameraRollRad(cam, world_up);
    if (roll == 0.0f) return cam;
    const float target = roll * std::exp(-lambda * dt);
    const Vec3 f = Normalized(Rotate(cam, {1, 0, 0}));
    return Normalized(QuatFromAxisAngle(f, target - roll) * cam);
}

// ---------------------------------------------------------------------------------------
// Skill Roll landing (replaces long-fall boots)
// ---------------------------------------------------------------------------------------

enum class LandingOutcome : uint8_t { Soft, SkillRoll, HardLanding };

struct LandingResult {
    LandingOutcome outcome = LandingOutcome::Soft;
    Vec3 velocity;             // engine units/s, to apply after touchdown
    float stagger_sec = 0.0f;  // > 0 only for HardLanding; damage stays with the game's own rules
    float impact_speed_mps = 0.0f;
};

class SkillRollResolver {
public:
    void OnCrouchPressed(double time_sec) { last_press_sec_ = time_sec; }

    // velocity/look in engine units; units_per_meter = 100 for UE3, 39.37 for Source.
    LandingResult OnLanded(double time_sec, Vec3 velocity, Vec3 look_forward,
                           const ApertureTuning& tuning, float units_per_meter,
                           Vec3 world_up = kWorldUp) {
        LandingResult r;
        const float vz = Dot(velocity, world_up);
        const Vec3 horizontal = velocity - world_up * vz;
        r.impact_speed_mps = std::fabs(std::min(vz, 0.0f)) / units_per_meter;
        r.velocity = horizontal;

        const double since_press = time_sec - last_press_sec_;
        const bool pressed_in_window = since_press >= 0.0 && since_press <= tuning.skill_roll_window_sec;
        last_press_sec_ = -1e9;  // one press serves one landing

        if (r.impact_speed_mps <= tuning.skill_roll_safe_impact_mps) {
            r.outcome = LandingOutcome::Soft;
            return r;
        }
        if (pressed_in_window) {
            Vec3 dir = look_forward - world_up * Dot(look_forward, world_up);
            dir = Normalized(dir, Normalized(horizontal));
            r.outcome = LandingOutcome::SkillRoll;
            r.velocity = horizontal + dir * (tuning.skill_roll_transfer * std::fabs(vz));
            return r;
        }
        r.outcome = LandingOutcome::HardLanding;
        r.velocity = {0, 0, 0};
        r.stagger_sec = tuning.hard_landing_stagger_sec;
        return r;
    }

private:
    double last_press_sec_ = -1e9;
};

// ---------------------------------------------------------------------------------------
// Gels
// ---------------------------------------------------------------------------------------

struct GelModifiers {
    float max_speed_mult = 1.0f;
    float accel_mult = 1.0f;
    float friction_mult = 1.0f;
    float bounce_beta = 0.0f;  // 0 = no elastic reflection
    float max_wallrun_sec = 1.45f;
};

inline GelModifiers GetGelModifiers(SurfaceGelType gel, const ApertureTuning& t) {
    GelModifiers g;
    g.max_wallrun_sec = t.max_wallrun_duration_sec;
    switch (gel) {
        case SurfaceGelType::OrangePropulsion:
            g.max_speed_mult = t.propulsion_speed_mult;
            g.accel_mult = t.propulsion_speed_mult;
            g.friction_mult = 1.0f - t.propulsion_friction_reduction;
            g.max_wallrun_sec = t.max_wallrun_duration_gel_sec;
            break;
        case SurfaceGelType::BlueRepulsion:
            g.bounce_beta = t.repulsion_bounce_beta;
            break;
        case SurfaceGelType::WhiteConversion:
        case SurfaceGelType::None:
            break;
    }
    return g;
}

// Walljump / Springboard on Repulsion Gel: the into-surface component is reflected and
// amplified, v' = v - (1 + beta) (v.n) n, for v.n < 0. Tangential motion is untouched.
inline Vec3 ApplyRepulsionBounce(Vec3 velocity, Vec3 surface_normal, float beta) {
    const Vec3 n = Normalized(surface_normal);
    const float vn = Dot(velocity, n);
    if (vn >= 0.0f) return velocity;
    return velocity - n * ((1.0f + beta) * vn);
}

// Sprint top speed after accelerating for `elapsed` seconds without a frontal collision:
// linear ramp from walk to max sprint over sprint_time_to_max_sec.
inline float SprintSpeedMps(float elapsed, const ApertureTuning& t) {
    const float a = std::clamp(elapsed / t.sprint_time_to_max_sec, 0.0f, 1.0f);
    return t.walk_speed_mps + (t.sprint_max_speed_mps - t.walk_speed_mps) * a;
}

}  // namespace aperture_runner
