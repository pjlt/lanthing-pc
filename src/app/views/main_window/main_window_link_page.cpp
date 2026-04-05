/*
 * BSD 3-Clause License
 */

#include "main_window_link_page.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace {
QString trMainWindow(const char* text) {
    return QCoreApplication::translate("MainWindow", text);
}
} // namespace

MainWindowLinkPageView MainWindowLinkPage::createPage(QWidget* parent) const {
    auto* page_link = new QWidget(parent);
    page_link->setObjectName("pageLink");
    page_link->setStyleSheet("\n"
                             "QComboBox#cbDeviceID {\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     border-radius: 5px;\n"
                             "     border: 2px solid rgb(33, 37, 43);\n"
                             "     padding-left: 10px;\n"
                             "     selection-color: rgb(255, 255, 255);\n"
                             "     selection-background-color: rgb(255, 121, 198);\n"
                             "}\n"
                             "QComboBox#cbDeviceID:hover {\n"
                             "     border: 2px solid rgb(64, 71, 88);\n"
                             "}\n"
                             "QComboBox#cbDeviceID:drop-down {\n"
                             "     subcontrol-origin: padding;\n"
                             "     subcontrol-position: top right;\n"
                             "     width: 25px;\n"
                             "     border-left-width: 3px;\n"
                             "     border-left-color: rgba(39, 44, 54, 150);\n"
                             "     border-left-style: solid;\n"
                             "     border-top-right-radius: 3px;\n"
                             "     border-bottom-right-radius: 3px;\n"
                             "     background-image: url(:/res/icons/cil-arrow-bottom.png);\n"
                             "     background-position: center;\n"
                             "     background-repeat: no-repeat;\n"
                             "}\n"
                             "QComboBox#cbDeviceID QAbstractItemView {\n"
                             "     color: rgb(255, 121, 198);\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     padding: 10px;\n"
                             "     selection-background-color: rgb(39, 44, 54);\n"
                             "}\n"
                             "#leditAccessToken {\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     border-radius: 5px;\n"
                             "     border: 2px solid rgb(33, 37, 43);\n"
                             "     padding-left: 10px;\n"
                             "     selection-color: rgb(255, 255, 255);\n"
                             "     selection-background-color: rgb(255, 121, 198);\n"
                             "}\n"
                             "#leditAccessToken:hover {\n"
                             "     border: 2px solid rgb(64, 71, 88);\n"
                             "}\n"
                             "#leditAccessToken:focus {\n"
                             "     border: 2px solid rgb(91, 101, 124);\n"
                             "}\n"
                             "QPushButton#btnConnect {\n"
                             "     border: 2px solid rgb(52, 59, 72);\n"
                             "     border-radius: 5px;\n"
                             "     background-color: rgb(52, 59, 72);\n"
                             "}\n"
                             "QPushButton#btnConnect:hover {\n"
                             "     background-color: rgb(57, 65, 80);\n"
                             "     border: 2px solid rgb(61, 70, 86);\n"
                             "}\n"
                             "QPushButton#btnConnect:pressed {\n"
                             "     background-color: rgb(35, 40, 49);\n"
                             "     border: 2px solid rgb(43, 50, 61);\n"
                             "}\n"
                             "#labelClient1 {\n"
                             "     border: none;\n"
                             "     background-color: transparent;\n"
                             "}\n"
                             "#labelClient1:hover {\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     border: 2px solid rgb(64, 71, 88);\n"
                             "}\n");
    auto* root_layout = new QVBoxLayout(page_link);
    root_layout->setSpacing(0);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setStretch(0, 3);
    root_layout->setStretch(1, 3);
    root_layout->setStretch(2, 1);
    root_layout->setStretch(3, 0);

    auto* frame_identity = new QFrame(page_link);
    frame_identity->setObjectName("linkRow1");
    auto* identity_layout = new QVBoxLayout(frame_identity);
    identity_layout->setSpacing(0);
    identity_layout->setContentsMargins(30, 9, 30, 9);
    auto* id_row = new QHBoxLayout();
    id_row->setSpacing(0);
    auto* label_device_id = new QLabel(trMainWindow("Device ID"), frame_identity);
    label_device_id->setMinimumSize(QSize(157, 71));
    label_device_id->setStyleSheet("font: 13pt;");
    id_row->addWidget(label_device_id);

    auto* label_my_device_id = new QLabel(frame_identity);
    label_my_device_id->setStyleSheet("font: 13pt;");
    id_row->addWidget(label_my_device_id);

    auto* btn_copy = new QPushButton(frame_identity);
    btn_copy->setObjectName("btnCopy");
    btn_copy->setMaximumWidth(50);
    btn_copy->setText(QString());
    btn_copy->setIcon(QIcon(":/res/icons/cil-clone.png"));
    id_row->addWidget(btn_copy);

    auto* label_copied = new QLabel(trMainWindow("Copied"), frame_identity);
    label_copied->setMinimumWidth(60);
    label_copied->setMaximumWidth(60);
    label_copied->setStyleSheet("font: 9pt;");
    id_row->addWidget(label_copied);
    identity_layout->addLayout(id_row);

    auto* token_row = new QHBoxLayout();
    token_row->setSpacing(0);
    auto* label_access_token = new QLabel(trMainWindow("Access Token"), frame_identity);
    label_access_token->setMinimumSize(QSize(157, 71));
    label_access_token->setStyleSheet("font: 13pt;");
    token_row->addWidget(label_access_token);

    auto* label_my_access_token = new QLabel(QStringLiteral("******"), frame_identity);
    label_my_access_token->setStyleSheet("font: 13pt;");
    token_row->addWidget(label_my_access_token);

    auto* btn_show_token = new QPushButton(frame_identity);
    btn_show_token->setObjectName("btnShowToken");
    btn_show_token->setMaximumWidth(50);
    btn_show_token->setText(QString());
    btn_show_token->setIcon(QIcon(":/res/icons/cil-low-vision.png"));
    token_row->addWidget(btn_show_token);

    auto* btn_refresh_token = new QPushButton(frame_identity);
    btn_refresh_token->setObjectName("btnRefreshToken");
    btn_refresh_token->setMaximumWidth(60);
    btn_refresh_token->setText(QString());
    btn_refresh_token->setIcon(QIcon(":/res/icons/cil-reload.png"));
    token_row->addWidget(btn_refresh_token);
    identity_layout->addLayout(token_row);
    root_layout->addWidget(frame_identity);

    auto* frame_connect = new QFrame(page_link);
    frame_connect->setObjectName("linkRow2");
    auto* connect_row = new QVBoxLayout(frame_connect);
    connect_row->setSpacing(6);
    connect_row->setContentsMargins(30, 0, 30, 0);

    auto* cb_device_id = new QComboBox(frame_connect);
    cb_device_id->setObjectName("cbDeviceID");
    cb_device_id->setEditable(true);
    cb_device_id->setMinimumHeight(50);

    auto* ledit_access_token = new QLineEdit(frame_connect);
    ledit_access_token->setObjectName("leditAccessToken");
    ledit_access_token->setMinimumHeight(45);
    ledit_access_token->setPlaceholderText(trMainWindow("Access token"));

    auto* btn_connect = new QPushButton(frame_connect);
    btn_connect->setObjectName("btnConnect");
    btn_connect->setMinimumHeight(40);
    btn_connect->setText(QString());
    btn_connect->setIcon(QIcon(":/res/icons/cil-link.png"));

    connect_row->addWidget(cb_device_id);
    connect_row->addWidget(ledit_access_token);
    connect_row->addWidget(btn_connect);
    root_layout->addWidget(frame_connect);

    auto* frame_clients = new QFrame(page_link);
    frame_clients->setObjectName("linkRow3");
    frame_clients->setStyleSheet("border:none;");
    auto* clients_row = new QHBoxLayout(frame_clients);
    clients_row->setSpacing(0);
    clients_row->setContentsMargins(30, 0, 30, 0);

    auto* indicator1 = new QFrame(frame_clients);
    indicator1->setObjectName("indicator1");
    indicator1->setStyleSheet("border:none;");
    auto* indicator_row = new QHBoxLayout(indicator1);
    indicator_row->setSpacing(0);
    indicator_row->setContentsMargins(0, 0, 0, 0);

    auto* label_client1 = new QLabel(indicator1);
    label_client1->setObjectName("labelClient1");
    label_client1->setPixmap(QPixmap(":/res/png_icons/pc2.png"));
    label_client1->setScaledContents(true);
    label_client1->setFixedSize(70, 70);
    indicator_row->addWidget(label_client1);

    auto* indicator_icons_container = new QFrame(indicator1);
    indicator_icons_container->setStyleSheet("border:none;");
    auto* indicator_icons = new QVBoxLayout(indicator_icons_container);
    indicator_icons->setSpacing(0);
    indicator_icons->setContentsMargins(0, 0, 0, 0);

    auto* label_gamepad1 = new QLabel(indicator1);
    auto* label_mouse1 = new QLabel(indicator1);
    auto* label_keyboard1 = new QLabel(indicator1);
    label_gamepad1->setPixmap(QPixmap(":/res/png_icons/gamepad_gray.png"));
    label_mouse1->setPixmap(QPixmap(":/res/png_icons/mouse_gray.png"));
    label_keyboard1->setPixmap(QPixmap(":/res/png_icons/keyboard_gray.png"));
    label_gamepad1->setScaledContents(true);
    label_mouse1->setScaledContents(true);
    label_keyboard1->setScaledContents(true);
    label_gamepad1->setFixedSize(20, 20);
    label_mouse1->setFixedSize(20, 20);
    label_keyboard1->setFixedSize(20, 20);
    label_gamepad1->setStyleSheet("border:none;");
    label_mouse1->setStyleSheet("border:none;");
    label_keyboard1->setStyleSheet("border:none;");
    indicator_icons->addWidget(label_gamepad1);
    indicator_icons->addWidget(label_mouse1);
    indicator_icons->addWidget(label_keyboard1);
    indicator_row->addWidget(indicator_icons_container);

    clients_row->addWidget(indicator1);
    clients_row->addStretch(1);

    auto* indicator2 = new QFrame(frame_clients);
    indicator2->setObjectName("indicator2");
    indicator2->setStyleSheet("border:none;");
    clients_row->addWidget(indicator2);
    root_layout->addWidget(frame_clients);

    auto* frame_status = new QFrame(page_link);
    frame_status->setObjectName("frame_20");
    frame_status->setStyleSheet("border: none; background-color: transparent;");
    auto* status_row = new QHBoxLayout(frame_status);
    status_row->setContentsMargins(30, 10, 30, 10);

    auto* label_version = new QLabel(frame_status);
    label_version->setObjectName("labelVersion");
    status_row->addStretch(1);
    status_row->addWidget(label_version);
    root_layout->addWidget(frame_status);

    MainWindowLinkPageView view;
    view.page = page_link;
    view.cb_device_id = cb_device_id;
    view.ledit_access_token = ledit_access_token;
    view.btn_connect = btn_connect;
    view.btn_copy = btn_copy;
    view.btn_show_token = btn_show_token;
    view.btn_refresh_token = btn_refresh_token;
    view.label_my_device_id = label_my_device_id;
    view.label_my_access_token = label_my_access_token;
    view.label_copied = label_copied;
    view.label_version = label_version;
    view.label_client1 = label_client1;
    view.label_gamepad1 = label_gamepad1;
    view.label_mouse1 = label_mouse1;
    view.label_keyboard1 = label_keyboard1;
    view.indicator1 = indicator1;
    view.indicator2 = indicator2;
    return view;
}
