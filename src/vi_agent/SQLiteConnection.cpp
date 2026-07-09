#include "SQLiteConnection.h"

#include <sqlite3.h>

namespace {

QString sqliteError(sqlite3* db)
{
    return db ? QString::fromUtf8(sqlite3_errmsg(db)) : QStringLiteral("sqlite database handle is null");
}

} // namespace

SQLiteConnection::~SQLiteConnection()
{
    close();
}

bool SQLiteConnection::openReadOnly(const QString& databasePath, QString* errorMessage)
{
    close();

    const QByteArray pathBytes = databasePath.toUtf8();
    const int rc = sqlite3_open_v2(pathBytes.constData(), &db_, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to open sqlite database: %1; path=%2")
                .arg(sqliteError(db_), databasePath);
        }
        close();
        return false;
    }

    sqlite3_busy_timeout(db_, 1500);
    return true;
}

void SQLiteConnection::close()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SQLiteConnection::query(
    const QString& sql,
    const QStringList& bindings,
    const std::function<bool(sqlite3_stmt*)>& onRow,
    QString* errorMessage)
{
    if (!db_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("sqlite database is not open");
        }
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    const QByteArray sqlBytes = sql.toUtf8();
    int rc = sqlite3_prepare_v2(db_, sqlBytes.constData(), -1, &statement, nullptr);
    if (rc != SQLITE_OK) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to prepare SQL: %1\nSQL: %2").arg(sqliteError(db_), sql);
        }
        return false;
    }

    for (int i = 0; i < bindings.size(); ++i) {
        const QByteArray value = bindings.at(i).toUtf8();
        rc = sqlite3_bind_text(statement, i + 1, value.constData(), value.size(), SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("failed to bind SQL parameter %1: %2").arg(i + 1).arg(sqliteError(db_));
            }
            sqlite3_finalize(statement);
            return false;
        }
    }

    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (onRow && !onRow(statement)) {
            break;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to execute SQL: %1\nSQL: %2").arg(sqliteError(db_), sql);
        }
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

QString SQLiteConnection::columnText(sqlite3_stmt* statement, int column)
{
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text ? QString::fromUtf8(reinterpret_cast<const char*>(text)) : QString();
}

int SQLiteConnection::columnInt(sqlite3_stmt* statement, int column)
{
    return sqlite3_column_int(statement, column);
}

double SQLiteConnection::columnDouble(sqlite3_stmt* statement, int column)
{
    return sqlite3_column_double(statement, column);
}

