/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

void MainWindow::setupOtherCallbacks() {
    // 注意，有些按下就有效，有些要按下再释放
    connect(ui->btnLinkTab, &QPushButton::pressed, [this]() { switchToMainPage(); });
    connect(ui->btnSettingsTab, &QPushButton::pressed, [this]() { switchToSettingPage(); });
    connect(ui->btnManagerTab, &QPushButton::pressed, [this]() { switchToManagerPage(); });
    connect(ui->btnAboutTab, &QPushButton::pressed, [this]() { switchToAboutPage(); });
    connect(ui->btnMinimize, &QPushButton::clicked,
            [this]() { setWindowState(Qt::WindowState::WindowMinimized); });
    connect(ui->btnClose, &QPushButton::clicked, [this]() { hide(); });
    connect(link_btn_copy_, &QPushButton::pressed, this, &MainWindow::onCopyPressed);
    connect(link_btn_show_token_, &QPushButton::pressed, this, &MainWindow::onShowTokenPressed);
    connect(link_btn_refresh_token_, &QPushButton::clicked, this, &MainWindow::onRefreshTokenClicked);
    connect(link_btn_connect_, &QPushButton::clicked, this, &MainWindow::onConnectBtnClicked);
    connect(settings_checkbox_service_, &QCheckBox::stateChanged,
            [this](int) { params_.enable_run_as_service(settings_checkbox_service_->isChecked()); });
    connect(settings_checkbox_refresh_password_, &QCheckBox::stateChanged, [this](int) {
        params_.enable_auto_refresh_access_token(settings_checkbox_refresh_password_->isChecked());
    });
    connect(settings_checkbox_share_clipboard_, &QCheckBox::stateChanged, [this](int) {
        params_.enable_share_clipboard(settings_checkbox_share_clipboard_->isChecked());
    });
    connect(settings_radio_windowed_fullscreen_, &QRadioButton::toggled,
            [this](bool is_windowed) { params_.set_fullscreen_mode(is_windowed); });
    connect(settings_checkbox_tcp_, &QCheckBox::stateChanged,
            [this](int) { params_.enable_tcp(settings_checkbox_tcp_->isChecked()); });
    connect(settings_ledit_relay_, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.isEmpty()) {
            settings_btn_relay_->setEnabled(true);
            return;
        }
        QString text = _text;
        int pos = text.length(); // -1; ???
        QValidator::State state = relay_validator_.validate(text, pos);
        settings_btn_relay_->setEnabled(state == QValidator::State::Acceptable);
    });
    connect(settings_btn_relay_, &QPushButton::clicked, [this]() {
        settings_btn_relay_->setEnabled(false);
        params_.set_relay_server(settings_ledit_relay_->text().trimmed().toStdString());
    });
    connect(settings_ledit_min_port_, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.trimmed().isEmpty() && settings_ledit_max_port_->text().trimmed().isEmpty()) {
            settings_btn_port_range_->setEnabled(true);
            return;
        }
        if (_text.trimmed().isEmpty() || settings_ledit_max_port_->text().trimmed().isEmpty()) {
            settings_btn_port_range_->setEnabled(false);
            return;
        }
        int min_port = _text.trimmed().toInt();
        int max_port = settings_ledit_max_port_->text().trimmed().toInt();
        if (min_port >= max_port) {
            settings_btn_port_range_->setEnabled(false);
        }
        else {
            settings_btn_port_range_->setEnabled(true);
        }
    });
    connect(settings_ledit_max_port_, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.trimmed().isEmpty() && settings_ledit_min_port_->text().trimmed().isEmpty()) {
            settings_btn_port_range_->setEnabled(true);
            return;
        }
        if (_text.trimmed().isEmpty() || settings_ledit_min_port_->text().trimmed().isEmpty()) {
            settings_btn_port_range_->setEnabled(false);
            return;
        }
        int max_port = _text.trimmed().toInt();
        int min_port = settings_ledit_min_port_->text().trimmed().toInt();
        if (min_port >= max_port) {
            settings_btn_port_range_->setEnabled(false);
        }
        else {
            settings_btn_port_range_->setEnabled(true);
        }
    });
    connect(settings_btn_port_range_, &QPushButton::clicked, [this]() {
        if (settings_ledit_min_port_->text().trimmed().isEmpty() &&
            settings_ledit_max_port_->text().trimmed().isEmpty()) {
            params_.set_port_range(0, 0);
            settings_btn_port_range_->setEnabled(false);
            return;
        }
        if (settings_ledit_min_port_->text().trimmed().isEmpty() ||
            settings_ledit_max_port_->text().trimmed().isEmpty()) {
            return;
        }
        int min_port = settings_ledit_min_port_->text().trimmed().toInt();
        int max_port = settings_ledit_max_port_->text().trimmed().toInt();
        if (min_port < max_port && min_port > 1024 && min_port < 65536 && max_port > 1025 &&
            max_port <= 65536) {
            params_.set_port_range(min_port, max_port);
            settings_btn_port_range_->setEnabled(false);
        }
    });
    connect(settings_ledit_max_mbps_, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.trimmed().isEmpty()) {
            settings_btn_max_mbps_->setEnabled(true);
            return;
        }
        int mbps = _text.trimmed().toInt();
        if (mbps >= 1 && mbps <= 100) {
            settings_btn_max_mbps_->setEnabled(true);
        }
        else {
            settings_btn_max_mbps_->setEnabled(false);
        }
    });
    connect(settings_btn_max_mbps_, &QPushButton::clicked, [this]() {
        if (settings_ledit_max_mbps_->text().trimmed().isEmpty()) {
            params_.set_max_mbps(0);
            settings_btn_max_mbps_->setEnabled(false);
        }
        else {
            int mbps = settings_ledit_max_mbps_->text().trimmed().toInt();
            if (mbps >= 1 && mbps <= 100) {
                params_.set_max_mbps(static_cast<uint32_t>(mbps));
                settings_btn_max_mbps_->setEnabled(false);
            }
        }
    });
    connect(settings_ledit_ignored_nic_, &QLineEdit::textChanged,
            [this](const QString&) { settings_btn_ignored_nic_->setEnabled(true); });
    connect(settings_btn_ignored_nic_, &QPushButton::clicked, [this]() {
        settings_btn_ignored_nic_->setEnabled(false);
        params_.set_ignored_nic(settings_ledit_ignored_nic_->text().trimmed().toStdString());
    });
    connect(settings_ledit_red_, &QLineEdit::textChanged,
            std::bind(&MainWindow::onLineEditStatusColorChanged, this, std::placeholders::_1));
    connect(settings_ledit_green_, &QLineEdit::textChanged,
            std::bind(&MainWindow::onLineEditStatusColorChanged, this, std::placeholders::_1));
    connect(settings_ledit_blue_, &QLineEdit::textChanged,
            std::bind(&MainWindow::onLineEditStatusColorChanged, this, std::placeholders::_1));
    connect(settings_checkbox_overlay_, &QCheckBox::stateChanged,
            [this](int) { params_.set_show_overlay(settings_checkbox_overlay_->isChecked()); });
    connect(settings_btn_status_color_, &QPushButton::clicked, [this]() {
        settings_btn_status_color_->setEnabled(false);
        if (settings_ledit_red_->text().isEmpty() && settings_ledit_green_->text().isEmpty() &&
            settings_ledit_blue_->text().isEmpty()) {
            params_.set_status_color(-1);
        }
        else {
            uint32_t red = static_cast<uint32_t>(settings_ledit_red_->text().trimmed().toInt());
            uint32_t green = static_cast<uint32_t>(settings_ledit_green_->text().trimmed().toInt());
            uint32_t blue = static_cast<uint32_t>(settings_ledit_blue_->text().trimmed().toInt());
            params_.set_status_color((red << 24) | (green << 16) | (blue << 8));
        }
    });
    connect(settings_ledit_mouse_accel_, &QLineEdit::textChanged, [this](const QString&) {
        if (settings_ledit_mouse_accel_->text().isEmpty()) {
            settings_btn_mouse_accel_->setEnabled(true);
            return;
        }
        double accel = settings_ledit_mouse_accel_->text().trimmed().toDouble();
        int64_t accel_int = static_cast<int64_t>(accel * 10);
        if (accel_int >= 1 && accel_int <= 30) {
            settings_btn_mouse_accel_->setEnabled(true);
        }
        else {
            settings_btn_mouse_accel_->setEnabled(false);
        }
    });
    connect(settings_btn_mouse_accel_, &QPushButton::clicked, [this]() {
        settings_btn_mouse_accel_->setEnabled(false);
        if (settings_ledit_mouse_accel_->text().isEmpty()) {
            params_.set_rel_mouse_accel(0);
        }
        else {
            double accel = settings_ledit_mouse_accel_->text().trimmed().toDouble();
            int64_t accel_int = static_cast<int64_t>(accel * 10);
            if (accel_int >= 1 && accel_int <= 30) {
                params_.set_rel_mouse_accel(accel_int);
            }
            else {
                LOG(ERR) << "Set relative mouse accel '"
                         << settings_ledit_mouse_accel_->text().toStdString() << "' failed";
            }
        }
    });
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
