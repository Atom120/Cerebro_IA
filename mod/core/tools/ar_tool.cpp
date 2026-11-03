// ar_tool: diagnostics for the shared-memory contract.
//   ar_tool layout              print struct sizes and region offsets as JSON
//   ar_tool emit [seconds]      open the real arena as Host+Guest, publish a Faith state and a
//                               few events, then keep the mapping alive for `seconds`
//                               (default 5) so the Python director can read it.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "aperture_runner/protocol.h"
#include "aperture_runner/shared_arena.h"

using namespace aperture_runner;

namespace {

int PrintLayout() {
    std::printf("{\n");
    std::printf("  \"total_size\": %llu,\n", static_cast<unsigned long long>(kShmTotalSize));
    std::printf("  \"sizeof\": {\"ArenaHeader\": %zu, \"FaithKinematicState\": %zu, \"PortalPairState\": %zu, "
                "\"TeleportCommand\": %zu, \"ApertureEventPacket\": %zu, \"DirectorCommand\": %zu, "
                "\"EventRing\": %zu, \"DirectorRing\": %zu, \"FaithBlock\": %zu},\n",
                sizeof(ArenaHeader), sizeof(FaithKinematicState), sizeof(PortalPairState),
                sizeof(TeleportCommand), sizeof(ApertureEventPacket), sizeof(DirectorCommand),
                sizeof(EventRing), sizeof(DirectorRing), sizeof(FaithBlock));
    std::printf("  \"offsets\": {\"faith\": %llu, \"portal\": %llu, \"teleport_ring\": %llu, "
                "\"host_event_ring\": %llu, \"guest_event_ring\": %llu, \"director_ring\": %llu, "
                "\"brush0\": %llu, \"brush1\": %llu, \"overlay0\": %llu, \"overlay1\": %llu, \"end\": %llu},\n",
                (unsigned long long)kOffFaith, (unsigned long long)kOffPortal,
                (unsigned long long)kOffTeleportRing, (unsigned long long)kOffHostEventRing,
                (unsigned long long)kOffGuestEventRing, (unsigned long long)kOffDirectorRing,
                (unsigned long long)kOffBrush0, (unsigned long long)kOffBrush1,
                (unsigned long long)kOffOverlay0, (unsigned long long)kOffOverlay1,
                (unsigned long long)kOffEnd);
    std::printf("  \"faith_fields\": {\"position\": %zu, \"velocity\": %zu, \"movement_state\": %zu, "
                "\"active_gel\": %zu, \"flags\": %zu, \"momentum_normalized\": %zu},\n",
                offsetof(FaithKinematicState, position), offsetof(FaithKinematicState, velocity),
                offsetof(FaithKinematicState, movement_state), offsetof(FaithKinematicState, active_gel),
                offsetof(FaithKinematicState, flags), offsetof(FaithKinematicState, momentum_normalized));
    std::printf("  \"ring_fields\": {\"head\": %zu, \"tail\": %zu, \"dropped\": %zu, \"slots\": %zu}\n",
                offsetof(EventRing, head), offsetof(EventRing, tail), offsetof(EventRing, dropped),
                offsetof(EventRing, slots));
    std::printf("}\n");
    return 0;
}

int Emit(int seconds) {
    ArenaOpenError err;
    auto host = SharedArena::Open(ArenaRole::Host, &err);
    if (!host) {
        std::fprintf(stderr, "open as host failed: %s\n", ToString(err));
        return 1;
    }
    auto guest = SharedArena::Open(ArenaRole::Guest, &err);
    if (!guest) {
        std::fprintf(stderr, "open as guest failed: %s\n", ToString(err));
        return 1;
    }

    FaithKinematicState faith{};
    faith.tick_counter = 4242;
    faith.position = Float3{120.0f, -40.0f, 175.0f};
    faith.velocity = Float3{778.0f, 0.0f, 0.0f};
    faith.camera_rotation = Float4{0, 0, 0, 1};
    faith.body_root_rotation = Float4{0, 0, 0, 1};
    faith.movement_state = static_cast<uint8_t>(TdMovementState::WallRunningRight);
    faith.active_gel = static_cast<uint8_t>(SurfaceGelType::OrangePropulsion);
    faith.momentum_normalized = 0.9f;
    faith.flags = kFaithFlagInFlowState;
    guest->WriteFaithState(faith);

    ApertureEventPacket e{};
    e.event_type = static_cast<uint16_t>(ApertureEventType::PortalFlingExecuted);
    e.speed_at_event = 21.5f;
    std::snprintf(e.details, sizeof(e.details), "exit 21.5 m/s");
    host->PushEvent(e);
    e.event_type = static_cast<uint16_t>(ApertureEventType::TurretDropkicked);
    e.speed_at_event = 8.1f;
    std::snprintf(e.details, sizeof(e.details), "turret_03 into acid");
    guest->PushEvent(e);
    e.event_type = static_cast<uint16_t>(ApertureEventType::SkillRollFailed);
    e.speed_at_event = 13.0f;
    std::snprintf(e.details, sizeof(e.details), "impact 13.0 m/s, no roll");
    guest->PushEvent(e);

    std::printf("emitted 1 faith state, 1 host event, 2 guest events; holding %d s\n", seconds);
    std::fflush(stdout);
    for (int i = 0; i < seconds * 10; ++i) {
        host->Heartbeat();
        guest->Heartbeat();
        DirectorCommand cmd{};
        while (host->PopDirectorCommand(&cmd)) {
            std::printf("director command seq=%u action=%u param=%.2f target=%s\n", cmd.sequence_id,
                        cmd.action, cmd.param, cmd.target);
            std::fflush(stdout);
        }
        Sleep(100);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "layout") == 0) return PrintLayout();
    if (argc >= 2 && std::strcmp(argv[1], "emit") == 0) return Emit(argc >= 3 ? std::atoi(argv[2]) : 5);
    std::fprintf(stderr, "usage: ar_tool layout | ar_tool emit [seconds]\n");
    return 2;
}
