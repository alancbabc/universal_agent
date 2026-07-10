#include "ProjectProfile.h"

#include "PathResolver.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace {

QString readString(const QJsonObject& object, const char* key)
{
    const QJsonValue value = object.value(QString::fromLatin1(key));
    return value.isString() ? value.toString().trimmed() : QString();
}

double readDouble(const QJsonObject& object, const char* key, double fallback)
{
    const QJsonValue value = object.value(QString::fromLatin1(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

int readInt(const QJsonObject& object, const char* key, int fallback)
{
    const QJsonValue value = object.value(QString::fromLatin1(key));
    return value.isDouble() ? value.toInt(fallback) : fallback;
}

bool requireString(const QJsonObject& object, const char* key, QString* outValue, QString* errorMessage)
{
    const QString value = readString(object, key);
    if (value.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("profile missing required field: %1").arg(QString::fromLatin1(key));
        }
        return false;
    }

    *outValue = value;
    return true;
}

} // namespace

bool ProjectProfile::loadFromFile(const QString& profilePath, ProjectProfile* outProfile, QString* errorMessage)
{
    if (!outProfile) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("profile output pointer is null");
        }
        return false;
    }

    const QString resolvedPath = PathResolver::resolveExistingFile(profilePath);
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to open profile: %1").arg(resolvedPath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to parse profile JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    ProjectProfile profile;
    profile.profilePath = QFileInfo(resolvedPath).canonicalFilePath();
    profile.projectId = readString(root, "project_id");

    const QJsonObject database = root.value(QStringLiteral("database")).toObject();
    profile.databasePath = readString(database, "path");

    const QJsonObject entities = root.value(QStringLiteral("entities")).toObject();
    const QJsonObject item = entities.value(QStringLiteral("inspection_item")).toObject();
    const QJsonObject defect = entities.value(QStringLiteral("defect_instance")).toObject();
    const QJsonObject event = entities.value(QStringLiteral("runtime_event")).toObject();

    if (!requireString(item, "table", &profile.inspectionItem.table, errorMessage)
        || !requireString(item, "time", &profile.inspectionItem.time, errorMessage)
        || !requireString(item, "camera_id", &profile.inspectionItem.cameraId, errorMessage)
        || !requireString(item, "is_ok", &profile.inspectionItem.isOk, errorMessage)
        || !requireString(item, "defect_code", &profile.inspectionItem.defectCode, errorMessage)) {
        return false;
    }

    profile.inspectionItem.cameraName = readString(item, "camera_name");
    profile.inspectionItem.defectName = readString(item, "defect_name");
    profile.inspectionItem.inferMs = readString(item, "infer_ms");
    profile.inspectionItem.triggerToReceiveMs = readString(item, "trigger_to_receive_ms");
    profile.inspectionItem.receiveToDetectMs = readString(item, "receive_to_detect_ms");
    profile.inspectionItem.toDetectToResultMs = readString(item, "to_detect_to_result_ms");

    profile.defectInstance.table = readString(defect, "table");
    profile.defectInstance.time = readString(defect, "time");
    profile.defectInstance.cameraId = readString(defect, "camera_id");
    profile.defectInstance.defectCode = readString(defect, "defect_code");
    profile.defectInstance.defectName = readString(defect, "defect_name");

    if (!requireString(event, "table", &profile.runtimeEvent.table, errorMessage)
        || !requireString(event, "time", &profile.runtimeEvent.time, errorMessage)
        || !requireString(event, "level", &profile.runtimeEvent.level, errorMessage)
        || !requireString(event, "event_type", &profile.runtimeEvent.eventType, errorMessage)
        || !requireString(event, "message", &profile.runtimeEvent.message, errorMessage)) {
        return false;
    }
    profile.runtimeEvent.source = readString(event, "source");

    const QJsonObject defaults = root.value(QStringLiteral("analysis_defaults")).toObject();
    const QJsonObject range = defaults.value(QStringLiteral("default_time_range")).toObject();
    profile.defaults.startTime = readString(range, "start");
    profile.defaults.endTime = readString(range, "end");

    const QJsonObject thresholds = root.value(QStringLiteral("thresholds")).toObject();
    profile.thresholds.ngRateWarn = readDouble(thresholds, "ng_rate_warn", profile.thresholds.ngRateWarn);
    profile.thresholds.cameraNgRateRatioWarn = readDouble(
        thresholds,
        "camera_ng_rate_ratio_warn",
        profile.thresholds.cameraNgRateRatioWarn);
    profile.thresholds.inferP95MsWarn = readDouble(thresholds, "infer_p95_ms_warn", profile.thresholds.inferP95MsWarn);
    profile.thresholds.triggerToReceiveP95MsWarn = readDouble(
        thresholds,
        "trigger_to_receive_p95_ms_warn",
        profile.thresholds.triggerToReceiveP95MsWarn);
    profile.thresholds.receiveToDetectP95MsWarn = readDouble(
        thresholds,
        "receive_to_detect_p95_ms_warn",
        profile.thresholds.receiveToDetectP95MsWarn);
    profile.thresholds.toDetectToResultP95MsWarn = readDouble(
        thresholds,
        "to_detect_to_result_p95_ms_warn",
        profile.thresholds.toDetectToResultP95MsWarn);
    profile.thresholds.continuousNgWarn = readInt(thresholds, "continuous_ng_warn", profile.thresholds.continuousNgWarn);

    *outProfile = profile;
    return true;
}
