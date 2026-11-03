// CLI for Mirror's Edge read-only telemetry recorder (B2).
// Owner: Benja (mod/tools/me_telemetry/).
//
// Usage:
//   me_telemetry_recorder --probe
//   me_telemetry_recorder --record qa/telemetria/live_capture.csv --duration 10 --hz 120
//   me_telemetry_recorder --generate-reference qa/telemetria --hz 120
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "me_telemetry.h"

namespace {

void PrintUsage() {
    std::printf(
        "ApertureRunner Mirror's Edge Telemetry Recorder (Read-Only, GOG v1.01 0x4965FFF1)\n"
        "Usage:\n"
        "  me_telemetry_recorder --probe\n"
        "  me_telemetry_recorder --record <out.csv> [--duration <sec>] [--hz <60..120>]\n"
        "  me_telemetry_recorder --generate-reference <out_dir> [--hz <60..120>]\n");
}

int RunProbe() {
    using namespace ar::telemetry;
    const LiveProbeResult r = ProbeLiveMirrorsEdge();
    std::printf("=== Mirror's Edge Read-Only Live Probe (GOG v1.01) ===\n");
    std::printf("  Target ImageBase : 0x%08X (PE Timestamp expected: 0x%08X)\n",
                gog_v101::kImageBase, gog_v101::kPeTimestamp);
    std::printf("  GEngine VA       : 0x%08X -> +0x%03X -> [0] -> +0x%02X -> +0x%03X (ATdPawn)\n",
                gog_v101::kGEngineVa, gog_v101::kOffsetGamePlayersData,
                gog_v101::kOffsetLocalPlayerActor, gog_v101::kOffsetControllerPawn);
    std::printf("  Pawn Offsets     : Loc=+0x%03X, Vel=+0x%03X, MoveState=+0x%03X, WalkState=+0x%03X\n",
                gog_v101::kOffsetActorLocation, gog_v101::kOffsetActorVelocity,
                gog_v101::kOffsetTdPawnMovementState, gog_v101::kOffsetTdPawnWalkingState);

    if (!r.process_found) {
        std::printf("  [STATUS] MirrorsEdge.exe is not currently running.\n");
        return 1;
    }
    std::printf("  Process PID      : %u (Read-Only handle: %s, PE Timestamp: 0x%08X)\n", r.pid,
                r.handle_opened_readonly ? "YES" : "NO", r.pe_timestamp);
    std::printf("  GEngine          : 0x%08X\n", r.gengine_ptr);
    std::printf("  GamePlayers.Data : 0x%08X -> LocalPlayer[0] = 0x%08X\n",
                r.game_players_data_ptr, r.local_player_ptr);
    std::printf("  Controller       : 0x%08X\n", r.controller_ptr);
    std::printf("  Pawn (Faith)     : 0x%08X (vtable = 0x%08X, Physics = %u)\n", r.pawn_ptr,
                r.pawn_vtable, static_cast<unsigned>(r.physics_mode));

    if (!r.IsChainComplete()) {
        std::printf("  [STATUS] Pointer chain incomplete (load into a map/level first).\n");
        return 2;
    }

    const ConvertedSourceSample src = ConvertSampleToSource(r.sample);
    std::printf("  Position (uu)    : (%.2f, %.2f, %.2f) -> Source (su): (%.2f, %.2f, %.2f)\n",
                r.sample.x, r.sample.y, r.sample.z, src.origin_su.x, src.origin_su.y,
                src.origin_su.z);
    std::printf("  Velocity (uu/s)  : (%.2f, %.2f, %.2f) -> Source (su/s): (%.2f, %.2f, %.2f)\n",
                r.sample.vx, r.sample.vy, r.sample.vz, src.velocity_su.x, src.velocity_su.y,
                src.velocity_su.z);
    std::printf("  MovementState    : %u (%s) -> FaithAnimState: %s\n",
                static_cast<unsigned>(r.sample.movement_state),
                MeMovementStateName(r.sample.movement_state),
                ar::SequenceName(src.faith_anim));
    std::printf("  WalkingState     : %u (%s)\n", static_cast<unsigned>(r.sample.walking_state),
                MeWalkingStateName(r.sample.walking_state));
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    bool do_probe = false;
    std::string record_path;
    std::string ref_dir;
    double duration_sec = 5.0;
    int hz = 120;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--probe") == 0) {
            do_probe = true;
        } else if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            record_path = argv[++i];
        } else if (std::strcmp(argv[i], "--generate-reference") == 0 && i + 1 < argc) {
            ref_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration_sec = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--hz") == 0 && i + 1 < argc) {
            hz = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            PrintUsage();
            return 0;
        }
    }

    if (do_probe) {
        return RunProbe();
    }
    if (!ref_dir.empty()) {
        if (!ar::telemetry::GenerateAllReferenceCsvs(ref_dir, hz)) {
            std::fprintf(stderr, "Failed to write reference CSVs to %s\n", ref_dir.c_str());
            return 1;
        }
        std::printf("Generated 9 reference telemetry CSVs at %d Hz in %s\n", hz, ref_dir.c_str());
        return 0;
    }
    if (!record_path.empty()) {
        std::size_t rows = 0;
        std::printf("Recording MirrorsEdge.exe (read-only) for %.2f s at %d Hz -> %s ...\n",
                    duration_sec, hz, record_path.c_str());
        if (!ar::telemetry::RecordLiveMirrorsEdge(record_path, duration_sec, hz, &rows)) {
            std::fprintf(stderr,
                         "Failed to record from MirrorsEdge.exe (make sure a map is loaded).\n");
            return 1;
        }
        std::printf("Wrote %zu samples to %s\n", rows, record_path.c_str());
        return 0;
    }

    PrintUsage();
    return 0;
}
