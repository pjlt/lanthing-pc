#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>

#include <QtWidgets/QMainWindow>

#include "ui.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace lt {
class App;
};

class Menu;
class MainPage;
class SettingPage;

class MainWindow : public QMainWindow, public lt::UiCallback {
    Q_OBJECT

public:
    MainWindow(lt::App* app, QWidget* parent = nullptr);
    ~MainWindow();
    void switchToMainPage();
    void switchToSettingPage();

protected:
    void closeEvent(QCloseEvent* ev) override;

protected:
    void onLoginRet(ErrCode code, const std::string& err = {}) override;
    void onInviteRet(ErrCode code, const std::string& err = {}) override;

    void onDisconnectedWithServer() override { ; }
    void onDevicesChanged(const std::vector<std::string>& dev_ids) override { (void)dev_ids; }

    void onLocalDeviceID(int64_t device_id) override;

    void onLocalAccessToken(const std::string& access_token) override;

private:
    void doInvite(const std::string& dev_id, const std::string& token);

private:
    Ui::MainWindow* ui;

    Menu* menu_ui = nullptr;
    MainPage* main_page_ui;
    SettingPage* setting_page_ui;

    std::function<void()> switch_to_main_page_;
    std::function<void()> switch_to_setting_page_;

    lt::App* app;
};
#endif // MAINWINDOW_H
