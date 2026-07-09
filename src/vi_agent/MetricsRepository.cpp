#include "MetricsRepository.h"

#include "PathResolver.h"
#include "ProjectProfile.h"
#include "SQLiteConnection.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QVector>

namespace {

QString quoteIdentifier(const QString& identifier, QString* errorMessage)
{
    static const QRegularExpression validIdentifier(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!validIdentifier.match(identifier).hasMatch()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsafe SQL identifier in profile: %1").arg(identifier);
        }
        return QString();
    }
    return QStringLiteral("\"%1\"").arg(identifier);
}

QString requiredIdentifier(const QString& identifier, const char* label, QString* errorMessage)
{
    if (identifier.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("profile missing SQL identifier: %1").arg(QString::fromLatin1(label));
        }
        return QString();
    }
    return quoteIdentifier(identifier, errorMessage);
}

QString percent(double ratio)
{
    return QStringLiteral("%1%").arg(ratio * 100.0, 0, 'f', 2);
}

} // namespace

bool MetricsRepository::buildBasicReport(
    const AgentConfig& config,
    const QString& userQuestion,
    QString* outReport,
    QString* errorMessage)
{
    if (!outReport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("report output pointer is null");
        }
        return false;
    }

    ProjectProfile profile;
    if (!ProjectProfile::loadFromFile(config.profilePath, &profile, errorMessage)) {
        return false;
    }

    const QString configuredDbPath = config.databasePath.trimmed().isEmpty()
        ? profile.databasePath
        : config.databasePath;
    const QString databasePath = PathResolver::resolveExistingFile(configuredDbPath, profile.profilePath);

    SQLiteConnection db;
    if (!db.openReadOnly(databasePath, errorMessage)) {
        return false;
    }

    const QString itemTable = quoteIdentifier(profile.inspectionItem.table, errorMessage);
    const QString eventTable = quoteIdentifier(profile.runtimeEvent.table, errorMessage);
    const QString timeCol = requiredIdentifier(profile.inspectionItem.time, "inspection_item.time", errorMessage);
    const QString isOkCol = requiredIdentifier(profile.inspectionItem.isOk, "inspection_item.is_ok", errorMessage);
    const QString cameraCol = requiredIdentifier(profile.inspectionItem.cameraId, "inspection_item.camera_id", errorMessage);
    const QString defectCol = requiredIdentifier(profile.inspectionItem.defectCode, "inspection_item.defect_code", errorMessage);
    const QString eventTimeCol = requiredIdentifier(profile.runtimeEvent.time, "runtime_event.time", errorMessage);
    const QString eventLevelCol = requiredIdentifier(profile.runtimeEvent.level, "runtime_event.level", errorMessage);
    const QString eventTypeCol = requiredIdentifier(profile.runtimeEvent.eventType, "runtime_event.event_type", errorMessage);
    const QString eventMessageCol = requiredIdentifier(profile.runtimeEvent.message, "runtime_event.message", errorMessage);
    if (itemTable.isEmpty() || eventTable.isEmpty() || timeCol.isEmpty() || isOkCol.isEmpty()
        || cameraCol.isEmpty() || defectCol.isEmpty() || eventTimeCol.isEmpty() || eventLevelCol.isEmpty()
        || eventTypeCol.isEmpty() || eventMessageCol.isEmpty()) {
        return false;
    }

    const QString cameraNameCol = profile.inspectionItem.cameraName.isEmpty()
        ? cameraCol
        : quoteIdentifier(profile.inspectionItem.cameraName, errorMessage);
    const QString defectNameCol = profile.inspectionItem.defectName.isEmpty()
        ? defectCol
        : quoteIdentifier(profile.inspectionItem.defectName, errorMessage);
    const QString inferMsCol = profile.inspectionItem.inferMs.isEmpty()
        ? QString()
        : quoteIdentifier(profile.inspectionItem.inferMs, errorMessage);
    if (cameraNameCol.isEmpty() || defectNameCol.isEmpty()) {
        return false;
    }

    const QString startTime = profile.defaults.startTime;
    const QString endTime = profile.defaults.endTime;
    if (startTime.isEmpty() || endTime.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("profile default time range is required for the first basic report");
        }
        return false;
    }

    int totalCount = 0;
    int ngCount = 0;
    double avgInferMs = 0.0;
    const QString totalSql = inferMsCol.isEmpty()
        ? QStringLiteral(
            "SELECT COUNT(*), "
            "SUM(CASE WHEN %1 = 0 THEN 1 ELSE 0 END), "
            "0 "
            "FROM %2 WHERE %3 BETWEEN ? AND ?")
              .arg(isOkCol, itemTable, timeCol)
        : QStringLiteral(
            "SELECT COUNT(*), "
            "SUM(CASE WHEN %1 = 0 THEN 1 ELSE 0 END), "
            "AVG(%2) "
            "FROM %3 WHERE %4 BETWEEN ? AND ?")
              .arg(isOkCol, inferMsCol, itemTable, timeCol);

    if (!db.query(totalSql, QStringList() << startTime << endTime, [&](sqlite3_stmt* statement) {
            totalCount = SQLiteConnection::columnInt(statement, 0);
            ngCount = SQLiteConnection::columnInt(statement, 1);
            avgInferMs = SQLiteConnection::columnDouble(statement, 2);
            return true;
        }, errorMessage)) {
        return false;
    }

    struct CameraMetric {
        QString cameraId;
        QString cameraName;
        int total = 0;
        int ng = 0;
        double ngRate = 0.0;
    };
    QVector<CameraMetric> cameraMetrics;

    const QString cameraSql = QStringLiteral(
        "SELECT %1, %2, COUNT(*), "
        "SUM(CASE WHEN %3 = 0 THEN 1 ELSE 0 END), "
        "1.0 * SUM(CASE WHEN %3 = 0 THEN 1 ELSE 0 END) / COUNT(*) AS ng_rate "
        "FROM %4 WHERE %5 BETWEEN ? AND ? "
        "GROUP BY %1, %2 ORDER BY ng_rate DESC")
        .arg(cameraCol, cameraNameCol, isOkCol, itemTable, timeCol);

    if (!db.query(cameraSql, QStringList() << startTime << endTime, [&](sqlite3_stmt* statement) {
            CameraMetric metric;
            metric.cameraId = SQLiteConnection::columnText(statement, 0);
            metric.cameraName = SQLiteConnection::columnText(statement, 1);
            metric.total = SQLiteConnection::columnInt(statement, 2);
            metric.ng = SQLiteConnection::columnInt(statement, 3);
            metric.ngRate = SQLiteConnection::columnDouble(statement, 4);
            cameraMetrics.append(metric);
            return true;
        }, errorMessage)) {
        return false;
    }

    struct DefectMetric {
        QString code;
        QString name;
        int count = 0;
    };
    QVector<DefectMetric> defectMetrics;

    const QString defectSql = QStringLiteral(
        "SELECT %1, %2, COUNT(*) "
        "FROM %3 WHERE %4 BETWEEN ? AND ? "
        "GROUP BY %1, %2 ORDER BY COUNT(*) DESC")
        .arg(defectCol, defectNameCol, itemTable, timeCol);

    if (!db.query(defectSql, QStringList() << startTime << endTime, [&](sqlite3_stmt* statement) {
            DefectMetric metric;
            metric.code = SQLiteConnection::columnText(statement, 0);
            metric.name = SQLiteConnection::columnText(statement, 1);
            metric.count = SQLiteConnection::columnInt(statement, 2);
            defectMetrics.append(metric);
            return true;
        }, errorMessage)) {
        return false;
    }

    struct EventRow {
        QString time;
        QString level;
        QString type;
        QString message;
    };
    QVector<EventRow> events;

    const QString eventSql = QStringLiteral(
        "SELECT %1, %2, %3, %4 "
        "FROM %5 WHERE %1 BETWEEN ? AND ? AND %2 IN ('WARN', 'ERROR') "
        "ORDER BY %1 LIMIT 10")
        .arg(eventTimeCol, eventLevelCol, eventTypeCol, eventMessageCol, eventTable);

    if (!db.query(eventSql, QStringList() << startTime << endTime, [&](sqlite3_stmt* statement) {
            EventRow event;
            event.time = SQLiteConnection::columnText(statement, 0);
            event.level = SQLiteConnection::columnText(statement, 1);
            event.type = SQLiteConnection::columnText(statement, 2);
            event.message = SQLiteConnection::columnText(statement, 3);
            events.append(event);
            return true;
        }, errorMessage)) {
        return false;
    }

    const double ngRate = totalCount > 0 ? static_cast<double>(ngCount) / static_cast<double>(totalCount) : 0.0;

    QString report;
    report += QStringLiteral("SQLite 基础统计链路已打通\n");
    report += QStringLiteral("==========================\n\n");
    report += QStringLiteral("用户问题：%1\n").arg(userQuestion.trimmed().isEmpty() ? QStringLiteral("(未输入)") : userQuestion.trimmed());
    report += QStringLiteral("数据库：%1\n").arg(QFileInfo(databasePath).absoluteFilePath());
    report += QStringLiteral("Profile：%1\n").arg(profile.profilePath);
    report += QStringLiteral("时间范围：%1 ~ %2\n\n").arg(startTime, endTime);

    report += QStringLiteral("总体指标\n");
    report += QStringLiteral("- 总检测数：%1\n").arg(totalCount);
    report += QStringLiteral("- NG 数：%1\n").arg(ngCount);
    report += QStringLiteral("- NG 率：%1\n").arg(percent(ngRate));
    report += QStringLiteral("- 平均推理耗时：%1 ms\n\n").arg(avgInferMs, 0, 'f', 1);

    report += QStringLiteral("按相机统计\n");
    double bestNgRate = 0.0;
    double secondNgRate = 0.0;
    QString bestCamera;
    for (int i = 0; i < cameraMetrics.size(); ++i) {
        const CameraMetric& metric = cameraMetrics.at(i);
        if (i == 0) {
            bestNgRate = metric.ngRate;
            bestCamera = metric.cameraId;
        } else if (i == 1) {
            secondNgRate = metric.ngRate;
        }
        report += QStringLiteral("- %1（%2）：总数 %3，NG %4，NG 率 %5\n")
            .arg(metric.cameraId, metric.cameraName)
            .arg(metric.total)
            .arg(metric.ng)
            .arg(percent(metric.ngRate));
    }
    report += QStringLiteral("\n");

    report += QStringLiteral("缺陷分布\n");
    for (const DefectMetric& metric : defectMetrics) {
        report += QStringLiteral("- %1（%2）：%3\n").arg(metric.code, metric.name).arg(metric.count);
    }
    report += QStringLiteral("\n");

    report += QStringLiteral("运行事件 WARN/ERROR\n");
    if (events.isEmpty()) {
        report += QStringLiteral("- 未发现 WARN/ERROR 事件\n");
    } else {
        for (const EventRow& event : events) {
            report += QStringLiteral("- [%1] %2 %3：%4\n").arg(event.time, event.level, event.type, event.message);
        }
    }
    report += QStringLiteral("\n");

    report += QStringLiteral("初步异常判断\n");
    if (ngRate >= profile.thresholds.ngRateWarn) {
        report += QStringLiteral("- 整体 NG 率 %1 高于阈值 %2，需要关注。\n")
            .arg(percent(ngRate), percent(profile.thresholds.ngRateWarn));
    } else {
        report += QStringLiteral("- 整体 NG 率 %1 未超过阈值 %2。\n")
            .arg(percent(ngRate), percent(profile.thresholds.ngRateWarn));
    }

    if (!bestCamera.isEmpty() && secondNgRate > 0.0 && bestNgRate / secondNgRate >= profile.thresholds.cameraNgRateRatioWarn) {
        report += QStringLiteral("- 相机 %1 的 NG 率明显高于第二高相机，建议优先检查该相机、光源和视野区域。\n")
            .arg(bestCamera);
    } else if (!bestCamera.isEmpty()) {
        report += QStringLiteral("- 当前最高 NG 率相机为 %1，但相机间差异未达到强异常比例阈值。\n").arg(bestCamera);
    }

    *outReport = report;
    return true;
}

