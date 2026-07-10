#include "vi_agent_api.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QString lastAgentError()
{
    char buffer[2048] = {};
    vi_agent_getLastError(buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

void showResult(QWidget* parent, int code, const QString& action)
{
    if (code == VI_AGENT_OK) {
        return;
    }

    QMessageBox::warning(
        parent,
        QStringLiteral("VI Agent 调用失败"),
        QStringLiteral("%1 返回错误码 %2\n%3").arg(action).arg(code).arg(lastAgentError()));
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("VI Agent Demo Host"));
    window.resize(720, 480);

    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);

    auto* initJsonEdit = new QTextEdit;
    initJsonEdit->setPlainText(QStringLiteral(R"({
  "project_id": "wuliangye_line_a",
  "database_path": "database/real_log_vi_agent.db",
  "profile_path": "profiles/real_log_profile.json",
  "log_paths": ["app_20260613_180017.log"],
  "image_root": "D:/Device/Images",
  "model_endpoint": "https://example.com/v1",
  "ui_language": "zh-CN"
})"));
    layout->addWidget(initJsonEdit, 1);

    auto* buttonLayout = new QHBoxLayout;
    auto* initButton = new QPushButton(QStringLiteral("调用 init(json)"));
    auto* openButton = new QPushButton(QStringLiteral("打开 Agent 对话框"));
    auto* closeButton = new QPushButton(QStringLiteral("关闭 Agent 对话框"));
    auto* uninitButton = new QPushButton(QStringLiteral("调用 uninit()"));
    buttonLayout->addWidget(initButton);
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(closeButton);
    buttonLayout->addWidget(uninitButton);
    layout->addLayout(buttonLayout);

    window.setCentralWidget(central);

    QObject::connect(initButton, &QPushButton::clicked, &window, [&]() {
        const QByteArray json = initJsonEdit->toPlainText().toUtf8();
        showResult(&window, vi_agent_init(json.constData()), QStringLiteral("vi_agent_init"));
    });

    QObject::connect(openButton, &QPushButton::clicked, &window, [&]() {
        showResult(&window, vi_agent_openWindow(reinterpret_cast<void*>(window.winId())), QStringLiteral("vi_agent_openWindow"));
    });

    QObject::connect(closeButton, &QPushButton::clicked, &window, [&]() {
        showResult(&window, vi_agent_closeWindow(), QStringLiteral("vi_agent_closeWindow"));
    });

    QObject::connect(uninitButton, &QPushButton::clicked, &window, [&]() {
        showResult(&window, vi_agent_uninit(), QStringLiteral("vi_agent_uninit"));
    });

    const QByteArray startupJson = initJsonEdit->toPlainText().toUtf8();
    vi_agent_init(startupJson.constData());

    window.show();
    const int result = app.exec();
    vi_agent_uninit();
    return result;
}
