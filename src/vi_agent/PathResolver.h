#pragma once

#include <QString>

class PathResolver {
public:
    static QString resolveExistingFile(const QString& path, const QString& anchorFilePath = QString());
};

