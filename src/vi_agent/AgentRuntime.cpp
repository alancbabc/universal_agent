#include "AgentRuntime.h"

#include "ViAgentDialog.h"

#include <QApplication>
#include <QByteArray>

#include <cstring>
#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

AgentRuntime& AgentRuntime::instance()
{
    static AgentRuntime runtime;
    return runtime;
}

AgentRuntime::~AgentRuntime()
{
    uninit();
}

int AgentRuntime::init(const char* initJsonUtf8)
{
    clearLastError();

    if (!initJsonUtf8 || initJsonUtf8[0] == '\0') {
        setLastError(QStringLiteral("init JSON is empty"));
        return VI_AGENT_INVALID_ARGUMENT;
    }

    AgentConfig parsedConfig;
    QString errorMessage;
    if (!AgentConfig::fromJsonUtf8(initJsonUtf8, &parsedConfig, &errorMessage)) {
        setLastError(errorMessage);
        return errorMessage.contains(QStringLiteral("parse"), Qt::CaseInsensitive)
            ? VI_AGENT_JSON_PARSE_FAILED
            : VI_AGENT_CONFIG_INVALID;
    }

    closeWindow();
    config_ = parsedConfig;
    initialized_ = true;
    return VI_AGENT_OK;
}

int AgentRuntime::openWindow(void* parentHwnd)
{
    clearLastError();

    if (!initialized_) {
        setLastError(QStringLiteral("vi_agent_init must be called before vi_agent_openWindow"));
        return VI_AGENT_NOT_INITIALIZED;
    }

    if (!QApplication::instance()) {
        setLastError(QStringLiteral("QApplication instance is missing in host process"));
        return VI_AGENT_QAPPLICATION_MISSING;
    }

    const int createResult = createDialogIfNeeded(parentHwnd);
    if (createResult != VI_AGENT_OK) {
        return createResult;
    }

    return showExistingWindow();
}

int AgentRuntime::closeWindow()
{
    clearLastError();
    if (dialog_) {
        dialog_->close();
        dialog_.reset();
    }
    return VI_AGENT_OK;
}

int AgentRuntime::uninit()
{
    clearLastError();
    closeWindow();
    config_ = AgentConfig();
    initialized_ = false;
    return VI_AGENT_OK;
}

int AgentRuntime::getLastError(char* buffer, int bufferSize) const
{
    if (!buffer || bufferSize <= 0) {
        return VI_AGENT_INVALID_ARGUMENT;
    }

    const QByteArray bytes = lastError_.toUtf8();
    const int copySize = qMin(bufferSize - 1, bytes.size());
    if (copySize > 0) {
        memcpy(buffer, bytes.constData(), static_cast<size_t>(copySize));
    }
    buffer[copySize] = '\0';
    return VI_AGENT_OK;
}

const AgentConfig& AgentRuntime::config() const
{
    return config_;
}

bool AgentRuntime::isInitialized() const
{
    return initialized_;
}

void AgentRuntime::setLastError(QString error)
{
    lastError_ = std::move(error);
}

void AgentRuntime::clearLastError()
{
    lastError_.clear();
}

int AgentRuntime::showExistingWindow()
{
    if (!dialog_) {
        setLastError(QStringLiteral("agent dialog was not created"));
        return VI_AGENT_WINDOW_CREATE_FAILED;
    }

    dialog_->show();
    dialog_->raise();
    dialog_->activateWindow();
    return VI_AGENT_OK;
}

int AgentRuntime::createDialogIfNeeded(void* parentHwnd)
{
    if (dialog_) {
        dialog_->setConfig(config_);
        return VI_AGENT_OK;
    }

    dialog_ = std::make_unique<ViAgentDialog>();
    dialog_->setConfig(config_);

#ifdef Q_OS_WIN
    if (parentHwnd) {
        HWND childHwnd = reinterpret_cast<HWND>(dialog_->winId());
        SetWindowLongPtrW(childHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(parentHwnd));
    }
#else
    Q_UNUSED(parentHwnd);
#endif

    return VI_AGENT_OK;
}