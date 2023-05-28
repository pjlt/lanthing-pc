#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

protected:
    void onLoginRet(ErrCode code, const std::string& err = {}) override;
    void onInviteRet(ErrCode code, const std::string& err = {}) override;

    void onDisconnectedWithServer() override { ; }
    void onDevicesChanged(const std::vector<std::string>& dev_ids) { ; }

private:
    void doInvite(const std::string& dev_id, const std::string& token);

private:
    Ui::MainWindow* ui;

    Menu* menu_ui = nullptr;
    MainPage* main_page_ui;
    SettingPage* setting_page_ui;

    lt::App* app;
};
#endif // MAINWINDOW_H
