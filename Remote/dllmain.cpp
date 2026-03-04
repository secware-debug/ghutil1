// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "ClientInit.h"
#include "RemoteExport.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        ClientShutdown();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

// Exported for 3rd party exe: call via GetProcAddress(hDll, "Remote_Initialize")
extern "C" REMOTE_API void Remote_Initialize(void)
{
    ClientInit(nullptr);
}

extern "C" REMOTE_API void Remote_Shutdown(void)
{
    ClientShutdown();
}

