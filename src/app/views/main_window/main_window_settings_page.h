/*
 * BSD 3-Clause License
 */

#pragma once

class QWidget;
class QCheckBox;
class QRadioButton;
class QLineEdit;
class QPushButton;

struct MainWindowSettingsPageView {
    QWidget* page = nullptr;
    QCheckBox* checkbox_service = nullptr;
    QCheckBox* checkbox_refresh_password = nullptr;
    QCheckBox* checkbox_share_clipboard = nullptr;
    QRadioButton* radio_absolute_mouse = nullptr;
    QRadioButton* radio_relative_mouse = nullptr;
    QLineEdit* ledit_relay = nullptr;
    QPushButton* btn_relay = nullptr;
    QRadioButton* radio_real_fullscreen = nullptr;
    QRadioButton* radio_windowed_fullscreen = nullptr;
    QCheckBox* checkbox_tcp = nullptr;
    QLineEdit* ledit_min_port = nullptr;
    QLineEdit* ledit_max_port = nullptr;
    QPushButton* btn_port_range = nullptr;
    QLineEdit* ledit_ignored_nic = nullptr;
    QPushButton* btn_ignored_nic = nullptr;
    QLineEdit* ledit_max_mbps = nullptr;
    QPushButton* btn_max_mbps = nullptr;
    QCheckBox* checkbox_overlay = nullptr;
    QLineEdit* ledit_red = nullptr;
    QLineEdit* ledit_green = nullptr;
    QLineEdit* ledit_blue = nullptr;
    QPushButton* btn_status_color = nullptr;
    QLineEdit* ledit_mouse_accel = nullptr;
    QPushButton* btn_mouse_accel = nullptr;
};

class MainWindowSettingsPage {
public:
    MainWindowSettingsPageView createPage(QWidget* parent) const;
};
