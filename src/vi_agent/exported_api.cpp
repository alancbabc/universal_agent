#include "vi_agent_api.h"

#include "AgentRuntime.h"

int VI_AGENT_CALL vi_agent_init(const char* initJsonUtf8)
{
    try {
        return AgentRuntime::instance().init(initJsonUtf8);
    } catch (...) {
        return VI_AGENT_UNKNOWN_ERROR;
    }
}

int VI_AGENT_CALL vi_agent_openWindow(void* parentHwnd)
{
    try {
        return AgentRuntime::instance().openWindow(parentHwnd);
    } catch (...) {
        return VI_AGENT_UNKNOWN_ERROR;
    }
}

int VI_AGENT_CALL vi_agent_closeWindow()
{
    try {
        return AgentRuntime::instance().closeWindow();
    } catch (...) {
        return VI_AGENT_UNKNOWN_ERROR;
    }
}

int VI_AGENT_CALL vi_agent_uninit()
{
    try {
        return AgentRuntime::instance().uninit();
    } catch (...) {
        return VI_AGENT_UNKNOWN_ERROR;
    }
}

int VI_AGENT_CALL vi_agent_getLastError(char* buffer, int bufferSize)
{
    try {
        return AgentRuntime::instance().getLastError(buffer, bufferSize);
    } catch (...) {
        return VI_AGENT_UNKNOWN_ERROR;
    }
}

