#include "aperture_runner/shared_arena.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstring>

namespace aperture_runner {
namespace {

uint32_t LoadAcquire(const uint32_t& v) {
    return std::atomic_ref<uint32_t>(const_cast<uint32_t&>(v)).load(std::memory_order_acquire);
}
void StoreRelease(uint32_t& v, uint32_t value) {
    std::atomic_ref<uint32_t>(v).store(value, std::memory_order_release);
}
uint64_t Load64(const uint64_t& v) {
    return std::atomic_ref<uint64_t>(const_cast<uint64_t&>(v)).load(std::memory_order_acquire);
}
void Store64(uint64_t& v, uint64_t value) {
    std::atomic_ref<uint64_t>(v).store(value, std::memory_order_release);
}

// Seqlock writer: odd while writing, even when stable.
template <typename T>
void SeqlockWrite(uint32_t& seq, T& dst, const T& src) {
    const uint32_t s = std::atomic_ref<uint32_t>(seq).load(std::memory_order_relaxed);
    StoreRelease(seq, s + 1);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(&dst, &src, sizeof(T));
    std::atomic_thread_fence(std::memory_order_release);
    StoreRelease(seq, s + 2);
}

template <typename T>
bool SeqlockRead(const uint32_t& seq, const T& src, T* out, uint32_t max_attempts) {
    for (uint32_t attempt = 0; attempt < max_attempts; ++attempt) {
        const uint32_t before = LoadAcquire(seq);
        if (before == 0) return false;  // never written
        if (before & 1u) {
            YieldProcessor();
            continue;
        }
        std::memcpy(out, &src, sizeof(T));
        std::atomic_thread_fence(std::memory_order_acquire);
        if (LoadAcquire(seq) == before) return true;
    }
    return false;
}

template <typename T, uint32_t N>
bool RingPush(SpscRing<T, N>& ring, const T& item) {
    const uint32_t head = std::atomic_ref<uint32_t>(ring.head).load(std::memory_order_relaxed);
    const uint32_t tail = LoadAcquire(ring.tail);
    if (head - tail >= N) {
        std::atomic_ref<uint32_t>(ring.dropped).fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    std::memcpy(&ring.slots[head & (N - 1)], &item, sizeof(T));
    StoreRelease(ring.head, head + 1);
    return true;
}

template <typename T, uint32_t N>
bool RingPop(SpscRing<T, N>& ring, T* out) {
    const uint32_t tail = std::atomic_ref<uint32_t>(ring.tail).load(std::memory_order_relaxed);
    const uint32_t head = LoadAcquire(ring.head);
    if (head == tail) return false;
    std::memcpy(out, &ring.slots[tail & (N - 1)], sizeof(T));
    StoreRelease(ring.tail, tail + 1);
    return true;
}

}  // namespace

const char* ToString(ArenaOpenError error) {
    switch (error) {
        case ArenaOpenError::None: return "None";
        case ArenaOpenError::CreateMappingFailed: return "CreateMappingFailed";
        case ArenaOpenError::MapViewFailed: return "MapViewFailed";
        case ArenaOpenError::InitTimeout: return "InitTimeout";
        case ArenaOpenError::MagicMismatch: return "MagicMismatch";
        case ArenaOpenError::VersionMismatch: return "VersionMismatch";
        case ArenaOpenError::SizeMismatch: return "SizeMismatch";
    }
    return "Unknown";
}

std::unique_ptr<SharedArena> SharedArena::Open(ArenaRole role, ArenaOpenError* out_error,
                                               const std::wstring& name, uint32_t init_timeout_ms) {
    ArenaOpenError dummy;
    ArenaOpenError& err = out_error ? *out_error : dummy;
    err = ArenaOpenError::None;

    const DWORD size_high = static_cast<DWORD>(kShmTotalSize >> 32);
    const DWORD size_low = static_cast<DWORD>(kShmTotalSize & 0xFFFFFFFFull);
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, size_high,
                                        size_low, name.c_str());
    if (mapping == nullptr) {
        err = ArenaOpenError::CreateMappingFailed;
        return nullptr;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(kShmTotalSize));
    if (view == nullptr) {
        CloseHandle(mapping);
        err = ArenaOpenError::MapViewFailed;
        return nullptr;
    }

    auto* base = static_cast<uint8_t*>(view);
    auto* header = reinterpret_cast<ArenaHeader*>(base + kOffHeader);

    // Pagefile-backed mappings start zeroed, so init_state == kInitUntouched on first open.
    uint32_t expected = kInitUntouched;
    std::atomic_ref<uint32_t> init(header->init_state);
    if (init.compare_exchange_strong(expected, kInitInProgress, std::memory_order_acq_rel)) {
        header->magic = kShmMagic;
        header->version = kProtocolVersion;
        header->header_size = sizeof(ArenaHeader);
        header->total_size = kShmTotalSize;
        header->brush_published_index = 0;
        header->overlay_published_index = 0;
        init.store(kInitReady, std::memory_order_release);
    } else {
        const ULONGLONG deadline = GetTickCount64() + init_timeout_ms;
        while (init.load(std::memory_order_acquire) != kInitReady) {
            if (GetTickCount64() > deadline) {
                UnmapViewOfFile(view);
                CloseHandle(mapping);
                err = ArenaOpenError::InitTimeout;
                return nullptr;
            }
            Sleep(1);
        }
    }

    ArenaOpenError validation = ArenaOpenError::None;
    if (header->magic != kShmMagic) validation = ArenaOpenError::MagicMismatch;
    else if (header->version != kProtocolVersion) validation = ArenaOpenError::VersionMismatch;
    else if (header->total_size != kShmTotalSize) validation = ArenaOpenError::SizeMismatch;
    if (validation != ArenaOpenError::None) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        err = validation;
        return nullptr;
    }

    const uint32_t pid = GetCurrentProcessId();
    switch (role) {
        case ArenaRole::Host: header->host_pid = pid; break;
        case ArenaRole::Guest: header->guest_pid = pid; break;
        case ArenaRole::Director: header->director_pid = pid; break;
        case ArenaRole::Test: break;
    }

    std::unique_ptr<SharedArena> arena(new SharedArena(role, mapping, base));
    arena->Heartbeat();
    return arena;
}

SharedArena::SharedArena(ArenaRole role, void* mapping, uint8_t* base)
    : role_(role), mapping_(mapping), base_(base), header_(reinterpret_cast<ArenaHeader*>(base)) {}

SharedArena::~SharedArena() {
    if (base_) UnmapViewOfFile(base_);
    if (mapping_) CloseHandle(static_cast<HANDLE>(mapping_));
}

void SharedArena::Heartbeat() {
    const uint64_t now = GetTickCount64();
    switch (role_) {
        case ArenaRole::Host: Store64(header_->host_heartbeat_ms, now); break;
        case ArenaRole::Guest: Store64(header_->guest_heartbeat_ms, now); break;
        case ArenaRole::Director: Store64(header_->director_heartbeat_ms, now); break;
        case ArenaRole::Test: break;
    }
}

bool SharedArena::IsPeerAlive(ArenaRole peer, uint64_t timeout_ms) const {
    uint64_t last = 0;
    switch (peer) {
        case ArenaRole::Host: last = Load64(header_->host_heartbeat_ms); break;
        case ArenaRole::Guest: last = Load64(header_->guest_heartbeat_ms); break;
        case ArenaRole::Director: last = Load64(header_->director_heartbeat_ms); break;
        case ArenaRole::Test: return false;
    }
    return last != 0 && GetTickCount64() - last <= timeout_ms;
}

void SharedArena::WriteFaithState(const FaithKinematicState& state) {
    auto* block = At<FaithBlock>(kOffFaith);
    SeqlockWrite(block->seq, block->payload, state);
}

bool SharedArena::ReadFaithState(FaithKinematicState* out, uint32_t max_attempts) const {
    const auto* block = At<FaithBlock>(kOffFaith);
    return SeqlockRead(block->seq, block->payload, out, max_attempts);
}

void SharedArena::WritePortalState(const PortalPairState& state) {
    auto* block = At<PortalBlock>(kOffPortal);
    SeqlockWrite(block->seq, block->payload, state);
}

bool SharedArena::ReadPortalState(PortalPairState* out, uint32_t max_attempts) const {
    const auto* block = At<PortalBlock>(kOffPortal);
    return SeqlockRead(block->seq, block->payload, out, max_attempts);
}

bool SharedArena::PushTeleport(const TeleportCommand& cmd) {
    return RingPush(*At<TeleportRing>(kOffTeleportRing), cmd);
}

bool SharedArena::PopTeleport(TeleportCommand* out) {
    return RingPop(*At<TeleportRing>(kOffTeleportRing), out);
}

bool SharedArena::PushEvent(const ApertureEventPacket& packet) {
    ApertureEventPacket stamped = packet;
    stamped.details[kEventDetailsLen - 1] = '\0';
    if (role_ == ArenaRole::Guest) {
        stamped.source = static_cast<uint8_t>(EventSource::Guest);
        return RingPush(*At<EventRing>(kOffGuestEventRing), stamped);
    }
    stamped.source = static_cast<uint8_t>(EventSource::Host);
    return RingPush(*At<EventRing>(kOffHostEventRing), stamped);
}

bool SharedArena::PopEvent(EventSource source, ApertureEventPacket* out) {
    const uint64_t offset = source == EventSource::Guest ? kOffGuestEventRing : kOffHostEventRing;
    return RingPop(*At<EventRing>(offset), out);
}

bool SharedArena::PushDirectorCommand(const DirectorCommand& cmd) {
    DirectorCommand safe = cmd;
    safe.target[sizeof(safe.target) - 1] = '\0';
    return RingPush(*At<DirectorRing>(kOffDirectorRing), safe);
}

bool SharedArena::PopDirectorCommand(DirectorCommand* out) {
    return RingPop(*At<DirectorRing>(kOffDirectorRing), out);
}

SharedArena::BrushWriteView SharedArena::BeginBrushSnapshot() {
    const uint32_t published = LoadAcquire(header_->brush_published_index);
    const uint64_t offset = published == 0 ? kOffBrush1 : kOffBrush0;
    auto* hdr = At<BrushSnapshotHeader>(offset);
    const uint32_t s = std::atomic_ref<uint32_t>(hdr->seq).load(std::memory_order_relaxed);
    StoreRelease(hdr->seq, (s & 1u) ? s + 2 : s + 1);  // always land on an odd value
    std::atomic_thread_fence(std::memory_order_release);
    return {hdr, reinterpret_cast<ConvexBrush*>(base_ + offset + sizeof(BrushSnapshotHeader))};
}

void SharedArena::CommitBrushSnapshot(const BrushWriteView& view) {
    if (view.header->brush_count > kMaxBrushes) view.header->brush_count = kMaxBrushes;
    std::atomic_thread_fence(std::memory_order_release);
    const uint32_t s = std::atomic_ref<uint32_t>(view.header->seq).load(std::memory_order_relaxed);
    StoreRelease(view.header->seq, s + 1);  // odd -> even
    const uint64_t offset = reinterpret_cast<uint8_t*>(view.header) - base_;
    StoreRelease(header_->brush_published_index, offset == kOffBrush0 ? 0u : 1u);
}

bool SharedArena::ReadBrushSnapshot(BrushSnapshotHeader* out_header, ConvexBrush* out_brushes,
                                    uint32_t max_attempts) const {
    for (uint32_t attempt = 0; attempt < max_attempts; ++attempt) {
        const uint32_t published = LoadAcquire(header_->brush_published_index);
        const uint64_t offset = published == 0 ? kOffBrush0 : kOffBrush1;
        const auto* hdr = At<BrushSnapshotHeader>(offset);
        const uint32_t before = LoadAcquire(hdr->seq);
        if (before == 0) return false;
        if (before & 1u) continue;
        std::memcpy(out_header, hdr, sizeof(BrushSnapshotHeader));
        const uint32_t count = out_header->brush_count > kMaxBrushes ? kMaxBrushes : out_header->brush_count;
        std::memcpy(out_brushes, base_ + offset + sizeof(BrushSnapshotHeader), sizeof(ConvexBrush) * count);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (LoadAcquire(hdr->seq) == before) {
            out_header->brush_count = count;
            return true;
        }
    }
    return false;
}

SharedArena::OverlayWriteView SharedArena::BeginOverlayFrame(uint32_t width, uint32_t height) {
    if (width > kOverlayMaxWidth) width = kOverlayMaxWidth;
    if (height > kOverlayMaxHeight) height = kOverlayMaxHeight;
    const uint32_t published = LoadAcquire(header_->overlay_published_index);
    const uint64_t offset = published == 0 ? kOffOverlay1 : kOffOverlay0;
    auto* hdr = At<OverlayFrameHeader>(offset);
    const uint32_t s = std::atomic_ref<uint32_t>(hdr->seq).load(std::memory_order_relaxed);
    StoreRelease(hdr->seq, (s & 1u) ? s + 2 : s + 1);
    std::atomic_thread_fence(std::memory_order_release);
    hdr->width = width;
    hdr->height = height;
    return {hdr, base_ + offset + sizeof(OverlayFrameHeader)};
}

void SharedArena::CommitOverlayFrame(const OverlayWriteView& view) {
    std::atomic_thread_fence(std::memory_order_release);
    const uint32_t s = std::atomic_ref<uint32_t>(view.header->seq).load(std::memory_order_relaxed);
    StoreRelease(view.header->seq, s + 1);
    const uint64_t offset = reinterpret_cast<uint8_t*>(view.header) - base_;
    StoreRelease(header_->overlay_published_index, offset == kOffOverlay0 ? 0u : 1u);
}

bool SharedArena::ReadOverlayFrame(
    const std::function<void(const OverlayFrameHeader&, const uint8_t*)>& consume) const {
    const uint32_t published = LoadAcquire(header_->overlay_published_index);
    const uint64_t offset = published == 0 ? kOffOverlay0 : kOffOverlay1;
    const auto* hdr = At<OverlayFrameHeader>(offset);
    const uint32_t before = LoadAcquire(hdr->seq);
    if (before == 0 || (before & 1u)) return false;
    OverlayFrameHeader copy;
    std::memcpy(&copy, hdr, sizeof(copy));
    consume(copy, base_ + offset + sizeof(OverlayFrameHeader));
    std::atomic_thread_fence(std::memory_order_acquire);
    return LoadAcquire(hdr->seq) == before;
}

}  // namespace aperture_runner
