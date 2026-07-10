#include "MetricsRepository.h"

#include "PathResolver.h"
#include "ProjectProfile.h"
#include "SQLiteConnection.h"

#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <algorithm>

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

bool optionalIdentifier(const QString& identifier, QString* outIdentifier, QString* errorMessage)
{
    if (!outIdentifier) {
        return false;
    }

    *outIdentifier = QString();
    if (identifier.trimmed().isEmpty()) {
        return true;
    }

    *outIdentifier = quoteIdentifier(identifier, errorMessage);
    return !outIdentifier->isEmpty();
}

QString percent(double ratio)
{
    return QStringLiteral("%1%").arg(ratio * 100.0, 0, 'f', 2);
}

struct NumericStats {
    int count = 0;
    double min = 0.0;
    double avg = 0.0;
    double p95 = 0.0;
    double max = 0.0;

    bool hasData() const { return count > 0; }
};

bool queryNumericStats(
    SQLiteConnection& db,
    const QString& table,
    const QString& timeColumn,
    const QString& valueColumn,
    const QString& startTime,
    const QString& endTime,
    NumericStats* outStats,
    QString* errorMessage)
{
    if (!outStats || valueColumn.isEmpty()) {
        return true;
    }

    QVector<double> values;
    const QString sql = QStringLiteral(
        "SELECT %1 FROM %2 WHERE %3 BETWEEN ? AND ? AND %1 IS NOT NULL ORDER BY %1")
        .arg(valueColumn, table, timeColumn);

    if (!db.query(sql, QStringList() << startTime << endTime, [&](sqlite3_stmt* statement) {
            values.append(SQLiteConnection::columnDouble(statement, 0));
            return true;
        }, errorMessage)) {
        return false;
    }

    if (values.isEmpty()) {
        return true;
    }

    std::sort(values.begin(), values.end());
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }

    outStats->count = values.size();
    outStats->min = values.first();
    outStats->avg = sum / static_cast<double>(values.size());
    outStats->max = values.last();
    const int p95Index = static_cast<int>((values.size() - 1) * 0.95);
    outStats->p95 = values.at(p95Index);
    return true;
}

void appendTimingStat(QString* report, const QString& label, const NumericStats& stats, double warnThreshold)
{
    if (!report) {
        return;
    }

    if (!stats.hasData()) {
        *report += QStringLiteral("- %1：无数据\n").arg(label);
        return;
    }

    *report += QStringLiteral("- %1：样本 %2，min %3 ms，avg %4 ms，p95 %5 ms，max %6 ms")
        .arg(label)
        .arg(stats.count)
        .arg(stats.min, 0, 'f', 1)
        .arg(stats.avg, 0, 'f', 1)
        .arg(stats.p95, 0, 'f', 1)
        .arg(stats.max, 0, 'f', 1);
    if (warnThreshold > 0.0 && stats.p95 >= warnThreshold) {
        *report += QStringLiteral("，超过阈值 %1 ms").arg(warnThreshold, 0, 'f', 1);
    }
    *report += QStringLiteral("\n");
}

} // namespace

bool MetricsRepository::buildBasicReport(
    const AgentConfig& config,
    const QString& userQuestion,
    QString* outReport,
    QString* errorMessage)
{
    return buildBasicReport(config, userQuestion, QString(), QString(), outReport, errorMessage);
}

bool MetricsRepository::buildBasicReport(
    const AgentConfig& config,
    const QString& userQuestion,
    const QString& requestedStartTime,
    const QString& requestedEndTime,
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

    const QString itemTable = requiredIdentifier(profile.inspectionItem.table, "inspection_item.table", errorMessage);
    const QString eventTable = requiredIdentifier(profile.runtimeEvent.table, "runtime_event.table", errorMessage);
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

    QString cameraNameCol;
    QString defectNameCol;
    QString inferMsCol;
    QString triggerToReceiveMsCol;
    QString receiveToDetectMsCol;
    QString toDetectToResultMsCol;
    if (!optionalIdentifier(profile.inspectionItem.cameraName, &cameraNameCol, errorMessage)
        || !optionalIdentifier(profile.inspectionItem.defectName, &defectNameCol, errorMessage)
        || !optionalIdentifier(profile.inspectionItem.inferMs, &inferMsCol, errorMessage)
        || !optionalIdentifier(profile.inspectionItem.triggerToReceiveMs, &triggerToReceiveMsCol, errorMessage)
        || !optionalIdentifier(profile.inspectionItem.receiveToDetectMs, &receiveToDetectMsCol, errorMessage)
        || !optionalIdentifier(profile.inspectionItem.toDetectToResultMs, &toDetectToResultMsCol, errorMessage)) {
        return false;
    }
    if (cameraNameCol.isEmpty()) {
        cameraNameCol = cameraCol;
    }
    if (defectNameCol.isEmpty()) {
        defectNameCol = defectCol;
    }

    const QString startTime = requestedStartTime.trimmed().isEmpty()
        ? profile.defaults.startTime
        : requestedStartTime.trimmed();
    const QString endTime = requestedEndTime.trimmed().isEmpty()
        ? profile.defaults.endTime
        : requestedEndTime.trimmed();
    if (startTime.isEmpty() || endTime.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("time range is required; provide it in UI or profile analysis_defaults.default_time_range");
        }
        return false;
    }

    const QStringList rangeBindings = QStringList() << startTime << endTime;

    int totalCount = 0;
    int ngCount = 0;
    const QString totalSql = QStringLiteral(
        "SELECT COUNT(*), SUM(CASE WHEN %1 = 0 THEN 1 ELSE 0 END) "
        "FROM %2 WHERE %3 BETWEEN ? AND ?")
        .arg(isOkCol, itemTable, timeCol);

    if (!db.query(totalSql, rangeBindings, [&](sqlite3_stmt* statement) {
            totalCount = SQLiteConnection::columnInt(statement, 0);
            ngCount = SQLiteConnection::columnInt(statement, 1);
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

    if (!db.query(cameraSql, rangeBindings, [&](sqlite3_stmt* statement) {
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
    bool defectDistributionUsesInstance = false;

    if (!profile.defectInstance.table.trimmed().isEmpty()) {
        const QString defectInstanceTable = requiredIdentifier(profile.defectInstance.table, "defect_instance.table", errorMessage);
        const QString defectInstanceTimeCol = requiredIdentifier(profile.defectInstance.time, "defect_instance.time", errorMessage);
        const QString defectInstanceCodeCol = requiredIdentifier(profile.defectInstance.defectCode, "defect_instance.defect_code", errorMessage);
        QString defectInstanceNameCol;
        if (defectInstanceTable.isEmpty() || defectInstanceTimeCol.isEmpty() || defectInstanceCodeCol.isEmpty()
            || !optionalIdentifier(profile.defectInstance.defectName, &defectInstanceNameCol, errorMessage)) {
            return false;
        }
        if (defectInstanceNameCol.isEmpty()) {
            defectInstanceNameCol = defectInstanceCodeCol;
        }

        const QString defectSql = QStringLiteral(
            "SELECT %1, %2, COUNT(*) "
            "FROM %3 WHERE %4 BETWEEN ? AND ? "
            "GROUP BY %1, %2 ORDER BY COUNT(*) DESC")
            .arg(defectInstanceCodeCol, defectInstanceNameCol, defectInstanceTable, defectInstanceTimeCol);

        if (!db.query(defectSql, rangeBindings, [&](sqlite3_stmt* statement) {
                DefectMetric metric;
                metric.code = SQLiteConnection::columnText(statement, 0);
                metric.name = SQLiteConnection::columnText(statement, 1);
                metric.count = SQLiteConnection::columnInt(statement, 2);
                defectMetrics.append(metric);
                return true;
            }, errorMessage)) {
            return false;
        }
        defectDistributionUsesInstance = true;
    } else {
        const QString defectSql = QStringLiteral(
            "SELECT %1, %2, COUNT(*) "
            "FROM %3 WHERE %4 BETWEEN ? AND ? AND %5 = 0 "
            "GROUP BY %1, %2 ORDER BY COUNT(*) DESC")
            .arg(defectCol, defectNameCol, itemTable, timeCol, isOkCol);

        if (!db.query(defectSql, rangeBindings, [&](sqlite3_stmt* statement) {
                DefectMetric metric;
                metric.code = SQLiteConnection::columnText(statement, 0);
                metric.name = SQLiteConnection::columnText(statement, 1);
                metric.count = SQLiteConnection::columnInt(statement, 2);
                defectMetrics.append(metric);
                return true;
            }, errorMessage)) {
            return false;
        }
    }

    NumericStats inferStats;
    NumericStats triggerToReceiveStats;
    NumericStats receiveToDetectStats;
    NumericStats toDetectToResultStats;
    if (!queryNumericStats(db, itemTable, timeCol, inferMsCol, startTime, endTime, &inferStats, errorMessage)
        || !queryNumericStats(db, itemTable, timeCol, triggerToReceiveMsCol, startTime, endTime, &triggerToReceiveStats, errorMessage)
        || !queryNumericStats(db, itemTable, timeCol, receiveToDetectMsCol, startTime, endTime, &receiveToDetectStats, errorMessage)
        || !queryNumericStats(db, itemTable, timeCol, toDetectToResultMsCol, startTime, endTime, &toDetectToResultStats, errorMessage)) {
        return false;
    }

    struct NgStreakState {
        int length = 0;
        QString startTime;
        QString endTime;
        QString lastCode;
    };
    struct NgStreakMetric {
        QString cameraId;
        int longest = 0;
        QString startTime;
        QString endTime;
        QString lastCode;
    };
    QMap<QString, NgStreakState> activeStreaks;
    QMap<QString, NgStreakMetric> bestStreaks;

    const QString streakSql = QStringLiteral(
        "SELECT %1, %2, %3, %4 FROM %5 "
        "WHERE %2 BETWEEN ? AND ? ORDER BY %1, %2")
        .arg(cameraCol, timeCol, isOkCol, defectCol, itemTable);

    if (!db.query(streakSql, rangeBindings, [&](sqlite3_stmt* statement) {
            const QString cameraId = SQLiteConnection::columnText(statement, 0);
            const QString detectTime = SQLiteConnection::columnText(statement, 1);
            const int isOk = SQLiteConnection::columnInt(statement, 2);
            const QString defectCode = SQLiteConnection::columnText(statement, 3);

            NgStreakState& state = activeStreaks[cameraId];
            if (isOk == 0) {
                if (state.length == 0) {
                    state.startTime = detectTime;
                }
                state.length += 1;
                state.endTime = detectTime;
                state.lastCode = defectCode;

                NgStreakMetric& best = bestStreaks[cameraId];
                if (state.length > best.longest) {
                    best.cameraId = cameraId;
                    best.longest = state.length;
                    best.startTime = state.startTime;
                    best.endTime = state.endTime;
                    best.lastCode = state.lastCode;
                }
            } else {
                state = NgStreakState();
            }
            return true;
        }, errorMessage)) {
        return false;
    }

    struct EventRow {
        QString time;
        QString level;
        QString type;
        QString source;
        QString message;
    };
    QVector<EventRow> events;
    int warnErrorCount = 0;

    const QString eventCountSql = QStringLiteral(
        "SELECT COUNT(*) FROM %1 WHERE %2 BETWEEN ? AND ? AND %3 IN ('WARN', 'ERROR')")
        .arg(eventTable, eventTimeCol, eventLevelCol);
    if (!db.query(eventCountSql, rangeBindings, [&](sqlite3_stmt* statement) {
            warnErrorCount = SQLiteConnection::columnInt(statement, 0);
            return true;
        }, errorMessage)) {
        return false;
    }

    QString eventSourceCol;
    if (!optionalIdentifier(profile.runtimeEvent.source, &eventSourceCol, errorMessage)) {
        return false;
    }
    const QString eventSourceExpr = eventSourceCol.isEmpty() ? QStringLiteral("''") : eventSourceCol;
    const QString eventSql = QStringLiteral(
        "SELECT %1, %2, %3, %4, %5 "
        "FROM %6 WHERE %1 BETWEEN ? AND ? AND %2 IN ('WARN', 'ERROR') "
        "ORDER BY %1 LIMIT 12")
        .arg(eventTimeCol, eventLevelCol, eventTypeCol, eventSourceExpr, eventMessageCol, eventTable);

    if (!db.query(eventSql, rangeBindings, [&](sqlite3_stmt* statement) {
            EventRow event;
            event.time = SQLiteConnection::columnText(statement, 0);
            event.level = SQLiteConnection::columnText(statement, 1);
            event.type = SQLiteConnection::columnText(statement, 2);
            event.source = SQLiteConnection::columnText(statement, 3);
            event.message = SQLiteConnection::columnText(statement, 4);
            events.append(event);
            return true;
        }, errorMessage)) {
        return false;
    }

    const double ngRate = totalCount > 0 ? static_cast<double>(ngCount) / static_cast<double>(totalCount) : 0.0;
    const int continuousNgWarn = profile.thresholds.continuousNgWarn > 0 ? profile.thresholds.continuousNgWarn : 5;

    QString report;
    report += QStringLiteral("真实日志统计分析报告\n");
    report += QStringLiteral("====================\n\n");
    report += QStringLiteral("用户问题：%1\n").arg(userQuestion.trimmed().isEmpty() ? QStringLiteral("(未输入)") : userQuestion.trimmed());
    report += QStringLiteral("数据库：%1\n").arg(QFileInfo(databasePath).absoluteFilePath());
    report += QStringLiteral("Profile：%1\n").arg(profile.profilePath);
    report += QStringLiteral("时间范围：%1 ~ %2\n\n").arg(startTime, endTime);

    report += QStringLiteral("总体指标\n");
    report += QStringLiteral("- 检测项总数：%1\n").arg(totalCount);
    report += QStringLiteral("- NG 检测项数：%1\n").arg(ngCount);
    report += QStringLiteral("- NG 率：%1\n").arg(percent(ngRate));
    report += QStringLiteral("- WARN/ERROR 运行事件数：%1\n\n").arg(warnErrorCount);

    report += QStringLiteral("按相机统计\n");
    QString bestCamera;
    double bestNgRate = 0.0;
    double secondNgRate = 0.0;
    if (cameraMetrics.isEmpty()) {
        report += QStringLiteral("- 该时间段没有检测数据\n");
    } else {
        for (int i = 0; i < cameraMetrics.size(); ++i) {
            const CameraMetric& metric = cameraMetrics.at(i);
            if (i == 0) {
                bestCamera = metric.cameraId;
                bestNgRate = metric.ngRate;
            } else if (i == 1) {
                secondNgRate = metric.ngRate;
            }
            report += QStringLiteral("- %1（%2）：总数 %3，NG %4，NG 率 %5\n")
                .arg(metric.cameraId, metric.cameraName)
                .arg(metric.total)
                .arg(metric.ng)
                .arg(percent(metric.ngRate));
        }
    }
    report += QStringLiteral("\n");

    report += defectDistributionUsesInstance
        ? QStringLiteral("缺陷实例分布\n")
        : QStringLiteral("检测项缺陷结果分布\n");
    if (defectMetrics.isEmpty()) {
        report += QStringLiteral("- 该时间段没有缺陷记录\n");
    } else {
        for (const DefectMetric& metric : defectMetrics) {
            report += QStringLiteral("- %1（%2）：%3\n").arg(metric.code, metric.name).arg(metric.count);
        }
    }
    report += QStringLiteral("\n");

    report += QStringLiteral("耗时统计\n");
    appendTimingStat(&report, QStringLiteral("推理耗时 infer_ms"), inferStats, profile.thresholds.inferP95MsWarn);
    appendTimingStat(&report, QStringLiteral("触发到取图 trigger_to_receive_ms"), triggerToReceiveStats, profile.thresholds.triggerToReceiveP95MsWarn);
    appendTimingStat(&report, QStringLiteral("取图到检测 receive_to_detect_ms"), receiveToDetectStats, profile.thresholds.receiveToDetectP95MsWarn);
    appendTimingStat(&report, QStringLiteral("检测到结果 to_detect_to_result_ms"), toDetectToResultStats, profile.thresholds.toDetectToResultP95MsWarn);
    report += QStringLiteral("\n");

    report += QStringLiteral("连续 NG 检查\n");
    bool hasLongStreak = false;
    for (auto it = bestStreaks.constBegin(); it != bestStreaks.constEnd(); ++it) {
        const NgStreakMetric& streak = it.value();
        if (streak.longest >= continuousNgWarn) {
            hasLongStreak = true;
            report += QStringLiteral("- 相机 %1 最长连续 NG %2 次，%3 ~ %4，末次缺陷 %5\n")
                .arg(streak.cameraId)
                .arg(streak.longest)
                .arg(streak.startTime, streak.endTime, streak.lastCode);
        }
    }
    if (!hasLongStreak) {
        report += QStringLiteral("- 未发现达到 %1 次阈值的连续 NG\n").arg(continuousNgWarn);
    }
    report += QStringLiteral("\n");

    report += QStringLiteral("运行事件 WARN/ERROR（最多显示 12 条）\n");
    if (events.isEmpty()) {
        report += QStringLiteral("- 未发现 WARN/ERROR 事件\n");
    } else {
        for (const EventRow& event : events) {
            const QString sourceText = event.source.isEmpty() ? QStringLiteral("-") : event.source;
            report += QStringLiteral("- [%1] %2 %3 %4：%5\n")
                .arg(event.time, event.level, event.type, sourceText, event.message);
        }
    }
    report += QStringLiteral("\n");

    report += QStringLiteral("初步异常判断\n");
    bool hasInference = false;
    if (totalCount == 0) {
        report += QStringLiteral("- 当前时间范围没有检测数据，建议先确认时间范围或日志导入是否完整。\n");
        hasInference = true;
    }
    if (ngRate >= profile.thresholds.ngRateWarn) {
        report += QStringLiteral("- 整体 NG 率 %1 高于阈值 %2，需要优先关注缺陷集中类型和相机分布。\n")
            .arg(percent(ngRate), percent(profile.thresholds.ngRateWarn));
        hasInference = true;
    }
    if (!bestCamera.isEmpty() && bestNgRate >= profile.thresholds.ngRateWarn) {
        report += QStringLiteral("- NG 率最高的相机是 %1（%2）。建议检查该相机视野、光源、取图稳定性和对应工位来料状态。\n")
            .arg(bestCamera, percent(bestNgRate));
        hasInference = true;
    }
    if (!bestCamera.isEmpty() && secondNgRate > 0.0 && bestNgRate / secondNgRate >= profile.thresholds.cameraNgRateRatioWarn) {
        report += QStringLiteral("- 相机 %1 的 NG 率相对第二高相机差异明显，满足相机间异常差异阈值 %2 倍。\n")
            .arg(bestCamera)
            .arg(profile.thresholds.cameraNgRateRatioWarn, 0, 'f', 1);
        hasInference = true;
    }
    if (inferStats.hasData() && inferStats.p95 >= profile.thresholds.inferP95MsWarn) {
        report += QStringLiteral("- 推理耗时 P95 偏高，可能与模型版本、GPU/CPU 负载或图像尺寸变化有关。\n");
        hasInference = true;
    }
    if (triggerToReceiveStats.hasData() && triggerToReceiveStats.p95 >= profile.thresholds.triggerToReceiveP95MsWarn) {
        report += QStringLiteral("- 触发到取图 P95 偏高，建议检查相机触发、曝光、采图线程和缓存队列。\n");
        hasInference = true;
    }
    if (receiveToDetectStats.hasData() && receiveToDetectStats.p95 >= profile.thresholds.receiveToDetectP95MsWarn) {
        report += QStringLiteral("- 取图到检测 P95 偏高，建议关注图像入队、预处理和检测任务调度。\n");
        hasInference = true;
    }
    if (toDetectToResultStats.hasData() && toDetectToResultStats.p95 >= profile.thresholds.toDetectToResultP95MsWarn) {
        report += QStringLiteral("- 检测到结果输出 P95 偏高，建议检查后处理、结果写库和 UI/PLC 通信链路。\n");
        hasInference = true;
    }
    if (hasLongStreak) {
        report += QStringLiteral("- 存在连续 NG，建议按相机和时间回看原图，确认是否为真实连续缺陷、光照漂移或定位偏移。\n");
        hasInference = true;
    }
    if (warnErrorCount > 0) {
        report += QStringLiteral("- 同时间段存在 WARN/ERROR 运行事件，可与 NG 高峰时间做进一步关联分析。\n");
        hasInference = true;
    }
    if (!hasInference) {
        report += QStringLiteral("- 当前统计未触发明显异常阈值；可继续缩小时间窗口或按相机/缺陷类型做交叉分析。\n");
    }

    *outReport = report;
    return true;
}
