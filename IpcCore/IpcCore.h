#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	/* IPCCORE_EXPORTS is defined only when building the DLL itself.
	   Inside the DLL  -> dllexport (내보내기)
	   Outside the DLL -> dllimport (가져오기) */
#ifdef IPCCORE_EXPORTS
#define IPCCORE_API __declspec(dllexport)
#else
#define IPCCORE_API __declspec(dllimport)
#endif

	   /* smoke test only - will be removed */
	IPCCORE_API int __cdecl f_IpcPing(void);

#ifdef __cplusplus
}
#endif