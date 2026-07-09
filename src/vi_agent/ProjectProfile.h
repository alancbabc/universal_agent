#pragma once

#include <QString>

struct InspectionItemMapping {
    QString table;
    QString time;
    QString cameraId;
    QString cameraName;
    QString isOk;
    QString defectCode;
    QString defectName;
    QString inferMs;
};

struct RuntimeEventMapping {
    QString table;
    QString time;
    QString level;
    QString eventType;
    QString source;
    QString message;
};

struct AnalysisDefaults {
    QString startTime;
    QString endTime;
};

struct Thresholds {
    double ngRateWarn = 0.05;
    double cameraNgRateRatioWarn = 2.0;
};

class ProjectProfile {
public:
    QString profilePath;
    QString projectId;
    QString databasePath;
    InspectionItemMapping inspectionItem;
    RuntimeEventMapping runtimeEvent;
    AnalysisDefaults defaults;
    Thresholds thresholds;

    static bool loadFromFile(const QString& profilePath, ProjectProfile* outProfile, QString* errorMessage);
};

