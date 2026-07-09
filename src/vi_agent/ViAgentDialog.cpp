#include "ViAgentDialog.h"

#include "MetricsRepository.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QLabel* createValueLabel()
{
    auto* label = new QLabel;
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

} // namespace

ViAgentDialog::ViAgentDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("VI Agent 日志数据分析"));
    resize(920, 640);

    auto* rootLayout = new QVBoxLayout(this);

    auto* configGroup = new QGroupBox(QStringLiteral("初始化配置"));
    auto* configLayout = new QFormLayout(configGroup);
    projectIdValue_ = createValueLabel();
    databasePathValue_ = createValueLabel();
    profilePathValue_ = createValueLabel();
    modelEndpointValue_ = createValueLabel();
    configLayout->addRow(QStringLiteral("项目 ID"), projectIdValue_);
    configLayout->addRow(QStringLiteral("数据库"), databasePathValue_);
    configLayout->addRow(QStringLiteral("Profile"), profilePathValue_);
    configLayout->addRow(QStringLiteral("模型服务"), modelEndpointValue_);
    rootLayout->addWidget(configGroup);

    auto* queryGroup = new QGroupBox(QStringLiteral("调试入口"));
    auto* queryLayout = new QVBoxLayout(queryGroup);
    auto* questionEdit = new QLineEdit;
    questionEdit->setPlaceholderText(QStringLiteral("例如：分析最近 1 小时各相机 NG 率是否异常"));
    auto* buttonLayout = new QHBoxLayout;
    auto* statusButton = new QPushButton(QStringLiteral("显示当前配置"));
    auto* placeholderButton = new QPushButton(QStringLiteral("测试分析流程"));
    buttonLayout->addWidget(statusButton);
    buttonLayout->addWidget(placeholderButton);
    buttonLayout->addStretch();
    queryLayout->addWidget(questionEdit);
    queryLayout->addLayout(buttonLayout);
    rootLayout->addWidget(queryGroup);

    resultView_ = new QTextEdit;
    resultView_->setReadOnly(true);
    resultView_->setPlaceholderText(QStringLiteral("后续这里展示统计结果、异常原因分析和证据链。"));
    rootLayout->addWidget(resultView_, 1);

    connect(statusButton, &QPushButton::clicked, this, [this]() {
        resultView_->setPlainText(formatConfigSummary(config_));
    });

    connect(placeholderButton, &QPushButton::clicked, this, [this, questionEdit]() {
        const QString question = questionEdit->text().trimmed();
        QString report;
        QString errorMessage;
        if (!MetricsRepository::buildBasicReport(config_, question, &report, &errorMessage)) {
            resultView_->setPlainText(QStringLiteral("SQLite 基础统计查询失败：\n%1").arg(errorMessage));
            return;
        }
        resultView_->setPlainText(report);
    });
}

void ViAgentDialog::setConfig(const AgentConfig& config)
{
    config_ = config;

    projectIdValue_->setText(config.projectId);
    databasePathValue_->setText(config.databasePath.isEmpty() ? QStringLiteral("(未配置)") : config.databasePath);
    profilePathValue_->setText(config.profilePath.isEmpty() ? QStringLiteral("(未配置)") : config.profilePath);
    modelEndpointValue_->setText(config.modelEndpoint.isEmpty() ? QStringLiteral("(未配置)") : config.modelEndpoint);
}

QString ViAgentDialog::formatConfigSummary(const AgentConfig& config) const
{
    QString text;
    text += QStringLiteral("VI Agent 当前初始化配置\n");
    text += QStringLiteral("========================\n\n");
    text += QStringLiteral("project_id: %1\n").arg(config.projectId);
    text += QStringLiteral("database_path: %1\n").arg(config.databasePath);
    text += QStringLiteral("profile_path: %1\n").arg(config.profilePath);
    text += QStringLiteral("image_root: %1\n").arg(config.imageRoot);
    text += QStringLiteral("model_endpoint: %1\n").arg(config.modelEndpoint);
    text += QStringLiteral("ui_language: %1\n").arg(config.uiLanguage);
    text += QStringLiteral("log_paths:\n");
    for (const QString& path : config.logPaths) {
        text += QStringLiteral("  - %1\n").arg(path);
    }
    if (config.logPaths.isEmpty()) {
        text += QStringLiteral("  (未配置)\n");
    }
    return text;
}

