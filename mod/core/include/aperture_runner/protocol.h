// ApertureRunner shared-memory protocol (Local\ApertureRunner_SHM_v1).
//
// Binary contract shared by three processes that may be built differently:
//   - SourceHost  : Source Engine plugin inside portal2.exe (x86)
//   - DiceGuest   : ASI hook inside MirrorsEdge.exe (x86)
//   - Director    : Python AI director (x64, reads via ctypes/mmap)
//
// Rules that keep the layout identical across x86/x64 and compilers:
//   * Only fixed-width integers, float, and fixed-size arrays. No pointers, no bool, no enums
//     as storage (enums are uint8_t/uint16_t fields), no std:: types.
//   * Every struct has explicit padding and a static_assert on its size; region offsets are
//     compile-time constants mirrored in director/aperture_shm.py.
//   * Units are documented per field. "uu" = Unreal Units (UE3, left-handed, 1 m = 100 uu).
//     "su" = Source Units (right-handed, 1 su = 1 inch).
#pragma once

#include <cstddef>
#include <cstdint>

namespace aperture_runner {

inline constexpr wchar_t kShmName[] = L"Local\\ApertureRunner_SHM_v1";
inline constexpr uint32_t kShmMagic = 0x48535241u;  // 'ARSH' little-endian
inline constexpr uint32_t kProtocolVersion = 1;
inline constexpr uint64_t kShmTotalSize = 128ull * 1024ull * 1024ull;

// Skeleton capacity. The real Faith bone count must come from the SDK dump of the user's
// MirrorsEdge.exe; the guest writes the actual count into FaithKinematicState::bone_count.
inline constexpr uint32_t kMaxBones = 128;
inline constexpr uint32_t kMaxPlanesPerBrush = 24;
inline constexpr uint32_t kMaxBrushes = 16384;
inline constexpr uint32_t kEventDetailsLen = 96;
inline constexpr uint32_t kOverlayMaxWidth = 2560;
inline constexpr uint32_t kOverlayMaxHeight = 1440;
inline constexpr uint32_t kOverlayBytesPerPixel = 8;  // RGBA8888 + float32 linear depth

// ---------------------------------------------------------------------------------------
// Enumerations (stored as uint8_t / uint16_t)
// ---------------------------------------------------------------------------------------

// Canonical movement states. The guest maps the game's own EMovement values onto these;
// values the mod does not care about collapse to Other.
enum class TdMovementState : uint8_t {
    Other = 0,
    Walking,
    Sprinting,
    Crouching,
    Sliding,
    WallRunningLeft,
    WallRunningRight,
    WallClimbing,
    WallrunJump,
    Wallclimb180Jump,
    Vaulting,
    SpringBoarding,
    LedgeGrabbing,
    LedgeShimmy,
    SkillRolling,
    Falling,
    BargeDoor,
    JumpKick,
    TurretDisarm,
    Count
};

enum class SurfaceGelType : uint8_t {
    None = 0,
    BlueRepulsion = 1,
    OrangePropulsion = 2,
    WhiteConversion = 3,
};

enum class ApertureEventType : uint16_t {
    None = 0,
    PortalFlingExecuted = 1,
    WallrunThroughPortal = 2,
    TurretDropkicked = 3,
    TurretDisarmed = 4,
    SkillRollFailed = 5,
    ChamberSequenceBroken = 6,
    SkillRollSucceeded = 7,
    PortalTeleport = 8,
};

enum class DirectorActionType : uint16_t {
    None = 0,
    RotateWallPanel = 1,
    SpawnTurretAmbush = 2,
    OpenHatch = 3,
    CloseHatch = 4,
    ReleaseNeurotoxin = 5,
};

enum class EventSource : uint8_t {
    Host = 0,   // Source plugin
    Guest = 1,  // Mirror's Edge hook
};

// Flag bits for FaithKinematicState::flags
inline constexpr uint32_t kFaithFlagInFlowState = 1u << 0;
inline constexpr uint32_t kFaithFlagJustSkillRolled = 1u << 1;
inline constexpr uint32_t kFaithFlagHardLanding = 1u << 2;
inline constexpr uint32_t kFaithFlagOnGround = 1u << 3;
inline constexpr uint32_t kFaithFlagCrouchHeld = 1u << 4;
inline constexpr uint32_t kFaithFlagHoldingCube = 1u << 5;

// ---------------------------------------------------------------------------------------
// Plain data types
// ---------------------------------------------------------------------------------------

struct Float3 {
    float x, y, z;
};
static_assert(sizeof(Float3) == 12);

struct Float4 {
    float x, y, z, w;
};
static_assert(sizeof(Float4) == 16);

// Row-major 4x4, column-vector convention (p' = M * p, translation in m[r][3]).
// Matches Source's matrix3x4_t row layout for the top three rows.
struct Matrix4 {
    float m[4][4];
};
static_assert(sizeof(Matrix4) == 64);

struct BoneTransform {
    Float3 position;  // uu, component space
    float pad0;
    Float4 rotation;  // quaternion (x, y, z, w), UE3 handedness
};
static_assert(sizeof(BoneTransform) == 32);

// Written by: DiceGuest, every tick. Space: UE3 world (uu, left-handed).
struct FaithKinematicState {
    uint64_t tick_counter;
    double guest_time_sec;
    Float3 position;       // uu, capsule centre
    float pad0;
    Float3 velocity;       // uu/s
    float pad1;
    Float4 camera_rotation;     // quaternion
    Float4 body_root_rotation;  // quaternion
    Float3 wall_normal;    // uu-space unit normal of the wall being run/climbed (0 if none)
    float momentum_normalized;  // 0..1
    uint8_t movement_state;     // TdMovementState
    uint8_t active_gel;         // SurfaceGelType as seen by the guest after applying it
    uint16_t pad2;
    uint32_t flags;             // kFaithFlag*
    uint32_t last_applied_teleport_seq;  // ack for TeleportCommand::sequence_id
    uint32_t bone_count;        // <= kMaxBones
    BoneTransform bones[kMaxBones];
};
static_assert(sizeof(FaithKinematicState) == 112 + 32 * kMaxBones);

struct PortalDescriptor {
    Matrix4 world_matrix;  // Source world, su. Column 0 = forward (normal), 2 = up.
    Float3 normal;         // == column 0 of world_matrix, duplicated for readers
    uint8_t active;
    uint8_t linked;
    uint16_t pad0;
};
static_assert(sizeof(PortalDescriptor) == 80);

// Shared origin anchors that make both worlds line up (see math.h WorldCalibration).
struct WorldCalibrationData {
    Float3 source_origin;  // su
    float pad0;
    Float3 ue3_origin;     // uu
    float pad1;
};
static_assert(sizeof(WorldCalibrationData) == 32);

// Written by: SourceHost, every tick.
struct PortalPairState {
    uint64_t tick_counter;
    PortalDescriptor blue;
    PortalDescriptor orange;
    WorldCalibrationData calibration;
    uint32_t last_teleport_sequence_id;
    uint8_t gel_under_feet;  // SurfaceGelType sampled by the host at Faith's feet
    uint8_t pad0[3];
};
static_assert(sizeof(PortalPairState) == 208);

// Host -> Guest: atomic teleport through a portal. Space: UE3 world (uu).
struct TeleportCommand {
    uint32_t sequence_id;
    uint8_t preserve_wallrun;   // 1 if the wallrun continues on the far side
    uint8_t entered_portal;     // 0 = blue, 1 = orange
    uint16_t pad0;
    Float3 new_position;        // uu
    float pad1;
    Float3 new_velocity;        // uu/s
    float pad2;
    Float4 new_camera_rotation; // quaternion, roll not yet corrected (guest decays it)
    Float3 expected_wall_normal;  // uu, valid when preserve_wallrun == 1
    float pad3;
};
static_assert(sizeof(TeleportCommand) == 72);

struct ApertureEventPacket {
    uint64_t tick_counter;
    uint16_t event_type;  // ApertureEventType
    uint8_t source;       // EventSource
    uint8_t pad0;
    float speed_at_event;  // m/s
    Float3 event_position; // su (Source world) for host events, uu for guest events
    float pad1;
    char details[kEventDetailsLen];  // UTF-8, NUL-terminated
};
static_assert(sizeof(ApertureEventPacket) == 128);

// Director -> Host
struct DirectorCommand {
    uint32_t sequence_id;
    uint16_t action;  // DirectorActionType
    uint16_t pad0;
    float param;      // action-specific (e.g. ambush size, panel angle in degrees)
    float pad1;
    char target[48];  // optional entity name, UTF-8, NUL-terminated
};
static_assert(sizeof(DirectorCommand) == 64);

inline constexpr uint16_t kBrushFlagRunnerVision = 1u << 0;  // tint red (#E60012)
inline constexpr uint16_t kBrushFlagClimbableLedge = 1u << 1;
inline constexpr uint16_t kBrushFlagWallrunnable = 1u << 2;
inline constexpr uint16_t kBrushFlagDynamic = 1u << 3;
inline constexpr uint16_t kBrushFlagHardLight = 1u << 4;
inline constexpr uint16_t kBrushFlagPortalable = 1u << 5;

// Convex brush as intersection of half-spaces: plane (n, d) keeps points with n.p <= d.
// Space: UE3 world (uu), already converted by the host.
struct ConvexBrush {
    uint16_t plane_count;
    uint16_t flags;     // kBrushFlag*
    uint8_t gel;        // SurfaceGelType painted on this brush
    uint8_t pad0[3];
    uint32_t source_entity_index;
    uint32_t pad1;
    Float4 planes[kMaxPlanesPerBrush];  // (nx, ny, nz, d)
};
static_assert(sizeof(ConvexBrush) == 16 + 16 * kMaxPlanesPerBrush);

// Elliptic hole a linked portal cuts in its wall; the guest ignores wall contacts inside it.
struct PortalCarve {
    Matrix4 ue3_matrix;  // portal frame converted to UE3 (uu); column 0 = normal
    float half_width;    // uu
    float half_height;   // uu
    float depth;         // uu, how far behind the plane contacts are ignored
    uint8_t active;
    uint8_t pad0[3];
};
static_assert(sizeof(PortalCarve) == 80);

struct BrushSnapshotHeader {
    uint32_t seq;          // seqlock, odd while being written
    uint32_t brush_count;
    uint64_t tick_counter;
    Float3 centre;         // uu
    float radius;          // uu
    PortalCarve carves[2];
};
static_assert(sizeof(BrushSnapshotHeader) == 32 + 2 * 80);

struct OverlayFrameHeader {
    uint32_t seq;
    uint32_t width;
    uint32_t height;
    uint32_t pad0;
    uint64_t tick_counter;
    uint64_t pad1;
};
static_assert(sizeof(OverlayFrameHeader) == 32);

// ---------------------------------------------------------------------------------------
// Ring buffers and seqlocked blocks
// ---------------------------------------------------------------------------------------

inline constexpr uint32_t kCacheLine = 64;

// SPSC ring. head is written only by the producer, tail only by the consumer.
// Both are free-running counters; slot = counter & (Capacity - 1).
template <typename T, uint32_t Capacity>
struct SpscRing {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    uint32_t head;
    uint8_t pad_head[kCacheLine - 4];
    uint32_t tail;
    uint32_t dropped;  // producer-side count of pushes rejected because the ring was full
    uint8_t pad_tail[kCacheLine - 8];
    T slots[Capacity];
};

template <typename T>
struct SeqlockBlock {
    uint32_t seq;
    uint8_t pad[kCacheLine - 4];
    T payload;
};

inline constexpr uint32_t kTeleportRingCapacity = 64;
inline constexpr uint32_t kEventRingCapacity = 256;
inline constexpr uint32_t kDirectorRingCapacity = 64;

using TeleportRing = SpscRing<TeleportCommand, kTeleportRingCapacity>;
using EventRing = SpscRing<ApertureEventPacket, kEventRingCapacity>;
using DirectorRing = SpscRing<DirectorCommand, kDirectorRingCapacity>;
using FaithBlock = SeqlockBlock<FaithKinematicState>;
using PortalBlock = SeqlockBlock<PortalPairState>;

static_assert(sizeof(TeleportRing) == 128 + 72 * kTeleportRingCapacity);
static_assert(sizeof(EventRing) == 128 + 128 * kEventRingCapacity);
static_assert(sizeof(DirectorRing) == 128 + 64 * kDirectorRingCapacity);

// ---------------------------------------------------------------------------------------
// Arena header and fixed region map
// ---------------------------------------------------------------------------------------

inline constexpr uint32_t kInitUntouched = 0;
inline constexpr uint32_t kInitInProgress = 1;
inline constexpr uint32_t kInitReady = 2;

struct ArenaHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t init_state;  // kInit*
    uint32_t header_size;
    uint64_t total_size;
    uint32_t host_pid;
    uint32_t guest_pid;
    uint32_t director_pid;
    uint32_t pad0;
    uint64_t host_heartbeat_ms;      // GetTickCount64() of last host heartbeat
    uint64_t guest_heartbeat_ms;
    uint64_t director_heartbeat_ms;
    uint32_t brush_published_index;  // 0 or 1
    uint32_t overlay_published_index;
};
static_assert(sizeof(ArenaHeader) == 72);

inline constexpr uint64_t AlignUp(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

inline constexpr uint64_t kOffHeader = 0;
inline constexpr uint64_t kOffFaith = 4096;
inline constexpr uint64_t kOffPortal = AlignUp(kOffFaith + sizeof(FaithBlock), 4096);
inline constexpr uint64_t kOffTeleportRing = AlignUp(kOffPortal + sizeof(PortalBlock), 4096);
inline constexpr uint64_t kOffHostEventRing = AlignUp(kOffTeleportRing + sizeof(TeleportRing), 4096);
inline constexpr uint64_t kOffGuestEventRing = AlignUp(kOffHostEventRing + sizeof(EventRing), 4096);
inline constexpr uint64_t kOffDirectorRing = AlignUp(kOffGuestEventRing + sizeof(EventRing), 4096);

inline constexpr uint64_t kBrushBufferSize =
    AlignUp(sizeof(BrushSnapshotHeader) + uint64_t(sizeof(ConvexBrush)) * kMaxBrushes, 4096);
inline constexpr uint64_t kOffBrush0 = AlignUp(kOffDirectorRing + sizeof(DirectorRing), 4096);
inline constexpr uint64_t kOffBrush1 = kOffBrush0 + kBrushBufferSize;

inline constexpr uint64_t kOverlayBufferSize = AlignUp(
    sizeof(OverlayFrameHeader) +
        uint64_t(kOverlayMaxWidth) * kOverlayMaxHeight * kOverlayBytesPerPixel,
    4096);
inline constexpr uint64_t kOffOverlay0 = kOffBrush1 + kBrushBufferSize;
inline constexpr uint64_t kOffOverlay1 = kOffOverlay0 + kOverlayBufferSize;
inline constexpr uint64_t kOffEnd = kOffOverlay1 + kOverlayBufferSize;

static_assert(kOffEnd <= kShmTotalSize, "Region map exceeds the 128 MB arena");

}  // namespace aperture_runner
