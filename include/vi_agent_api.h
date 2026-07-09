#pragma once

#ifdef _WIN32
#ifdef VI_AGENT_BUILD_DLL
#define VI_AGENT_API __declspec(dllexport)
#else
#define VI_AGENT_API __declspec(dllimport)
#endif
#define VI_AGENT_CALL __cdecl
#else
#define VI_AGENT_API
#define VI_AGENT_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum ViAgentErrorCode {
    VI_AGENT_OK = 0,
    VI_AGENT_INVALID_ARGUMENT = -1,
    VI_AGENT_JSON_PARSE_FAILED = -2,
    VI_AGENT_NOT_INITIALIZED = -3,
    VI_AGENT_QAPPLICATION_MISSING = -4,
    VI_AGENT_WINDOW_CREATE_FAILED = -5,
    VI_AGENT_CONFIG_INVALID = -6,
    VI_AGENT_UNKNOWN_ERROR = -100
};

VI_AGENT_API int VI_AGENT_CALL vi_agent_init(const char* initJsonUtf8);
VI_AGENT_API int VI_AGENT_CALL vi_agent_openWindow(void* parentHwnd);
VI_AGENT_API int VI_AGENT_CALL vi_agent_closeWindow();
VI_AGENT_API int VI_AGENT_CALL vi_agent_uninit();
VI_AGENT_API int VI_AGENT_CALL vi_agent_getLastError(char* buffer, int bufferSize);

#ifdef __cplusplus
}
#endif

