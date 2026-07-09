#pragma once

#include "AgentConfig.h"
#include "vi_agent_api.h"

#include <memory>

#include <QString>

class ViAgentDialog;

class AgentRuntime {
public:
    static AgentRuntime& instance();

    int init(const char* initJsonUtf8);
    int openWindow(void* parentHwnd);
    int closeWindow();
    int uninit();
    int getLastError(char* buffer, int bufferSize) const;

    const AgentConfig& config() const;
    bool isInitialized() const;

private:
    AgentRuntime() = default;
    ~AgentRuntime();

    AgentRuntime(const AgentRuntime&) = delete;
    AgentRuntime& operator=(const AgentRuntime&) = delete;

    void setLastError(QString error);
    void clearLastError();
    int showExistingWindow();
    int createDialogIfNeeded(void* parentHwnd);

    bool initialized_ = false;
    AgentConfig config_;
    QString lastError_;
    std::unique_ptr<ViAgentDialog> dialog_;
};

