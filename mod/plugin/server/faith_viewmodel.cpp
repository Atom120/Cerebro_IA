#define _CRT_SECURE_NO_WARNINGS 1
#include "faith_viewmodel.h"

#include <cstdio>
#include <cstring>

#include "server_plugin.h"
#include "source_interfaces.h"

namespace ar {

namespace {

// IServerTools (toolframework/itoolentity.h, VSERVERTOOLS002). In this server.dll the vtable starts
// with a virtual destructor, so every slot is one more than in the SDK header. Read from the binary
// (2026-11-10): [14] pushes (1, -1, name) and calls ::CreateEntityByName, [15] wraps ::DispatchSpawn.
constexpr int kToolsCreateEntityByNameSlot = 14;
constexpr int kToolsDispatchSpawnSlot = 15;
// IVEngineServer (eiface.h, VEngineServer021): 4 PrecacheModel(const char*, bool preload).
constexpr int kEnginePrecacheModelSlot = 4;
// IHandleEntity / IServerUnknown / IServerNetworkable.
constexpr int kUnknownGetRefEHandleSlot = 2;
constexpr int kNetworkableGetServerClassSlot = 1;
constexpr int kNetworkableGetEdictSlot = 2;
// edict_t::m_fStateFlags (public/edict.h): FL_EDICT_CHANGED | FL_FULL_EDICT_CHANGED.
constexpr int kEdictChangedFlags = (1 << 0) | (1 << 8);
constexpr int kEffectNoDraw = 0x20;  // EF_NODRAW
// Known in this build (source_interfaces.h, checked in the game by the crouch test 64 -> 28):
// m_vecViewOffset (x at 0x320). Calibrates the SendProp layout.
constexpr const char* kKnownField = "m_vecViewOffset[0]";
constexpr int kKnownFieldOffset = 0x320;

template <typename T>
T VCall(void* self, int slot) {
    return reinterpret_cast<T>((*reinterpret_cast<void***>(self))[slot]);
}

bool IsName(const char* s) {
#ifdef _WIN32
    __try {
        if (!s) return false;
        int n = 0;
        for (; s[n] && n < 96; ++n) {
            if (s[n] < 0x20 || s[n] > 0x7e) return false;
        }
        return n > 0 && n < 96;
    } __except (1) {
        return false;
    }
#else
    return s != nullptr;
#endif
}

struct RawTable {
    char* props;
    int count;
    const char* name;
};

// SendProp layout (dt_send.h), calibrated at run time.
struct PropLayout {
    int stride = 0;
    int name = -1;
    int table = -1;
    int offset = -1;
    bool Valid() const { return stride > 0 && name >= 0 && table >= 0 && offset >= 0; }
};
PropLayout g_layout;

const char* PropName(const RawTable* t, int i, const PropLayout& l) {
    return *reinterpret_cast<const char* const*>(t->props + i * l.stride + l.name);
}
const RawTable* PropTable(const RawTable* t, int i, const PropLayout& l) {
    const RawTable* sub = *reinterpret_cast<const RawTable* const*>(t->props + i * l.stride + l.table);
    return (sub && IsName(sub->name) && std::strncmp(sub->name, "DT_", 3) == 0) ? sub : nullptr;
}
int PropOffset(const RawTable* t, int i, const PropLayout& l) {
    return *reinterpret_cast<const int*>(t->props + i * l.stride + l.offset);
}

int FindRec(const RawTable* t, const char* name, int base, const PropLayout& l, int depth) {
    if (!t || depth > 12 || t->count <= 0 || t->count > 4096) return -1;
    for (int i = 0; i < t->count; ++i) {
        const char* n = PropName(t, i, l);
        const int off = PropOffset(t, i, l);
        if (IsName(n) && std::strcmp(n, name) == 0 && off > 0) return base + off;
    }
    for (int i = 0; i < t->count; ++i) {
        const RawTable* sub = PropTable(t, i, l);
        if (!sub) continue;
        const int r = FindRec(sub, name, base + PropOffset(t, i, l), l, depth + 1);
        if (r >= 0) return r;
    }
    return -1;
}

int SafeFind(const RawTable* t, const char* name, const PropLayout& l) {
#ifdef _WIN32
    __try {
        return FindRec(t, name, 0, l, 0);
    } __except (1) {
        return -1;
    }
#else
    return FindRec(t, name, 0, l, 0);
#endif
}

// Finds stride, name, datatable and offset positions inside SendProp from the player's table.
PropLayout Calibrate(const RawTable* t) {
    PropLayout best;
#ifdef _WIN32
    __try {
        if (!t || t->count < 4 || t->count > 4096) return best;
        for (int stride = 0x40; stride <= 0x70 && !best.Valid(); stride += 4) {
            for (int name = 0; name < stride && !best.Valid(); name += 4) {
                PropLayout l;
                l.stride = stride;
                l.name = name;
                bool names_ok = true;
                for (int i = 0; i < t->count && names_ok; ++i) names_ok = IsName(PropName(t, i, l));
                if (!names_ok) continue;
                for (int table = 0; table < stride && !best.Valid(); table += 4) {
                    if (table == name) continue;
                    l.table = table;
                    bool has_sub = false;
                    for (int i = 0; i < t->count && !has_sub; ++i) has_sub = PropTable(t, i, l) != nullptr;
                    if (!has_sub) continue;
                    for (int offset = 0; offset < stride; offset += 4) {
                        if (offset == name || offset == table) continue;
                        l.offset = offset;
                        if (FindRec(t, kKnownField, 0, l, 0) == kKnownFieldOffset) {
                            best = l;
                            break;
                        }
                    }
                }
            }
        }
    } __except (1) {
        best = PropLayout{};
    }
#else
    (void)t;
#endif
    return best;
}

const RawTable* TableOf(void* entity) {
#ifdef _WIN32
    __try {
        void* networkable = VCall<void*(__thiscall*)(void*)>(entity, source::kServerUnknownGetNetworkableSlot)(entity);
        if (!networkable) return nullptr;
        char* server_class = VCall<char*(__thiscall*)(void*)>(networkable, kNetworkableGetServerClassSlot)(networkable);
        return server_class ? *reinterpret_cast<const RawTable* const*>(server_class + 4) : nullptr;  // m_pTable
    } __except (1) {
        return nullptr;
    }
#else
    (void)entity;
    return nullptr;
#endif
}

void MarkChanged(void* entity) {
#ifdef _WIN32
    __try {
        void* networkable = VCall<void*(__thiscall*)(void*)>(entity, source::kServerUnknownGetNetworkableSlot)(entity);
        int* edict = networkable ? VCall<int*(__thiscall*)(void*)>(networkable, kNetworkableGetEdictSlot)(networkable) : nullptr;
        if (edict) *edict |= kEdictChangedFlags;
    } __except (1) {
    }
#else
    (void)entity;
#endif
}

uint32_t EHandleOf(void* entity) {
#ifdef _WIN32
    __try {
        const uint32_t* h = VCall<const uint32_t*(__thiscall*)(void*)>(entity, kUnknownGetRefEHandleSlot)(entity);
        return h ? *h : 0xFFFFFFFFu;
    } __except (1) {
        return 0xFFFFFFFFu;
    }
#else
    (void)entity;
    return 0xFFFFFFFFu;
#endif
}

const char* ClassNameOf(void* entity) {
#ifdef _WIN32
    __try {
        void* networkable = VCall<void*(__thiscall*)(void*)>(entity, source::kServerUnknownGetNetworkableSlot)(entity);
        const char* n = networkable ? VCall<const char*(__thiscall*)(void*)>(networkable, source::kServerNetworkableGetClassNameSlot)(networkable) : nullptr;
        return IsName(n) ? n : "";
    } __except (1) {
        return "";
    }
#else
    (void)entity;
    return "";
#endif
}

template <typename T>
void Put(void* base, int offset, T value) {
    if (offset > 0) *reinterpret_cast<T*>(static_cast<char*>(base) + offset) = value;
}
template <typename T>
T Get(void* base, int offset) {
    return offset > 0 ? *reinterpret_cast<T*>(static_cast<char*>(base) + offset) : T{};
}

// Diagnostics when the calibration fails: the table header and the first props as raw dwords.
void DumpTable(const RawTable* t) {
#ifdef _WIN32
    __try {
        if (!t) {
            ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] G4 dump: sin tabla\n");
            return;
        }
        ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] G4 dump: tabla=%p props=%p count=%d name=%s\n", t, t->props,
                                           t->count, IsName(t->name) ? t->name : "?");
        const uint32_t* w = reinterpret_cast<const uint32_t*>(t->props);
        for (int row = 0; row < 3 * 0x60 / 16; ++row) {
            char line[256];
            int n = std::snprintf(line, sizeof(line), "[ApertureRunner] G4 dump +%03X:", row * 16);
            for (int k = 0; k < 4; ++k) {
                const uint32_t v = w[row * 4 + k];
                const char* str = reinterpret_cast<const char*>(static_cast<uintptr_t>(v));
                n += std::snprintf(line + n, sizeof(line) - n, " %08X%s%s", v, IsName(str) ? "=" : "", IsName(str) ? str : "");
            }
            ApertureRunnerServerPlugin::LogMsg("%s\n", line);
        }
    } __except (1) {
        ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] G4 dump: excepcion\n");
    }
#else
    (void)t;
#endif
}

}  // namespace

FaithViewModel& FaithViewModel::Instance() {
    static FaithViewModel instance;
    return instance;
}

void FaithViewModel::Init(void* server_tools, void* engine_server) {
    tools_ = server_tools;
    engine_ = engine_server;
}

void FaithViewModel::OnLevelShutdown() {
    vm_ = nullptr;  // the engine deletes the entity with the level
    vm_player_ = nullptr;
    failed_ = false;
    model_index_ = -1;
    last_sequence_ = -1;
    last_visible_ = false;
    std::snprintf(status_, sizeof(status_), "sin crear");
}

bool FaithViewModel::ResolveOffsets(void* player, void* vm) {
    if (!g_layout.Valid()) {
        // Layout read from this build (2026-11-10): SendProp is 0x50 bytes with a vtable; m_pVarName at
        // +0x30, m_pDataTable at +0x44, m_Offset at +0x48. Checked against m_vecViewOffset[0]; else search.
        PropLayout known;
        known.stride = 0x50;
        known.name = 0x30;
        known.table = 0x44;
        known.offset = 0x48;
        const int known_off = SafeFind(TableOf(player), kKnownField, known);
        ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] G4: %s con la disposicion conocida = 0x%X (esperado 0x%X)\n", kKnownField,
                                           known_off, kKnownFieldOffset);
        g_layout = known_off == kKnownFieldOffset ? known : Calibrate(TableOf(player));
    }
    if (!g_layout.Valid()) {
        DumpTable(TableOf(player));
        std::snprintf(status_, sizeof(status_), "no se pudo calibrar SendProp (m_vecViewOffset)");
        return false;
    }
    const RawTable* pt = TableOf(player);
    const RawTable* vt = TableOf(vm);
    const int view_models = SafeFind(pt, "m_hViewModel", g_layout);
    off_view_model_slot_ = view_models > 0 ? view_models + 4 * kSlot : -1;
    off_owner_ = SafeFind(vt, "m_hOwner", g_layout);
    off_vm_index_ = SafeFind(vt, "m_nViewModelIndex", g_layout);
    off_model_index_ = SafeFind(vt, "m_nModelIndex", g_layout);
    off_sequence_ = SafeFind(vt, "m_nSequence", g_layout);
    off_playback_ = SafeFind(vt, "m_flPlaybackRate", g_layout);
    off_effects_ = SafeFind(vt, "m_fEffects", g_layout);
    off_anim_parity_ = SafeFind(vt, "m_nAnimationParity", g_layout);
    off_new_seq_parity_ = SafeFind(vt, "m_nNewSequenceParity", g_layout);
    ApertureRunnerServerPlugin::LogMsg(
        "[ApertureRunner] G4 netvars: SendProp stride=0x%X nombre=+0x%X tabla=+0x%X offset=+0x%X | "
        "m_hViewModel[1]=0x%X m_hOwner=0x%X m_nViewModelIndex=0x%X m_nModelIndex=0x%X m_nSequence=0x%X "
        "m_flPlaybackRate=0x%X m_fEffects=0x%X m_nAnimationParity=0x%X m_nNewSequenceParity=0x%X\n",
        g_layout.stride, g_layout.name, g_layout.table, g_layout.offset, off_view_model_slot_, off_owner_, off_vm_index_,
        off_model_index_, off_sequence_, off_playback_, off_effects_, off_anim_parity_, off_new_seq_parity_);
    const bool ok = off_view_model_slot_ > 0 && off_owner_ > 0 && off_vm_index_ > 0 && off_model_index_ > 0 &&
                    off_sequence_ > 0 && off_effects_ > 0;
    if (!ok) std::snprintf(status_, sizeof(status_), "faltan netvars del viewmodel");
    return ok;
}

bool FaithViewModel::Create(void* player) {
#ifdef _WIN32
    const char* volatile stage = "PrecacheModel";
    __try {
        model_index_ = VCall<int(__thiscall*)(void*, const char*, bool)>(engine_, kEnginePrecacheModelSlot)(engine_, kModel, true);
        if (model_index_ <= 0) {
            std::snprintf(status_, sizeof(status_), "falta %s (corre el extractor)", kModel);
            return false;
        }
        stage = "CreateEntityByName";
        void* vm = VCall<void*(__thiscall*)(void*, const char*)>(tools_, kToolsCreateEntityByNameSlot)(tools_, "viewmodel");
        if (!vm || std::strcmp(ClassNameOf(vm), "viewmodel") != 0) {
            std::snprintf(status_, sizeof(status_), "CreateEntityByName(viewmodel) fallo");
            return false;
        }
        stage = "DispatchSpawn";
        VCall<void(__thiscall*)(void*, void*)>(tools_, kToolsDispatchSpawnSlot)(tools_, vm);
        stage = "netvars";
        if (!ResolveOffsets(player, vm)) return false;
        Put<uint32_t>(vm, off_owner_, EHandleOf(player));
        Put<int>(vm, off_vm_index_, kSlot);
        Put<int16_t>(vm, off_model_index_, static_cast<int16_t>(model_index_));
        Put<float>(vm, off_playback_, 1.0f);
        Put<uint32_t>(player, off_view_model_slot_, EHandleOf(vm));
        MarkChanged(vm);
        MarkChanged(player);
        vm_ = vm;
        vm_player_ = player;
        std::snprintf(status_, sizeof(status_), "activo (modelo %d, ehandle 0x%X)", model_index_, EHandleOf(vm));
        ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] G4: brazos de Faith creados (%s, modelo %d).\n", kModel, model_index_);
        return true;
    } __except (1) {
        std::snprintf(status_, sizeof(status_), "excepcion al crear el viewmodel (paso %s)", stage);
        return false;
    }
#else
    (void)player;
    return false;
#endif
}

void FaithViewModel::Update(void* player, FaithAnimState anim, bool visible) {
    if (!Ready() || !player) return;
    if (vm_ && vm_player_ != player) OnLevelShutdown();
    if (!vm_) {
        if (failed_ || !visible) return;
        if (!Create(player)) {
            failed_ = true;
            ApertureRunnerServerPlugin::LogMsg("[ApertureRunner] G4: sin brazos de Faith: %s\n", status_);
            return;
        }
    }
#ifdef _WIN32
    __try {
        bool changed = false;
        // Keep the slot pointing at our viewmodel (the game never touches slot 1, but be safe).
        const uint32_t handle = EHandleOf(vm_);
        if (Get<uint32_t>(player, off_view_model_slot_) != handle) {
            Put<uint32_t>(player, off_view_model_slot_, handle);
            MarkChanged(player);
        }
        if (visible != last_visible_) {
            const int effects = Get<int>(vm_, off_effects_);
            Put<int>(vm_, off_effects_, visible ? (effects & ~kEffectNoDraw) : (effects | kEffectNoDraw));
            last_visible_ = visible;
            changed = true;
        }
        // Sequence index = FaithAnimState: the extractor writes the .qc sequences in that order.
        const int sequence = override_sequence_ >= 0 ? override_sequence_ : static_cast<int>(anim);
        if (sequence != last_sequence_) {
            Put<int>(vm_, off_sequence_, sequence);
            Put<int>(vm_, off_anim_parity_, (Get<int>(vm_, off_anim_parity_) + 1) & 7);
            Put<int>(vm_, off_new_seq_parity_, (Get<int>(vm_, off_new_seq_parity_) + 1) & 7);
            last_sequence_ = sequence;
            changed = true;
        }
        if (changed) MarkChanged(vm_);
    } __except (1) {
        vm_ = nullptr;
        failed_ = true;
        std::snprintf(status_, sizeof(status_), "excepcion al actualizar el viewmodel");
    }
#else
    (void)anim;
    (void)visible;
#endif
}

void FaithViewModel::PrintStatus() const {
    ApertureRunnerServerPlugin::LogMsg("[AR] brazos (G4): %s; secuencia=%s\n", status_,
                                       last_sequence_ >= 0 ? SequenceName(static_cast<FaithAnimState>(last_sequence_)) : "-");
}

}  // namespace ar
