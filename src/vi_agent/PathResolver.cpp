#include "PathResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

QString PathResolver::resolveExistingFile(const QString& path, const QString& anchorFilePath)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QFileInfo directInfo(trimmed);
    if (directInfo.isAbsolute() && directInfo.exists() && directInfo.isFile()) {
        return directInfo.canonicalFilePath();
    }

    QStringList baseDirs;
    baseDirs.append(QDir::currentPath());

    if (!anchorFilePath.trimmed().isEmpty()) {
        const QFileInfo anchorInfo(anchorFilePath);
        if (anchorInfo.exists()) {
            baseDirs.append(anchorInfo.absoluteDir().absolutePath());
        }
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        QDir dir(appDir);
        for (int i = 0; i < 6; ++i) {
            baseDirs.append(dir.absolutePath());
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    for (const QString& baseDir : baseDirs) {
        const QFileInfo candidate(QDir(baseDir).absoluteFilePath(trimmed));
        if (candidate.exists() && candidate.isFile()) {
            return candidate.canonicalFilePath();
        }
    }

    return trimmed;
}

