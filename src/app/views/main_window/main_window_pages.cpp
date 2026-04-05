/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

#include "main_window_about_page.h"
#include "main_window_link_page.h"
#include "main_window_manager_page.h"
#include "main_window_settings_page.h"

void MainWindow::rebuildLinkPageInCode() {
    if (ui->stackedWidget == nullptr) {
        return;
    }

    QWidget* old_page = nullptr;
    int link_index = -1;
    for (int i = 0; i < ui->stackedWidget->count(); i++) {
        QWidget* page = ui->stackedWidget->widget(i);
        if (page != nullptr && page->objectName() == QStringLiteral("pageLink")) {
            old_page = page;
            link_index = i;
            break;
        }
    }

    if (link_index < 0) {
        link_index = 0;
    }

    MainWindowLinkPage page_builder;
    MainWindowLinkPageView page_view = page_builder.createPage(ui->stackedWidget);
    QWidget* page_link = page_view.page;
    link_cb_device_id_ = page_view.cb_device_id;
    link_ledit_access_token_ = page_view.ledit_access_token;
    link_btn_connect_ = page_view.btn_connect;
    link_btn_copy_ = page_view.btn_copy;
    link_btn_show_token_ = page_view.btn_show_token;
    link_btn_refresh_token_ = page_view.btn_refresh_token;
    link_label_my_device_id_ = page_view.label_my_device_id;
    link_label_my_access_token_ = page_view.label_my_access_token;
    link_label_copied_ = page_view.label_copied;
    link_label_version_ = page_view.label_version;
    link_label_client1_ = page_view.label_client1;
    link_label_gamepad1_ = page_view.label_gamepad1;
    link_label_mouse1_ = page_view.label_mouse1;
    link_label_keyboard1_ = page_view.label_keyboard1;
    link_indicator1_ = page_view.indicator1;
    link_indicator2_ = page_view.indicator2;

    // 状态文本继续复用原有侧边栏控件，避免页面内重复显示。
    link_label_login_info_ = ui->labelLoginInfo;
    link_label_controlled_info_ = ui->labelControlledInfo;

    if (old_page != nullptr) {
        ui->stackedWidget->removeWidget(old_page);
        old_page->deleteLater();
    }
    ui->stackedWidget->insertWidget(link_index, page_link);
}

void MainWindow::rebuildSettingsPageInCode() {
    if (ui->stackedWidget == nullptr) {
        return;
    }

    QWidget* old_page = nullptr;
    int settings_index = -1;
    for (int i = 0; i < ui->stackedWidget->count(); i++) {
        QWidget* page = ui->stackedWidget->widget(i);
        if (page != nullptr && page->objectName() == QStringLiteral("pageSettings")) {
            old_page = page;
            settings_index = i;
            break;
        }
    }

    if (settings_index < 0) {
        settings_index = ui->stackedWidget->count();
    }

    MainWindowSettingsPage page_builder;
    MainWindowSettingsPageView page_view = page_builder.createPage(ui->stackedWidget);
    QWidget* page_settings = page_view.page;
    settings_checkbox_service_ = page_view.checkbox_service;
    settings_checkbox_refresh_password_ = page_view.checkbox_refresh_password;
    settings_checkbox_share_clipboard_ = page_view.checkbox_share_clipboard;
    settings_radio_absolute_mouse_ = page_view.radio_absolute_mouse;
    settings_radio_relative_mouse_ = page_view.radio_relative_mouse;
    settings_ledit_relay_ = page_view.ledit_relay;
    settings_btn_relay_ = page_view.btn_relay;
    settings_radio_real_fullscreen_ = page_view.radio_real_fullscreen;
    settings_radio_windowed_fullscreen_ = page_view.radio_windowed_fullscreen;
    settings_checkbox_tcp_ = page_view.checkbox_tcp;
    settings_ledit_min_port_ = page_view.ledit_min_port;
    settings_ledit_max_port_ = page_view.ledit_max_port;
    settings_btn_port_range_ = page_view.btn_port_range;
    settings_ledit_ignored_nic_ = page_view.ledit_ignored_nic;
    settings_btn_ignored_nic_ = page_view.btn_ignored_nic;
    settings_ledit_max_mbps_ = page_view.ledit_max_mbps;
    settings_btn_max_mbps_ = page_view.btn_max_mbps;
    settings_checkbox_overlay_ = page_view.checkbox_overlay;
    settings_ledit_red_ = page_view.ledit_red;
    settings_ledit_green_ = page_view.ledit_green;
    settings_ledit_blue_ = page_view.ledit_blue;
    settings_btn_status_color_ = page_view.btn_status_color;
    settings_ledit_mouse_accel_ = page_view.ledit_mouse_accel;
    settings_btn_mouse_accel_ = page_view.btn_mouse_accel;

    if (old_page != nullptr) {
        ui->stackedWidget->removeWidget(old_page);
        old_page->deleteLater();
    }
    ui->stackedWidget->insertWidget(settings_index, page_settings);
}

void MainWindow::rebuildManagerPageInCode() {
    if (ui->stackedWidget == nullptr) {
        return;
    }

    QWidget* old_page = nullptr;
    int manager_index = -1;
    for (int i = 0; i < ui->stackedWidget->count(); i++) {
        QWidget* page = ui->stackedWidget->widget(i);
        if (page != nullptr && page->objectName() == QStringLiteral("pageMgr")) {
            old_page = page;
            manager_index = i;
            break;
        }
    }

    if (manager_index < 0) {
        manager_index = ui->stackedWidget->count();
    }

    MainWindowManagerPage page_builder;
    MainWindowManagerPageView page_view = page_builder.createPage(ui->stackedWidget);
    QWidget* page_mgr = page_view.page;
    trusted_devices_table_ = page_view.trusted_devices_table;

    if (old_page != nullptr) {
        ui->stackedWidget->removeWidget(old_page);
        old_page->deleteLater();
    }
    ui->stackedWidget->insertWidget(manager_index, page_mgr);
}

void MainWindow::rebuildAboutPageInCode() {
    if (ui->stackedWidget == nullptr) {
        return;
    }

    QWidget* old_page = nullptr;
    int about_index = -1;
    for (int i = 0; i < ui->stackedWidget->count(); i++) {
        QWidget* page = ui->stackedWidget->widget(i);
        if (page != nullptr && page->objectName() == QStringLiteral("pageAbout")) {
            old_page = page;
            about_index = i;
            break;
        }
    }

    if (about_index < 0) {
        about_index = ui->stackedWidget->count();
    }

    MainWindowAboutPage page_builder;
    QWidget* page_about = page_builder.createPage(ui->stackedWidget);

    if (old_page != nullptr) {
        ui->stackedWidget->removeWidget(old_page);
        old_page->deleteLater();
    }
    ui->stackedWidget->insertWidget(about_index, page_about);
}

void MainWindow::setupSettingsPage() {
    auto settings = params_.get_settings();
    settings_checkbox_service_->hide();
    settings_checkbox_refresh_password_->setChecked(settings.auto_refresh_access_token);
#if defined(LT_WINDOWS)
    settings_checkbox_share_clipboard_->setChecked(settings.share_clipboard);
#else  // LT_WINDOWS
    settings_checkbox_share_clipboard_->hide();
#endif // LT_WINDOWS
    settings_radio_absolute_mouse_->setChecked(settings.absolute_mouse);
    settings_radio_relative_mouse_->setChecked(!settings.absolute_mouse);
    connect(settings_radio_absolute_mouse_, &QRadioButton::toggled,
            [this](bool is_absolute) { params_.set_absolute_mouse(is_absolute); });
    settings_ledit_relay_->setText(QString::fromStdString(settings.relay_server));
    settings_btn_relay_->setEnabled(false);
    if (settings.windowed_fullscreen.has_value()) {
        settings_radio_real_fullscreen_->setChecked(!settings.windowed_fullscreen.value());
        settings_radio_windowed_fullscreen_->setChecked(settings.windowed_fullscreen.value());
    }
    else {
        settings_radio_real_fullscreen_->setChecked(false);
        settings_radio_windowed_fullscreen_->setChecked(false);
    }
    settings_checkbox_tcp_->setChecked(settings.tcp);
    settings_btn_port_range_->setEnabled(false);
    settings_ledit_min_port_->setValidator(new QIntValidator(1025, 65536, this));
    settings_ledit_max_port_->setValidator(new QIntValidator(1025, 65536, this));
    if (settings.min_port != 0 && settings.max_port != 0) {
        settings_ledit_min_port_->setText(QString::number(settings.min_port));
        settings_ledit_max_port_->setText(QString::number(settings.max_port));
    }
    settings_btn_ignored_nic_->setEnabled(false);
    if (!settings.ignored_nic.empty()) {
        settings_ledit_ignored_nic_->setText(QString::fromStdString(settings.ignored_nic));
    }
    settings_btn_max_mbps_->setEnabled(false);
    settings_ledit_max_mbps_->setValidator(new QIntValidator(1, 100, this));
    if (settings.max_mbps != 0) {
        settings_ledit_max_mbps_->setText(QString::number(settings.max_mbps));
    }
    settings_checkbox_overlay_->setChecked(settings.show_overlay);
    settings_btn_status_color_->setEnabled(false);
    settings_ledit_red_->setValidator(new QIntValidator(0, 255, this));
    settings_ledit_green_->setValidator(new QIntValidator(0, 255, this));
    settings_ledit_blue_->setValidator(new QIntValidator(0, 255, this));
    if (settings.status_color.has_value()) {
        uint32_t color = settings.status_color.value();
        uint32_t red = (color & 0xff000000) >> 24;
        uint32_t green = (color & 0x00ff0000) >> 16;
        uint32_t blue = (color & 0x0000ff00) >> 8;
        settings_ledit_red_->setText(QString::number(red));
        settings_ledit_green_->setText(QString::number(green));
        settings_ledit_blue_->setText(QString::number(blue));
    }
    settings_btn_mouse_accel_->setEnabled(false);
    settings_ledit_mouse_accel_->setValidator(new QDoubleValidator(0.1, 3.0, 1, this));
    if (settings.rel_mouse_accel > 0 && settings.rel_mouse_accel <= 30) {
        double accel = settings.rel_mouse_accel / 10.0;
        settings_ledit_mouse_accel_->setText(QString::number(accel, 'f', 1));
    }
}

void MainWindow::loadPixmap() {
    copy_.load(":/res/png_icons/copy.png");
    eye_close_.load(":/res/png_icons/eye_close.png");
    eye_open_.load(":/res/png_icons/eye_open.png");

    kick_.load(":/res/png_icons/close.png");

    mouse_.load(":/res/png_icons/mouse.png");
    mouse_white_.load(":/res/png_icons/mouse_white.png");
    mouse_gray_.load(":/res/png_icons/mouse_gray.png");
    mouse_red_.load(":/res/png_icons/mouse_red.png");
    mouse_green_.load(":/res/png_icons/mouse_green.png");

    kb_.load(":/res/png_icons/keyboard.png");
    kb_white_.load(":/res/png_icons/keyboard_white.png");
    kb_gray_.load(":/res/png_icons/keyboard_gray.png");
    kb_red_.load(":/res/png_icons/keyboard_red.png");
    kb_green_.load(":/res/png_icons/keyboard_green.png");

    gp_.load(":/res/png_icons/gamepad.png");
    gp_white_.load(":/res/png_icons/gamepad_white.png");
    gp_gray_.load(":/res/png_icons/gamepad_gray.png");
    gp_red_.load(":/res/png_icons/gamepad_red.png");
    gp_green_.load(":/res/png_icons/gamepad_green.png");

    audio_.load(":/res/png_icons/audio.png");
}
