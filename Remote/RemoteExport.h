#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef REMOTE_EXPORTS
#define REMOTE_API __declspec(dllexport)
#else
#define REMOTE_API __declspec(dllimport)
#endif

// Call after LoadLibrary to start the client (loads config.json, connects via WebSocket, sends target_hit).
// 3rd party exe: LoadLibrary("Remote.dll"), then GetProcAddress(h, "Remote_Initialize"), then call the function.
REMOTE_API void Remote_Initialize(void);

// Optional: call before FreeLibrary to shut down the client cleanly.
REMOTE_API void Remote_Shutdown(void);

#ifdef __cplusplus
}
#endif
