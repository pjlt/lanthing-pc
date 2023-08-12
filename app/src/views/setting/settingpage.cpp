#include "settingpage.h"
#include "ui_settingpage.h"

SettingPage::SettingPage(const PreloadSettings& preload, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SettingPage)
    , relay_validator_(new QRegularExpressionValidator(
          QRegularExpression("relay:(.+?:[0-9]+?):(.+?):(.+?)"), this)) {
    ui->setupUi(parent);
    ui->checkbox_daemon->setChecked(preload.run_as_daemon);
    ui->checkbox_refresh_passwd->setChecked(preload.refresh_access_token);
    ui->lineedit_relay->setText(QString::fromStdString(preload.relay_server));
    ui->button_relay->setEnabled(false);

    connect(ui->checkbox_daemon, &QCheckBox::stateChanged,
            [this](int) { emit runAsDaemonStateChanged(ui->checkbox_daemon->isChecked()); });
    connect(ui->checkbox_refresh_passwd, &QCheckBox::stateChanged, [this](int) {
        emit refreshAccessTokenStateChanged(ui->checkbox_refresh_passwd->isChecked());
    });
    connect(ui->lineedit_relay, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.isEmpty()) {
            ui->button_relay->setEnabled(true);
            return;
        }
        QString text = _text;
        int pos = text.length(); // -1; ???
        QValidator::State state = relay_validator_->validate(text, pos);
        ui->button_relay->setEnabled(state == QValidator::State::Acceptable);
    });
    connect(ui->button_relay, &QPushButton::released, [this]() {
        ui->button_relay->setEnabled(false);
        emit relayServerChanged(ui->lineedit_relay->text().toStdString());
    });
}

SettingPage::~SettingPage() {
    delete ui;
    delete relay_validator_;
}
