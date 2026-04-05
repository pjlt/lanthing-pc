/*
 * BSD 3-Clause License
 */

#include "main_window_settings_page.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

namespace {
QString trMainWindow(const char* text) {
    return QCoreApplication::translate("MainWindow", text);
}
} // namespace

MainWindowSettingsPageView MainWindowSettingsPage::createPage(QWidget* parent) const {
    auto* page_settings = new QWidget(parent);
    page_settings->setObjectName("pageSettings");
    page_settings->setStyleSheet("#pageSettings .QLineEdit {"
                                 "\n\tbackground-color: rgb(33, 37, 43);"
                                 "\n\tborder-radius: 5px;"
                                 "\n\tborder: 2px solid rgb(33, 37, 43);"
                                 "\n\tpadding-left: 10px;"
                                 "\n\tselection-color: rgb(255, 255, 255);"
                                 "\n\tselection-background-color: rgb(255, 121, 198);"
                                 "\n}"
                                 "\n#pageSettings .QLineEdit:hover {"
                                 "\n\tborder: 2px solid rgb(64, 71, 88);"
                                 "\n}"
                                 "\n#pageSettings .QLineEdit:focus {"
                                 "\n\tborder: 2px solid rgb(91, 101, 124);"
                                 "\n}"
                                 "\n"
                                 "\n#pageSettings .QPushButton {"
                                 "\n\tborder: 2px solid rgb(52, 59, 72);"
                                 "\n\tborder-radius: 5px;"
                                 "\n\tbackground-color: rgb(52, 59, 72);"
                                 "\n}"
                                 "\n#pageSettings .QPushButton:hover {"
                                 "\n\tbackground-color: rgb(57, 65, 80);"
                                 "\n\tborder: 2px solid rgb(61, 70, 86);"
                                 "\n}"
                                 "\n#pageSettings .QPushButton:pressed {"
                                 "\n\tbackground-color: rgb(35, 40, 49);"
                                 "\n\tborder: 2px solid rgb(43, 50, 61);"
                                 "\n}");

    auto* page_layout = new QVBoxLayout(page_settings);
    page_layout->setContentsMargins(30, 9, 9, 9);

    auto* scroll = new QScrollArea(page_settings);
    scroll->setWidgetResizable(true);
    auto* scroll_contents = new QWidget(scroll);
    scroll_contents->setMinimumSize(QSize(0, 850));
    auto* content_layout = new QVBoxLayout(scroll_contents);

    auto* gb_system = new QGroupBox(trMainWindow("System"), scroll_contents);
    auto* gb_system_layout = new QVBoxLayout(gb_system);
    gb_system_layout->setContentsMargins(9, 30, 9, 30);
    auto* checkbox_service = new QCheckBox(trMainWindow("Run as Service"), gb_system);
    auto* checkbox_refresh_password =
        new QCheckBox(trMainWindow("Auto refresh access token"), gb_system);
    auto* checkbox_share_clipboard = new QCheckBox(trMainWindow("Share Clipboard"), gb_system);
    gb_system_layout->addWidget(checkbox_service);
    gb_system_layout->addWidget(checkbox_refresh_password);
    gb_system_layout->addWidget(checkbox_share_clipboard);
    content_layout->addWidget(gb_system);

    auto* gb_mouse_mode =
        new QGroupBox(trMainWindow("Default Mouse Mode (Win+Shift+X)"), scroll_contents);
    auto* gb_mouse_mode_layout = new QVBoxLayout(gb_mouse_mode);
    gb_mouse_mode_layout->setContentsMargins(9, 30, 9, 30);
    auto* radio_absolute_mouse = new QRadioButton(trMainWindow("Absolute Mode"), gb_mouse_mode);
    auto* radio_relative_mouse = new QRadioButton(trMainWindow("Relative Mode"), gb_mouse_mode);
    gb_mouse_mode_layout->addWidget(radio_absolute_mouse);
    gb_mouse_mode_layout->addWidget(radio_relative_mouse);
    content_layout->addWidget(gb_mouse_mode);

    auto* gb_fullscreen = new QGroupBox(trMainWindow("Fullscreen Mode"), scroll_contents);
    auto* gb_fullscreen_layout = new QVBoxLayout(gb_fullscreen);
    gb_fullscreen_layout->setContentsMargins(9, 30, 9, 30);
    auto* radio_real_fullscreen =
        new QRadioButton(trMainWindow("Real Fullscreen"), gb_fullscreen);
    auto* radio_windowed_fullscreen =
        new QRadioButton(trMainWindow("Windowed Fullscreen"), gb_fullscreen);
    gb_fullscreen_layout->addWidget(radio_real_fullscreen);
    gb_fullscreen_layout->addWidget(radio_windowed_fullscreen);
    content_layout->addWidget(gb_fullscreen);

    auto* gb_transport = new QGroupBox(trMainWindow("Transport"), scroll_contents);
    auto* gb_transport_layout = new QVBoxLayout(gb_transport);
    gb_transport_layout->setContentsMargins(9, 30, 9, 30);
    auto* checkbox_tcp = new QCheckBox(trMainWindow("Enable TCP"), gb_transport);
    gb_transport_layout->addWidget(checkbox_tcp);
    auto* row_ports = new QHBoxLayout();
    auto* ledit_min_port = new QLineEdit(gb_transport);
    ledit_min_port->setPlaceholderText(trMainWindow("Min Port"));
    auto* ledit_max_port = new QLineEdit(gb_transport);
    ledit_max_port->setPlaceholderText(trMainWindow("Max Port"));
    auto* btn_port_range = new QPushButton(trMainWindow("Confirm"), gb_transport);
    row_ports->addWidget(ledit_min_port);
    row_ports->addWidget(ledit_max_port);
    row_ports->addWidget(btn_port_range);
    gb_transport_layout->addLayout(row_ports);
    content_layout->addWidget(gb_transport);

    auto* gb_network = new QGroupBox(trMainWindow("Network"), scroll_contents);
    auto* gb_network_layout = new QVBoxLayout(gb_network);
    gb_network_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_relay = new QHBoxLayout();
    auto* ledit_relay = new QLineEdit(gb_network);
    ledit_relay->setPlaceholderText(trMainWindow("relay:host:token:user"));
    auto* btn_relay = new QPushButton(trMainWindow("Confirm"), gb_network);
    row_relay->addWidget(ledit_relay);
    row_relay->addWidget(btn_relay);
    gb_network_layout->addLayout(row_relay);
    auto* row_nic = new QHBoxLayout();
    auto* ledit_ignored_nic = new QLineEdit(gb_network);
    ledit_ignored_nic->setPlaceholderText(trMainWindow("Ignored NIC list"));
    auto* btn_ignored_nic = new QPushButton(trMainWindow("Confirm"), gb_network);
    row_nic->addWidget(ledit_ignored_nic);
    row_nic->addWidget(btn_ignored_nic);
    gb_network_layout->addLayout(row_nic);
    content_layout->addWidget(gb_network);

    auto* gb_bandwidth = new QGroupBox(trMainWindow("Bandwidth"), scroll_contents);
    auto* gb_bandwidth_layout = new QVBoxLayout(gb_bandwidth);
    gb_bandwidth_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_mbps = new QHBoxLayout();
    auto* ledit_max_mbps = new QLineEdit(gb_bandwidth);
    ledit_max_mbps->setPlaceholderText(trMainWindow("Max Mbps (1-100)"));
    auto* btn_max_mbps = new QPushButton(trMainWindow("Confirm"), gb_bandwidth);
    row_mbps->addWidget(ledit_max_mbps);
    row_mbps->addWidget(btn_max_mbps);
    gb_bandwidth_layout->addLayout(row_mbps);
    content_layout->addWidget(gb_bandwidth);

    auto* gb_overlay = new QGroupBox(trMainWindow("Overlay"), scroll_contents);
    auto* gb_overlay_layout = new QVBoxLayout(gb_overlay);
    gb_overlay_layout->setContentsMargins(9, 30, 9, 30);
    auto* checkbox_overlay = new QCheckBox(trMainWindow("Show overlay"), gb_overlay);
    gb_overlay_layout->addWidget(checkbox_overlay);
    content_layout->addWidget(gb_overlay);

    auto* gb_status_color = new QGroupBox(trMainWindow("Status Color"), scroll_contents);
    auto* gb_status_color_layout = new QVBoxLayout(gb_status_color);
    gb_status_color_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_color = new QHBoxLayout();
    auto* ledit_red = new QLineEdit(gb_status_color);
    ledit_red->setPlaceholderText(QStringLiteral("R"));
    auto* ledit_green = new QLineEdit(gb_status_color);
    ledit_green->setPlaceholderText(QStringLiteral("G"));
    auto* ledit_blue = new QLineEdit(gb_status_color);
    ledit_blue->setPlaceholderText(QStringLiteral("B"));
    auto* btn_status_color = new QPushButton(trMainWindow("Confirm"), gb_status_color);
    row_color->addWidget(ledit_red);
    row_color->addWidget(ledit_green);
    row_color->addWidget(ledit_blue);
    row_color->addWidget(btn_status_color);
    gb_status_color_layout->addLayout(row_color);
    content_layout->addWidget(gb_status_color);

    auto* gb_mouse = new QGroupBox(trMainWindow("Relative Mouse Accel"), scroll_contents);
    auto* gb_mouse_layout = new QVBoxLayout(gb_mouse);
    gb_mouse_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_accel = new QHBoxLayout();
    auto* ledit_mouse_accel = new QLineEdit(gb_mouse);
    ledit_mouse_accel->setPlaceholderText(trMainWindow("0.1 - 3.0"));
    auto* btn_mouse_accel = new QPushButton(trMainWindow("Confirm"), gb_mouse);
    row_accel->addWidget(ledit_mouse_accel);
    row_accel->addWidget(btn_mouse_accel);
    gb_mouse_layout->addLayout(row_accel);
    content_layout->addWidget(gb_mouse);

    content_layout->addStretch(1);
    scroll->setWidget(scroll_contents);
    page_layout->addWidget(scroll);

    MainWindowSettingsPageView view;
    view.page = page_settings;
    view.checkbox_service = checkbox_service;
    view.checkbox_refresh_password = checkbox_refresh_password;
    view.checkbox_share_clipboard = checkbox_share_clipboard;
    view.radio_absolute_mouse = radio_absolute_mouse;
    view.radio_relative_mouse = radio_relative_mouse;
    view.ledit_relay = ledit_relay;
    view.btn_relay = btn_relay;
    view.radio_real_fullscreen = radio_real_fullscreen;
    view.radio_windowed_fullscreen = radio_windowed_fullscreen;
    view.checkbox_tcp = checkbox_tcp;
    view.ledit_min_port = ledit_min_port;
    view.ledit_max_port = ledit_max_port;
    view.btn_port_range = btn_port_range;
    view.ledit_ignored_nic = ledit_ignored_nic;
    view.btn_ignored_nic = btn_ignored_nic;
    view.ledit_max_mbps = ledit_max_mbps;
    view.btn_max_mbps = btn_max_mbps;
    view.checkbox_overlay = checkbox_overlay;
    view.ledit_red = ledit_red;
    view.ledit_green = ledit_green;
    view.ledit_blue = ledit_blue;
    view.btn_status_color = btn_status_color;
    view.ledit_mouse_accel = ledit_mouse_accel;
    view.btn_mouse_accel = btn_mouse_accel;
    return view;
}
