#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct AgentConfig {
    QString projectId;
    QString databasePath;
    QString profilePath;
    QString imageRoot;
    QString modelEndpoint;
    QString apiKey;
    QString uiLanguage = QStringLiteral("zh-CN");
    QStringList logPaths;
    QJsonObject rawJson;

    static bool fromJsonUtf8(const char* initJsonUtf8, AgentConfig* outConfig, QString* errorMessage);
};

