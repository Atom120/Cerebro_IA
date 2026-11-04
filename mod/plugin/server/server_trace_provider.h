#pragma once

#include "ar/trace_provider.h"
#include "source_interfaces.h"

namespace ar {

class ServerTraceProvider final : public ITraceProvider {
public:
    ServerTraceProvider() = default;
    explicit ServerTraceProvider(source::IEngineTrace* engine_trace, void* ignore_player = nullptr);
    ~ServerTraceProvider() override = default;

    void SetEngineTrace(source::IEngineTrace* engine_trace) { engine_trace_ = engine_trace; }
    source::IEngineTrace* GetEngineTrace() const { return engine_trace_; }

    void SetIgnorePlayer(void* player) { ignore_player_ = player; }
    void* GetIgnorePlayer() const { return ignore_player_; }

    TraceResult TraceHull(Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs) const override;
    TraceResult TraceRay(Vec3 start, Vec3 end) const override;

private:
    source::IEngineTrace* engine_trace_ = nullptr;
    void* ignore_player_ = nullptr;
};

}  // namespace ar
