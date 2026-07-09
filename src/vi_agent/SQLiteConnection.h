#pragma once

#include <QString>
#include <QStringList>

#include <functional>

struct sqlite3;
struct sqlite3_stmt;

class SQLiteConnection {
public:
    SQLiteConnection() = default;
    ~SQLiteConnection();

    SQLiteConnection(const SQLiteConnection&) = delete;
    SQLiteConnection& operator=(const SQLiteConnection&) = delete;

    bool openReadOnly(const QString& databasePath, QString* errorMessage);
    void close();
    bool query(const QString& sql, const QStringList& bindings, const std::function<bool(sqlite3_stmt*)>& onRow, QString* errorMessage);

    static QString columnText(sqlite3_stmt* statement, int column);
    static int columnInt(sqlite3_stmt* statement, int column);
    static double columnDouble(sqlite3_stmt* statement, int column);

private:
    sqlite3* db_ = nullptr;
};

