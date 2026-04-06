/*
 * BSD 3-Clause License
 */

#include "main_window_actions_binder.h"

#include "ui_main_window.h"

#include <QValidator>
#include <QtCore/qobject.h>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QWidget>

#include <app/views/gui.h>
#include <ltlib/logging.h>

void MainWindowActionsBinder::bind(const MainWindowActionsBindingContext& c) const {
    QObject::connect(c.ui->btnLinkTab, &QPushButton::pressed, c.owner, [c]() { c.switch_to_main_page(); });
    QObject::connect(c.ui->btnSettingsTab, &QPushButton::pressed, c.owner,
            [c]() { c.switch_to_setting_page(); });
    QObject::connect(c.ui->btnManagerTab, &QPushButton::pressed, c.owner,
            [c]() { c.switch_to_manager_page(); });
    QObject::connect(c.ui->btnAboutTab, &QPushButton::pressed, c.owner,
            [c]() { c.switch_to_about_page(); });
    QObject::connect(c.ui->btnMinimize, &QPushButton::clicked, c.owner, [c]() {
        if (auto* widget = qobject_cast<QWidget*>(c.owner)) {
            widget->setWindowState(Qt::WindowState::WindowMinimized);
        }
    });
    QObject::connect(c.ui->btnClose, &QPushButton::clicked, c.owner, [c]() {
        if (auto* widget = qobject_cast<QWidget*>(c.owner)) {
            widget->hide();
        }
    });

    QObject::connect(c.link_btn_copy, &QPushButton::pressed, c.owner, [c]() { c.on_copy_pressed(); });
    QObject::connect(c.link_btn_show_token, &QPushButton::pressed, c.owner,
            [c]() { c.on_show_token_pressed(); });
    QObject::connect(c.link_btn_refresh_token, &QPushButton::clicked, c.owner,
            [c]() { c.on_refresh_token_clicked(); });
    QObject::connect(c.link_btn_connect, &QPushButton::clicked, c.owner,
            [c]() { c.on_connect_btn_clicked(); });

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    const auto check_state_changed_signal = &QCheckBox::checkStateChanged;
#else
    const auto check_state_changed_signal = &QCheckBox::stateChanged;
#endif

    QObject::connect(c.settings_checkbox_service, check_state_changed_signal, c.owner, [c]() {
        c.params->enable_run_as_service(c.settings_checkbox_service->isChecked());
    });
    QObject::connect(c.settings_checkbox_refresh_password, check_state_changed_signal, c.owner, [c]() {
        c.params->enable_auto_refresh_access_token(c.settings_checkbox_refresh_password->isChecked());
    });
    QObject::connect(c.settings_checkbox_share_clipboard, check_state_changed_signal, c.owner, [c]() {
        c.params->enable_share_clipboard(c.settings_checkbox_share_clipboard->isChecked());
    });
    QObject::connect(c.settings_radio_windowed_fullscreen, &QRadioButton::toggled, c.owner,
            [c](bool is_windowed) { c.params->set_fullscreen_mode(is_windowed); });
    QObject::connect(c.settings_checkbox_tcp, check_state_changed_signal, c.owner,
            [c]() { c.params->enable_tcp(c.settings_checkbox_tcp->isChecked()); });
    QObject::connect(c.settings_ledit_relay, &QLineEdit::textChanged, c.owner, [c](const QString& _text) {
        if (_text.isEmpty()) {
            c.settings_btn_relay->setEnabled(true);
            return;
        }
        QString text = _text;
        int pos = text.length();
        QValidator::State state = c.relay_validator->validate(text, pos);
        c.settings_btn_relay->setEnabled(state == QValidator::State::Acceptable);
    });
    QObject::connect(c.settings_btn_relay, &QPushButton::clicked, c.owner, [c]() {
        c.settings_btn_relay->setEnabled(false);
        c.params->set_relay_server(c.settings_ledit_relay->text().trimmed().toStdString());
    });
    QObject::connect(c.settings_ledit_min_port, &QLineEdit::textChanged, c.owner, [c](const QString& _text) {
        if (_text.trimmed().isEmpty() && c.settings_ledit_max_port->text().trimmed().isEmpty()) {
            c.settings_btn_port_range->setEnabled(true);
            return;
        }
        if (_text.trimmed().isEmpty() || c.settings_ledit_max_port->text().trimmed().isEmpty()) {
            c.settings_btn_port_range->setEnabled(false);
            return;
        }
        int min_port = _text.trimmed().toInt();
        int max_port = c.settings_ledit_max_port->text().trimmed().toInt();
        c.settings_btn_port_range->setEnabled(min_port < max_port);
    });
    QObject::connect(c.settings_ledit_max_port, &QLineEdit::textChanged, c.owner, [c](const QString& _text) {
        if (_text.trimmed().isEmpty() && c.settings_ledit_min_port->text().trimmed().isEmpty()) {
            c.settings_btn_port_range->setEnabled(true);
            return;
        }
        if (_text.trimmed().isEmpty() || c.settings_ledit_min_port->text().trimmed().isEmpty()) {
            c.settings_btn_port_range->setEnabled(false);
            return;
        }
        int max_port = _text.trimmed().toInt();
        int min_port = c.settings_ledit_min_port->text().trimmed().toInt();
        c.settings_btn_port_range->setEnabled(min_port < max_port);
    });
    QObject::connect(c.settings_btn_port_range, &QPushButton::clicked, c.owner, [c]() {
        if (c.settings_ledit_min_port->text().trimmed().isEmpty() &&
            c.settings_ledit_max_port->text().trimmed().isEmpty()) {
            c.params->set_port_range(0, 0);
            c.settings_btn_port_range->setEnabled(false);
            return;
        }
        if (c.settings_ledit_min_port->text().trimmed().isEmpty() ||
            c.settings_ledit_max_port->text().trimmed().isEmpty()) {
            return;
        }
        int min_port = c.settings_ledit_min_port->text().trimmed().toInt();
        int max_port = c.settings_ledit_max_port->text().trimmed().toInt();
        if (min_port < max_port && min_port > 1024 && min_port < 65536 && max_port > 1025 &&
            max_port <= 65536) {
            c.params->set_port_range(min_port, max_port);
            c.settings_btn_port_range->setEnabled(false);
        }
    });
    QObject::connect(c.settings_ledit_max_mbps, &QLineEdit::textChanged, c.owner, [c](const QString& _text) {
        if (_text.trimmed().isEmpty()) {
            c.settings_btn_max_mbps->setEnabled(true);
            return;
        }
        int mbps = _text.trimmed().toInt();
        c.settings_btn_max_mbps->setEnabled(mbps >= 1 && mbps <= 100);
    });
    QObject::connect(c.settings_btn_max_mbps, &QPushButton::clicked, c.owner, [c]() {
        if (c.settings_ledit_max_mbps->text().trimmed().isEmpty()) {
            c.params->set_max_mbps(0);
            c.settings_btn_max_mbps->setEnabled(false);
        }
        else {
            int mbps = c.settings_ledit_max_mbps->text().trimmed().toInt();
            if (mbps >= 1 && mbps <= 100) {
                c.params->set_max_mbps(static_cast<uint32_t>(mbps));
                c.settings_btn_max_mbps->setEnabled(false);
            }
        }
    });
    QObject::connect(c.settings_ledit_ignored_nic, &QLineEdit::textChanged, c.owner,
            [c](const QString&) { c.settings_btn_ignored_nic->setEnabled(true); });
    QObject::connect(c.settings_btn_ignored_nic, &QPushButton::clicked, c.owner, [c]() {
        c.settings_btn_ignored_nic->setEnabled(false);
        c.params->set_ignored_nic(c.settings_ledit_ignored_nic->text().trimmed().toStdString());
    });
    QObject::connect(c.settings_ledit_red, &QLineEdit::textChanged, c.owner,
            [c](const QString& text) { c.on_status_color_changed(text); });
    QObject::connect(c.settings_ledit_green, &QLineEdit::textChanged, c.owner,
            [c](const QString& text) { c.on_status_color_changed(text); });
    QObject::connect(c.settings_ledit_blue, &QLineEdit::textChanged, c.owner,
            [c](const QString& text) { c.on_status_color_changed(text); });
    QObject::connect(c.settings_checkbox_overlay, check_state_changed_signal, c.owner,
            [c]() { c.params->set_show_overlay(c.settings_checkbox_overlay->isChecked()); });
    QObject::connect(c.settings_btn_status_color, &QPushButton::clicked, c.owner, [c]() {
        c.settings_btn_status_color->setEnabled(false);
        if (c.settings_ledit_red->text().isEmpty() && c.settings_ledit_green->text().isEmpty() &&
            c.settings_ledit_blue->text().isEmpty()) {
            c.params->set_status_color(-1);
        }
        else {
            uint32_t red = static_cast<uint32_t>(c.settings_ledit_red->text().trimmed().toInt());
            uint32_t green =
                static_cast<uint32_t>(c.settings_ledit_green->text().trimmed().toInt());
            uint32_t blue = static_cast<uint32_t>(c.settings_ledit_blue->text().trimmed().toInt());
            c.params->set_status_color((red << 24) | (green << 16) | (blue << 8));
        }
    });
    QObject::connect(c.settings_ledit_mouse_accel, &QLineEdit::textChanged, c.owner, [c](const QString&) {
        if (c.settings_ledit_mouse_accel->text().isEmpty()) {
            c.settings_btn_mouse_accel->setEnabled(true);
            return;
        }
        double accel = c.settings_ledit_mouse_accel->text().trimmed().toDouble();
        int64_t accel_int = static_cast<int64_t>(accel * 10);
        c.settings_btn_mouse_accel->setEnabled(accel_int >= 1 && accel_int <= 30);
    });
    QObject::connect(c.settings_btn_mouse_accel, &QPushButton::clicked, c.owner, [c]() {
        c.settings_btn_mouse_accel->setEnabled(false);
        if (c.settings_ledit_mouse_accel->text().isEmpty()) {
            c.params->set_rel_mouse_accel(0);
        }
        else {
            double accel = c.settings_ledit_mouse_accel->text().trimmed().toDouble();
            int64_t accel_int = static_cast<int64_t>(accel * 10);
            if (accel_int >= 1 && accel_int <= 30) {
                c.params->set_rel_mouse_accel(accel_int);
            }
            else {
                LOG(ERR) << "Set relative mouse accel '"
                         << c.settings_ledit_mouse_accel->text().toStdString() << "' failed";
            }
        }
    });
}
