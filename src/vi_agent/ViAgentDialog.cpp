#include "ViAgentDialog.h"

#include "MetricsRepository.h"
#include "ProjectProfile.h"

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
    resize(980, 700);

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

    auto* timeLayout = new QHBoxLayout;
    startTimeEdit_ = new QLineEdit;
    endTimeEdit_ = new QLineEdit;
    startTimeEdit_->setPlaceholderText(QStringLiteral("2026-06-14 02:45:33.000"));
    endTimeEdit_->setPlaceholderText(QStringLiteral("2026-06-14 08:32:18.000"));
    timeLayout->addWidget(new QLabel(QStringLiteral("开始时间")));
    timeLayout->addWidget(startTimeEdit_, 1);
    timeLayout->addWidget(new QLabel(QStringLiteral("结束时间")));
    timeLayout->addWidget(endTimeEdit_, 1);
    queryLayout->addLayout(timeLayout);

    auto* questionEdit = new QLineEdit;
    questionEdit->setPlaceholderText(QStringLiteral("例如：分析这个时间段各相机 NG 率是否异常"));
    queryLayout->addWidget(questionEdit);

    auto* buttonLayout = new QHBoxLayout;
    auto* statusButton = new QPushButton(QStringLiteral("显示当前配置"));
    auto* analysisButton = new QPushButton(QStringLiteral("分析当前时间段"));
    buttonLayout->addWidget(statusButton);
    buttonLayout->addWidget(analysisButton);
    buttonLayout->addStretch();
    queryLayout->addLayout(buttonLayout);
    rootLayout->addWidget(queryGroup);

    resultView_ = new QTextEdit;
    resultView_->setReadOnly(true);
    resultView_->setPlaceholderText(QStringLiteral("这里展示统计结果、异常原因分析和证据链。"));
    rootLayout->addWidget(resultView_, 1);

    connect(statusButton, &QPushButton::clicked, this, [this]() {
        resultView_->setPlainText(formatConfigSummary(config_));
    });

    connect(analysisButton, &QPushButton::clicked, this, [this, questionEdit]() {
        const QString question = questionEdit->text().trimmed();
        const QString startTime = startTimeEdit_ ? startTimeEdit_->text().trimmed() : QString();
        const QString endTime = endTimeEdit_ ? endTimeEdit_->text().trimmed() : QString();
        QString report;
        QString errorMessage;
        if (!MetricsRepository::buildBasicReport(config_, question, startTime, endTime, &report, &errorMessage)) {
            resultView_->setPlainText(QStringLiteral("SQLite 统计分析失败：\n%1").arg(errorMessage));
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

    loadDefaultTimeRange();
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
    text += QStringLiteral("analysis_start_time: %1\n").arg(startTimeEdit_ ? startTimeEdit_->text().trimmed() : QString());
    text += QStringLiteral("analysis_end_time: %1\n").arg(endTimeEdit_ ? endTimeEdit_->text().trimmed() : QString());
    text += QStringLiteral("log_paths:\n");
    for (const QString& path : config.logPaths) {
        text += QStringLiteral("  - %1\n").arg(path);
    }
    if (config.logPaths.isEmpty()) {
        text += QStringLiteral("  (未配置)\n");
    }
    return text;
}

void ViAgentDialog::loadDefaultTimeRange()
{
    if (config_.profilePath.trimmed().isEmpty()) {
        return;
    }

    ProjectProfile profile;
    QString errorMessage;
    if (!ProjectProfile::loadFromFile(config_.profilePath, &profile, &errorMessage)) {
        return;
    }

    if (startTimeEdit_ && startTimeEdit_->text().trimmed().isEmpty()) {
        startTimeEdit_->setText(profile.defaults.startTime);
    }
    if (endTimeEdit_ && endTimeEdit_->text().trimmed().isEmpty()) {
        endTimeEdit_->setText(profile.defaults.endTime);
    }
}
