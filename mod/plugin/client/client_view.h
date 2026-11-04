#pragma once

// G4 (camera part; Gonzalo's area, written by Sebas): reads and writes the local player's view
// angles through the client engine interface (engine.dll "VEngineClient0xx", slots 19/20 in
// Source 2013). In single player the server and client run on the same thread, so the server-side
// Faith driver can call this every tick to apply mod/camera: the 180 turn, the wallrun roll and the
// landing shake.

namespace ar {

class ClientView {
public:
    static bool Init();                       // finds IVEngineClient in engine.dll
    static bool Ready();
    static const char* Version();             // interface version found, or ""
    static bool GetViewAngles(float angles[3]);
    static bool SetViewAngles(const float angles[3]);
};

}  // namespace ar
