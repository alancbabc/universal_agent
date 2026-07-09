#include "AgentConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

QString optionalString(const QJsonObject& object, const char* key)
{
    const QJsonValue value = object.value(QString::fromLatin1(key));
    return value.isString() ? value.toString().trimmed() : QString();
}

QStringList optionalStringList(const QJsonObject& object, const char* key)
{
    QStringList values;
    const QJsonValue value = object.value(QString::fromLatin1(key));
    if (!value.isArray()) {
        return values;
    }

    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        if (item.isString()) {
            const QString text = item.toString().trimmed();
            if (!text.isEmpty()) {
                values.append(text);
            }
        }
    }
    return values;
}

} // namespace

bool AgentConfig::fromJsonUtf8(const char* initJsonUtf8, AgentConfig* outConfig, QString* errorMessage)
{
    if (!initJsonUtf8 || !outConfig) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("init JSON is null");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(initJsonUtf8), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to parse init JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject object = document.object();
    AgentConfig config;
    config.rawJson = object;
    config.projectId = optionalString(object, "project_id");
    config.databasePath = optionalString(object, "database_path");
    config.profilePath = optionalString(object, "profile_path");
    config.imageRoot = optionalString(object, "image_root");
    config.modelEndpoint = optionalString(object, "model_endpoint");
    config.apiKey = optionalString(object, "api_key");
    config.logPaths = optionalStringList(object, "log_paths");

    const QString uiLanguage = optionalString(object, "ui_language");
    if (!uiLanguage.isEmpty()) {
        config.uiLanguage = uiLanguage;
    }

    if (config.projectId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("project_id is required");
        }
        return false;
    }

    *outConfig = config;
    return true;
}