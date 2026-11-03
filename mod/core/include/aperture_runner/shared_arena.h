// Win32 owner of the Local\ApertureRunner_SHM_v1 mapping and the typed accessors on top of it.
//
// Concurrency model (each region has exactly one writer):
//   FaithBlock          seqlock   writer: Guest          readers: Host, Director
//   PortalBlock         seqlock   writer: Host           readers: Guest, Director
//   TeleportRing        SPSC      producer: Host         consumer: Guest
//   HostEventRing       SPSC      producer: Host         consumer: Director
//   GuestEventRing      SPSC      producer: Guest        consumer: Director
//   DirectorRing        SPSC      producer: Director     consumer: Host
//   Brush buffers 0/1   double-buffered seqlock, writer: Host, reader: Guest
//   Overlay buffers 0/1 double-buffered seqlock, writer: Guest, reader: Host
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "aperture_runner/protocol.h"

namespace aperture_runner {

enum class ArenaRole : uint8_t { Host, Guest, Director, Test };

enum class ArenaOpenError : uint8_t {
    None,
    CreateMappingFailed,
    MapViewFailed,
    InitTimeout,
    MagicMismatch,
    VersionMismatch,
    SizeMismatch,
};

const char* ToString(ArenaOpenError error);

class SharedArena {
public:
    // Opens the mapping, creating it if neither peer has yet. Whichever process wins the
    // init_state CAS initialises the header; the others wait up to init_timeout_ms.
    static std::unique_ptr<SharedArena> Open(ArenaRole role, ArenaOpenError* out_error,
                                             const std::wstring& name = kShmName,
                                             uint32_t init_timeout_ms = 2000);

    ~SharedArena();
    SharedArena(const SharedArena&) = delete;
    SharedArena& operator=(const SharedArena&) = delete;

    ArenaRole role() const { return role_; }
    const ArenaHeader& header() const { return *header_; }

    // Liveness
    void Heartbeat();
    bool IsPeerAlive(ArenaRole peer, uint64_t timeout_ms) const;

    // Seqlocked state blocks. Read* returns false if a consistent copy could not be taken
    // within max_attempts (the writer was mid-update every time) or nothing was written yet.
    void WriteFaithState(const FaithKinematicState& state);
    bool ReadFaithState(FaithKinematicState* out, uint32_t max_attempts = 64) const;
    void WritePortalState(const PortalPairState& state);
    bool ReadPortalState(PortalPairState* out, uint32_t max_attempts = 64) const;

    // Rings. Push returns false (and bumps the ring's dropped counter) when full.
    bool PushTeleport(const TeleportCommand& cmd);
    bool PopTeleport(TeleportCommand* out);
    bool PushEvent(const ApertureEventPacket& packet);  // ring chosen by this arena's role
    bool PopEvent(EventSource source, ApertureEventPacket* out);
    bool PushDirectorCommand(const DirectorCommand& cmd);
    bool PopDirectorCommand(DirectorCommand* out);

    // Brush snapshots (host writes, guest reads). The writer fills the unpublished buffer in
    // place, then commits, which flips the published index.
    struct BrushWriteView {
        BrushSnapshotHeader* header;
        ConvexBrush* brushes;  // capacity kMaxBrushes
    };
    BrushWriteView BeginBrushSnapshot();
    void CommitBrushSnapshot(const BrushWriteView& view);
    // Copies the published snapshot into the caller's storage. brushes must hold kMaxBrushes.
    bool ReadBrushSnapshot(BrushSnapshotHeader* out_header, ConvexBrush* out_brushes,
                           uint32_t max_attempts = 8) const;

    // First-person body overlay (guest writes, host reads).
    struct OverlayWriteView {
        OverlayFrameHeader* header;
        uint8_t* pixels;  // width * height * kOverlayBytesPerPixel
    };
    OverlayWriteView BeginOverlayFrame(uint32_t width, uint32_t height);
    void CommitOverlayFrame(const OverlayWriteView& view);
    // Invokes consume with a pointer into shared memory; returns false if the frame was
    // overwritten while consume ran (caller should discard what it uploaded).
    bool ReadOverlayFrame(const std::function<void(const OverlayFrameHeader&, const uint8_t*)>& consume) const;

    uint8_t* base() const { return base_; }

private:
    SharedArena(ArenaRole role, void* mapping, uint8_t* base);

    template <typename T>
    T* At(uint64_t offset) const { return reinterpret_cast<T*>(base_ + offset); }

    ArenaRole role_;
    void* mapping_;  // HANDLE
    uint8_t* base_;
    ArenaHeader* header_;
};

}  // namespace aperture_runner
