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
    QString triggerToReceiveMs;
    QString receiveToDetectMs;
    QString toDetectToResultMs;
};

struct DefectInstanceMapping {
    QString table;
    QString time;
    QString cameraId;
    QString defectCode;
    QString defectName;
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
    double inferP95MsWarn = 850.0;
    double triggerToReceiveP95MsWarn = 300.0;
    double receiveToDetectP95MsWarn = 50.0;
    double toDetectToResultP95MsWarn = 900.0;
    int continuousNgWarn = 5;
};

class ProjectProfile {
public:
    QString profilePath;
    QString projectId;
    QString databasePath;
    InspectionItemMapping inspectionItem;
    DefectInstanceMapping defectInstance;
    RuntimeEventMapping runtimeEvent;
    AnalysisDefaults defaults;
    Thresholds thresholds;

    static bool loadFromFile(const QString& profilePath, ProjectProfile* outProfile, QString* errorMessage);
};
