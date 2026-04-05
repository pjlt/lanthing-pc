/*
 * BSD 3-Clause License
 */

#pragma once

#include <functional>

#include <app/views/gui.h>

#include <QString>

class QObject;
class QComboBox;
class QCheckBox;
class QRadioButton;
class QLineEdit;
class QPushButton;
class QRegularExpressionValidator;
class Ui_MainWindow;

struct MainWindowActionsBindingContext {
    QObject* owner = nullptr;
    Ui_MainWindow* ui = nullptr;
    lt::GUI::Params* params = nullptr;
    QRegularExpressionValidator* relay_validator = nullptr;

    QComboBox* link_cb_device_id = nullptr;
    QPushButton* link_btn_copy = nullptr;
    QPushButton* link_btn_show_token = nullptr;
    QPushButton* link_btn_refresh_token = nullptr;
    QPushButton* link_btn_connect = nullptr;

    QCheckBox* settings_checkbox_service = nullptr;
    QCheckBox* settings_checkbox_refresh_password = nullptr;
    QCheckBox* settings_checkbox_share_clipboard = nullptr;
    QRadioButton* settings_radio_windowed_fullscreen = nullptr;
    QCheckBox* settings_checkbox_tcp = nullptr;
    QLineEdit* settings_ledit_relay = nullptr;
    QPushButton* settings_btn_relay = nullptr;
    QLineEdit* settings_ledit_min_port = nullptr;
    QLineEdit* settings_ledit_max_port = nullptr;
    QPushButton* settings_btn_port_range = nullptr;
    QLineEdit* settings_ledit_max_mbps = nullptr;
    QPushButton* settings_btn_max_mbps = nullptr;
    QLineEdit* settings_ledit_ignored_nic = nullptr;
    QPushButton* settings_btn_ignored_nic = nullptr;
    QLineEdit* settings_ledit_red = nullptr;
    QLineEdit* settings_ledit_green = nullptr;
    QLineEdit* settings_ledit_blue = nullptr;
    QCheckBox* settings_checkbox_overlay = nullptr;
    QPushButton* settings_btn_status_color = nullptr;
    QLineEdit* settings_ledit_mouse_accel = nullptr;
    QPushButton* settings_btn_mouse_accel = nullptr;

    std::function<void()> switch_to_main_page;
    std::function<void()> switch_to_setting_page;
    std::function<void()> switch_to_manager_page;
    std::function<void()> switch_to_about_page;
    std::function<void()> on_copy_pressed;
    std::function<void()> on_show_token_pressed;
    std::function<void()> on_refresh_token_clicked;
    std::function<void()> on_connect_btn_clicked;
    std::function<void(const QString&)> on_status_color_changed;
};

class MainWindowActionsBinder {
public:
    void bind(const MainWindowActionsBindingContext& context) const;
};
