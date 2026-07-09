#pragma once

#include "AgentConfig.h"

#include <QString>

class MetricsRepository {
public:
    static bool buildBasicReport(const AgentConfig& config, const QString& userQuestion, QString* outReport, QString* errorMessage);
};

