/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

#include "main_window_actions_binder.h"

void MainWindow::setupOtherCallbacks() {
    MainWindowActionsBindingContext context;
    context.owner = this;
    context.ui = ui;
    context.params = &params_;
    context.relay_validator = &relay_validator_;

    context.link_cb_device_id = link_cb_device_id_;
    context.link_btn_copy = link_btn_copy_;
    context.link_btn_show_token = link_btn_show_token_;
    context.link_btn_refresh_token = link_btn_refresh_token_;
    context.link_btn_connect = link_btn_connect_;

    context.settings_checkbox_service = settings_checkbox_service_;
    context.settings_checkbox_refresh_password = settings_checkbox_refresh_password_;
    context.settings_checkbox_share_clipboard = settings_checkbox_share_clipboard_;
    context.settings_radio_windowed_fullscreen = settings_radio_windowed_fullscreen_;
    context.settings_checkbox_tcp = settings_checkbox_tcp_;
    context.settings_ledit_relay = settings_ledit_relay_;
    context.settings_btn_relay = settings_btn_relay_;
    context.settings_ledit_min_port = settings_ledit_min_port_;
    context.settings_ledit_max_port = settings_ledit_max_port_;
    context.settings_btn_port_range = settings_btn_port_range_;
    context.settings_ledit_max_mbps = settings_ledit_max_mbps_;
    context.settings_btn_max_mbps = settings_btn_max_mbps_;
    context.settings_ledit_ignored_nic = settings_ledit_ignored_nic_;
    context.settings_btn_ignored_nic = settings_btn_ignored_nic_;
    context.settings_ledit_red = settings_ledit_red_;
    context.settings_ledit_green = settings_ledit_green_;
    context.settings_ledit_blue = settings_ledit_blue_;
    context.settings_checkbox_overlay = settings_checkbox_overlay_;
    context.settings_btn_status_color = settings_btn_status_color_;
    context.settings_ledit_mouse_accel = settings_ledit_mouse_accel_;
    context.settings_btn_mouse_accel = settings_btn_mouse_accel_;

    context.switch_to_main_page = [this]() { switchToMainPage(); };
    context.switch_to_setting_page = [this]() { switchToSettingPage(); };
    context.switch_to_manager_page = [this]() { switchToManagerPage(); };
    context.switch_to_about_page = [this]() { switchToAboutPage(); };
    context.on_copy_pressed = [this]() { onCopyPressed(); };
    context.on_show_token_pressed = [this]() { onShowTokenPressed(); };
    context.on_refresh_token_clicked = [this]() { onRefreshTokenClicked(); };
    context.on_connect_btn_clicked = [this]() { onConnectBtnClicked(); };
    context.on_status_color_changed = [this](const QString& text) {
        onLineEditStatusColorChanged(text);
    };

    actions_binder_->bind(context);
}

void MainWindow::onConnectBtnClicked() {
    auto dev_id = link_cb_device_id_->currentText();
    auto token = link_ledit_access_token_->text().trimmed().toStdString();
    int64_t deviceID = dev_id.toLongLong();
    if (deviceID < 100'000'000 || deviceID > 999'999'999 || token.empty()) {
        LOG(ERR) << "DeviceID(" << dev_id.toStdString().c_str() << ") invalid!";
        QMessageBox msgbox;
        msgbox.setText(tr("DeviceID or AccessToken invalid"));
        msgbox.setIcon(QMessageBox::Icon::Information);
        msgbox.exec();
    }
    else {
        params_.connect(deviceID, token);
    }
}

void MainWindow::onShowTokenPressed() {
    if (token_showing_) {
        token_showing_ = false;
        link_label_my_access_token_->setText("******");
    }
    else {
        token_showing_ = true;
        token_last_show_time_ms_ = ltlib::steady_now_ms();
        link_label_my_access_token_->setText(QString::fromStdString(access_token_text_));
        QTimer::singleShot(5'100, std::bind(&MainWindow::onTimeoutHideToken, this));
    }
}

void MainWindow::onRefreshTokenClicked() {
    params_.refresh_access_token();
    token_showing_ = true;
    token_last_show_time_ms_ = ltlib::steady_now_ms();
    link_label_my_access_token_->setText(QString::fromStdString(access_token_text_));
    QTimer::singleShot(5'100, std::bind(&MainWindow::onTimeoutHideToken, this));
}

void MainWindow::onCopyPressed() {
    auto clipboard = QApplication::clipboard();
    QString device_id = link_label_my_device_id_->text();
    device_id = device_id.simplified();
    device_id.replace(" ", "");
    clipboard->setText(device_id);
    link_label_copied_->show();
    QTimer::singleShot(2'000, [this]() { link_label_copied_->hide(); });
}

void MainWindow::onTimeoutHideToken() {
    if (!token_showing_) {
        return;
    }
    int64_t now_ms = ltlib::steady_now_ms();
    if (token_last_show_time_ms_ + 5'000 <= now_ms) {
        token_showing_ = false;
        link_label_my_access_token_->setText("******");
    }
    else {
        QTimer::singleShot(token_last_show_time_ms_ + 5'100 - now_ms,
                           std::bind(&MainWindow::onTimeoutHideToken, this));
    }
}

void MainWindow::onLineEditStatusColorChanged(const QString&) {
    if (settings_ledit_red_->text().isEmpty() && settings_ledit_green_->text().isEmpty() &&
        settings_ledit_blue_->text().isEmpty()) {
        settings_btn_status_color_->setEnabled(true);
        return;
    }
    if (settings_ledit_red_->text().isEmpty() || settings_ledit_green_->text().isEmpty() ||
        settings_ledit_blue_->text().isEmpty()) {
        settings_btn_status_color_->setEnabled(false);
        return;
    }
    uint32_t red = static_cast<uint32_t>(settings_ledit_red_->text().trimmed().toInt());
    uint32_t green = static_cast<uint32_t>(settings_ledit_green_->text().trimmed().toInt());
    uint32_t blue = static_cast<uint32_t>(settings_ledit_blue_->text().trimmed().toInt());
    if (red > 255 || green > 255 || blue > 255) {
        settings_btn_status_color_->setEnabled(false);
        return;
    }
    settings_btn_status_color_->setEnabled(true);
}
