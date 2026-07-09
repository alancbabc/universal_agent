#pragma once

#include "AgentConfig.h"

#include <QDialog>

class QLabel;
class QTextEdit;

class ViAgentDialog : public QDialog {
public:
    explicit ViAgentDialog(QWidget* parent = nullptr);

    void setConfig(const AgentConfig& config);

private:
    QString formatConfigSummary(const AgentConfig& config) const;

    QLabel* projectIdValue_ = nullptr;
    QLabel* databasePathValue_ = nullptr;
    QLabel* profilePathValue_ = nullptr;
    QLabel* modelEndpointValue_ = nullptr;
    QTextEdit* resultView_ = nullptr;
    AgentConfig config_;
};

