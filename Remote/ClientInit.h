#pragma once

// Call once when the DLL is attached (e.g. from DllMain DLL_PROCESS_ATTACH).
// Loads config.json, connects to server via WebSocket (HTTPS tunnel), and sends a test "target_hit" message.
void ClientInit(HMODULE hModule);

// Shutdown (call from DLL_PROCESS_DETACH if desired).
void ClientShutdown();
