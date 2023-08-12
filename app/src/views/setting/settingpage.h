#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <QValidator>
#include <QtWidgets/QWidget>

namespace Ui {
class SettingPage;
}

struct PreloadSettings {
    bool run_as_daemon;
    bool refresh_access_token;
    std::string relay_server;
};

class SettingPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingPage(const PreloadSettings& preload, QWidget* parent = nullptr);
    ~SettingPage();

Q_SIGNALS:
    void runAsDaemonStateChanged(bool run_as_daemon);
    void refreshAccessTokenStateChanged(bool auto_refresh);
    void relayServerChanged(const std::string& svr);

private:
    Ui::SettingPage* ui;
    QRegularExpressionValidator* relay_validator_;
};

#endif // SETTINGPAGE_H
