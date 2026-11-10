// G4: Faith's arms in first person. A second viewmodel (slot 1 of the player, the Portal gun keeps
// slot 0) shows models/aperture_runner/faith_arms.mdl (built by mod/tools/extractor) and plays the
// sequence of the current FaithAnimState. The client draws viewmodels from the eye, in their own
// depth pass, so the arms never lag behind the camera or clip into walls.
//
// Nothing here uses fixed offsets: the networked fields (m_hOwner, m_nSequence, m_hViewModel...) are
// looked up by name in the SendTables at run time, and the layout of SendProp is calibrated against a
// field whose offset is known and checked in the game (m_vecViewOffset = 0x320, see source_interfaces.h).
#pragma once

#include <cstdint>

#include "ar/faith_movement.h"

namespace ar {

class FaithViewModel {
public:
    static FaithViewModel& Instance();

    // Interfaces: VSERVERTOOLS002 (gameServerFactory) and VEngineServer021 (interfaceFactory).
    void Init(void* server_tools, void* engine_server);
    bool Ready() const { return tools_ && engine_; }

    // Every Faith tick. `player` is the CBasePlayer* of ProcessMovement.
    // `visible`: Faith is drawn at all (legs and arms); `arms_visible`: the arms viewmodel too.
    void Update(void* player, FaithAnimState anim, bool visible, bool arms_visible);
    void OnLevelShutdown();
    void PrintStatus() const;
    void SetOverride(int sequence) { override_sequence_ = sequence; }  // diagnostics: -1 = follow Faith

    static constexpr const char* kModel = "models/aperture_runner/faith_arms.mdl";
    static constexpr const char* kBodyModel = "models/aperture_runner/faith_body.mdl";
    static constexpr int kSlot = 1;

private:
    bool Create(void* player);
    bool ResolveOffsets(void* player, void* vm);
    bool CreateBody(void* player);
    bool SetKeyValue(void* entity, const char* key, const char* value);

    void* tools_ = nullptr;
    void* engine_ = nullptr;
    void* vm_ = nullptr;          // CBaseViewModel*
    void* vm_player_ = nullptr;   // the player it belongs to
    bool failed_ = false;         // creation failed this level: do not retry every tick
    int model_index_ = -1;
    int last_sequence_ = -1;
    bool last_visible_ = false;
    bool last_arms_visible_ = false;
    bool last_body_visible_ = false;
    int override_sequence_ = -1;

    // Legs and body (faith_body.mdl): a prop_dynamic whose network parent is the player, so it moves
    // with her without interpolation lag; the client animates it (m_bClientSideAnimation).
    void* body_ = nullptr;
    bool body_failed_ = false;
    int set_kv_slot_ = -1;
    int off_move_parent_ = -1;     // body moveparent
    int off_local_origin_ = -1;    // body m_vecOrigin (local to the parent)
    int off_client_anim_ = -1;     // body m_bClientSideAnimation
    int off_body_sequence_ = -1;
    int off_body_playback_ = -1;
    int off_body_effects_ = -1;
    int off_body_new_seq_parity_ = -1;
    char body_status_[120] = "sin crear";
    char status_[160] = "sin crear";

    // Offsets (bytes) found in the SendTables.
    int off_view_model_slot_ = -1;  // player m_hViewModel[kSlot]
    int off_owner_ = -1;            // vm m_hOwner
    int off_vm_index_ = -1;         // vm m_nViewModelIndex
    int off_model_index_ = -1;      // vm m_nModelIndex
    int off_sequence_ = -1;         // vm m_nSequence
    int off_playback_ = -1;         // vm m_flPlaybackRate
    int off_effects_ = -1;          // vm m_fEffects
    int off_anim_parity_ = -1;      // vm m_nAnimationParity
    int off_new_seq_parity_ = -1;   // vm m_nNewSequenceParity
};

}  // namespace ar
