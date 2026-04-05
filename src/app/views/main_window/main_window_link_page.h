/*
 * BSD 3-Clause License
 */

#pragma once

class QWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QFrame;

struct MainWindowLinkPageView {
    QWidget* page = nullptr;
    QComboBox* cb_device_id = nullptr;
    QLineEdit* ledit_access_token = nullptr;
    QPushButton* btn_connect = nullptr;
    QPushButton* btn_copy = nullptr;
    QPushButton* btn_show_token = nullptr;
    QPushButton* btn_refresh_token = nullptr;
    QLabel* label_my_device_id = nullptr;
    QLabel* label_my_access_token = nullptr;
    QLabel* label_copied = nullptr;
    QLabel* label_version = nullptr;
    QLabel* label_client1 = nullptr;
    QLabel* label_gamepad1 = nullptr;
    QLabel* label_mouse1 = nullptr;
    QLabel* label_keyboard1 = nullptr;
    QFrame* indicator1 = nullptr;
    QFrame* indicator2 = nullptr;
};

class MainWindowLinkPage {
public:
    MainWindowLinkPageView createPage(QWidget* parent) const;
};
