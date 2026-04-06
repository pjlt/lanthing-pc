/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 */

#include "main_window_private.h"

#include "main_window_about_page.h"
#include "main_window_actions_binder.h"
#include "main_window_link_page.h"
#include "main_window_manager_page.h"
#include "main_window_navigator.h"
#include "main_window_settings_page.h"
#include "main_window_status_presenter.h"

MainWindow::MainWindow(const lt::GUI::Params& params, QWidget* parent)
    : QMainWindow(parent)
    , params_(params)
    , ui(new Ui_MainWindow)
    , actions_binder_(std::make_unique<MainWindowActionsBinder>())
    , navigator_(std::make_unique<MainWindowNavigator>(ui))
    , relay_validator_(QRegularExpression("relay:(.+?:[0-9]+?):(.+?):(.+?)")) {

    ui->setupUi(this);

    // 用纯C++替换Link页面，作为去.ui迁移样板。
    rebuildLinkPageInCode();

    qApp->installEventFilter(this);

    loadPixmap();

    // 版本号
    std::stringstream oss_ver;
    oss_ver << "v" << LT_VERSION_MAJOR << "." << LT_VERSION_MINOR << "." << LT_VERSION_PATCH;
    link_label_version_->setText(QString::fromStdString(oss_ver.str()));

    // 无边框
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    // 日志
#if defined(LT_WINDOWS)
    connect(ui->btnLog, &QPushButton::clicked,
            [this]() { ltlib::openFolder(std::string(getenv("APPDATA")) + "\\lanthing\\log\\"); });
#else
    ui->btnLog->setVisible(false);
#endif // !defined(LT_WINDOWS)

    // 调整"已复制"标签的SizePolicy，让它在隐藏的时候保持占位
    QSizePolicy retain = link_label_copied_->sizePolicy();
    retain.setRetainSizeWhenHidden(true);
    link_label_copied_->setSizePolicy(retain);
    link_label_copied_->hide();

    // 调整设备码输入样式
    history_device_ids_ = params.get_history_device_ids();
    QIcon pc_icon{":/res/png_icons/pc.png"};
    if (history_device_ids_.empty()) {
        link_cb_device_id_->addItem(pc_icon, "");
    }
    else {
        for (const auto& id : history_device_ids_) {
            link_cb_device_id_->addItem(pc_icon, QString::fromStdString(id));
        }
    }
    link_cb_device_id_->setValidator(new QIntValidator(100'000'000, 999'999'999, this));

    // 验证码
    QAction* lock_position = new QAction();
    lock_position->setIcon(QIcon(":/res/png_icons/lock.png"));
    link_ledit_access_token_->addAction(lock_position, QLineEdit::LeadingPosition);
    link_ledit_access_token_->setValidator(new AccesstokenValidator(this));
    connect(link_ledit_access_token_, &QLineEdit::textChanged, [this](const QString& text) {
        if (text.trimmed().isEmpty()) {
            params_.clear_last_access_token();
        }
    });
    if (!history_device_ids_.empty()) {
        std::string last_device_id = history_device_ids_.front();
        auto [id, token] = params.get_last_access_token();
        if (last_device_id == id) {
            link_ledit_access_token_->setText(QString::fromStdString(token));
        }
    }

    // 客户端指示器
    setupClientIndicators();

    // 用纯C++替换Settings页面，作为去.ui迁移样板。
    rebuildSettingsPageInCode();

    // '设置'页面
    setupSettingsPage();

    // 左下角状态栏
    status_presenter_ =
        std::make_unique<MainWindowStatusPresenter>(link_label_login_info_, link_label_controlled_info_);
    setLoginStatusInUIThread(lt::GUI::LoginStatus::Connecting);
#if LT_WINDOWS
    setServiceStatusInUIThread(lt::GUI::ServiceStatus::Down);
#else  // LT_WINDOWS
    QSizePolicy sp_retain = link_label_controlled_info_->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    link_label_controlled_info_->setSizePolicy(sp_retain);
    link_label_controlled_info_->hide();
#endif // LT_WINDOWS

    // 用纯C++替换Manager页面，作为去.ui迁移样板。
    rebuildManagerPageInCode();

    // 客户端表格
    addOrUpdateTrustedDevices();

    // 用纯C++替换About页面，作为去.ui迁移样板。
    rebuildAboutPageInCode();

    // 剪切板
    setupClipboard();

    // 其它回调
    setupOtherCallbacks();
}

MainWindow::~MainWindow() {
    delete ui;
}

bool MainWindow::isUiThread() const {
    return qApp->thread() == QThread::currentThread();
}

void MainWindow::postToUiThread(std::function<void()> callback) {
    if (isUiThread()) {
        callback();
        return;
    }

    QTimer* timer = new QTimer();
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [callback = std::move(callback), timer]() {
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* evt) {
    if (obj == ui->windowBar && evt->type() == QEvent::MouseButtonPress) {
        QMouseEvent* ev = static_cast<QMouseEvent*>(evt);
        if (ev->buttons() & Qt::LeftButton) {
            old_pos_ = ev->globalPosition();
        }
    }
    if (obj == ui->windowBar && evt->type() == QEvent::MouseMove) {
        QMouseEvent* ev = static_cast<QMouseEvent*>(evt);
        if (ev->buttons() & Qt::LeftButton) {
            const QPointF delta = ev->globalPosition() - old_pos_;
            move(x() + delta.x(), y() + delta.y());
            old_pos_ = ev->globalPosition();
        }
    }
    return QObject::eventFilter(obj, evt);
}

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

void MainWindow::setLoginStatus(lt::GUI::LoginStatus status) {
    postToUiThread([this, status]() { setLoginStatusInUIThread(status); });
}

void MainWindow::setServiceStatus(lt::GUI::ServiceStatus status) {
    postToUiThread([this, status]() { setServiceStatusInUIThread(status); });
}

void MainWindow::setDeviceID(int64_t device_id) {
    postToUiThread([this, device_id]() {
        device_id_ = device_id;
        std::string id = std::to_string(device_id);
        std::string id2;
        for (size_t i = 0; i < id.size(); i++) {
            id2.push_back(id[i]);
            if (i % 3 == 2 && i != id.size() - 1) {
                id2.push_back(' ');
            }
        }
        link_label_my_device_id_->setText(QString::fromStdString(id2));
    });
}

void MainWindow::setAccessToken(const std::string& access_token) {
    postToUiThread([this, access_token]() {
        access_token_text_ = access_token;
        if (token_showing_) {
            link_label_my_access_token_->setText(QString::fromStdString(access_token));
        }
    });
}

void MainWindow::onConfirmConnection(int64_t device_id) {
    postToUiThread([this, device_id]() {
        QMessageBox msgbox;
        msgbox.setWindowTitle(tr("New Connection"));
        std::string id_str = std::to_string(device_id);
        QString message = tr("Device %s is requesting connection");
        std::vector<char> buffer(128);
        snprintf(buffer.data(), buffer.size(), message.toStdString().c_str(), id_str.c_str());
        msgbox.setText(buffer.data());
        auto btn_accept = msgbox.addButton(tr("Accept"), QMessageBox::ButtonRole::YesRole);
        auto btn_accept_next_time =
            msgbox.addButton(tr("Accept, as well as next time"), QMessageBox::ButtonRole::YesRole);
        auto btn_reject = msgbox.addButton(tr("Reject"), QMessageBox::ButtonRole::RejectRole);
        msgbox.exec();
        auto clicked_btn = msgbox.clickedButton();
        lt::GUI::ConfirmResult result = lt::GUI::ConfirmResult::Reject;
        if (clicked_btn == btn_accept) {
            result = lt::GUI::ConfirmResult::Accept;
            LOG(INFO) << "User accept";
        }
        else if (clicked_btn == btn_accept_next_time) {
            result = lt::GUI::ConfirmResult::AcceptWithNextTime;
            LOG(INFO) << "User accept, as well as next time";
        }
        else if (clicked_btn == btn_reject) {
            result = lt::GUI::ConfirmResult::Reject;
            LOG(INFO) << "User reject";
        }
        else {
            result = lt::GUI::ConfirmResult::Reject;
            LOG(INFO) << "Unknown button, treat as reject";
        }
        params_.on_user_confirmed_connection(device_id, result);
    });
}

void MainWindow::onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    postToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::service2app::ConnectionStatus>(_msg);
        if (!peer_client_device_id_.has_value()) {
            LOG(WARNING) << "Received ConnectionStatus, but we are not serving any client, "
                            "received device_id:"
                         << msg->device_id();
            return;
        }
        if (peer_client_device_id_.value() != msg->device_id()) {
            LOG(WARNING) << "Received ClientStatus with " << msg->device_id()
                         << ", but we are serving " << peer_client_device_id_.value();
            return;
        }
        double Mbps = static_cast<double>(msg->bandwidth_bps()) / 1000. / 1000.;
        int32_t delay_ms = msg->delay_ms();
        p2p_ = msg->p2p();
        mouse_hit_time_ = msg->hit_mouse() ? ltlib::steady_now_ms() : mouse_hit_time_;
        keyboard_hit_time_ = msg->hit_keyboard() ? ltlib::steady_now_ms() : keyboard_hit_time_;
        gamepad_hit_time_ = msg->hit_gamepad() ? ltlib::steady_now_ms() : gamepad_hit_time_;
        std::ostringstream oss;
        oss << msg->device_id() << " " << delay_ms * 2 << "ms " << std::fixed
            << std::setprecision(1) << Mbps << "Mbps " << video_codec_ << " "
            << (p2p_ ? "P2P " : "Relay ") << (gpu_decode_ ? "GPU:" : "CPU:")
            << (gpu_encode_ ? "GPU " : "CPU ");
        link_label_client1_->setToolTip(QString::fromStdString(oss.str()));
    });
}

void MainWindow::onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    postToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::service2app::AcceptedConnection>(_msg);
        if (peer_client_device_id_.has_value()) {
            if (peer_client_device_id_.value() == msg->device_id()) {
                LOG(WARNING) << "Received same AccpetedConnection " << msg->device_id();
                return;
            }
            else {
                // 暂时只支持一个客户端
                LOGF(ERR,
                     "Received AcceptedConnection(%" PRId64
                     "), but we are serving another client(%" PRId64 ")",
                     msg->device_id(), peer_client_device_id_.value());
                return;
            }
        }
        enable_gamepad_ = msg->enable_gamepad();
        enable_keyboard_ = msg->enable_keyboard();
        enable_mouse_ = msg->enable_mouse();
        enable_audio_ = msg->enable_audio();
        gpu_encode_ = msg->gpu_encode();
        gpu_decode_ = msg->gpu_decode();
        p2p_ = msg->p2p();
        video_codec_ = ltproto::common::VideoCodecType_Name(msg->video_codec());
        // 为了防止protobuf名字冲突，未知类型定义为"UnknownVCT"(UnknownVideoCodecType)，但是这个名字展示给用户并不好看
        if (msg->video_codec() == ltproto::common::UnknownVCT) {
            video_codec_ = "?";
        }
        peer_client_device_id_ = msg->device_id();
        std::ostringstream oss;
        oss << msg->device_id() << " ?ms ?Mbps " << video_codec_ << " "
            << (p2p_ ? "P2P " : "Relay ") << (gpu_encode_ ? "GPU:" : "CPU:")
            << (gpu_decode_ ? "GPU " : "CPU ");
        link_label_client1_->setToolTip(QString::fromStdString(oss.str()));
        link_indicator1_->show();
        QTimer::singleShot(50, this, &MainWindow::onUpdateIndicator);
    });
}

void MainWindow::onDisconnectedConnection(int64_t device_id) {
    postToUiThread([this, device_id]() {
        if (!peer_client_device_id_.has_value()) {
            LOG(ERR) << "Received DisconnectedClient, but no connected client";
            return;
        }
        if (peer_client_device_id_.value() != device_id) {
            LOGF(ERR,
                 "Received DisconnectedClient, but device_id(%" PRId64
                 ") != peer_client_device_id_(%" PRId64 ")",
                 device_id, peer_client_device_id_.value());
            return;
        }
        link_indicator1_->hide();
        peer_client_device_id_ = std::nullopt;
        gpu_encode_ = false;
        gpu_decode_ = false;
        p2p_ = false;
        bandwidth_bps_ = false;
        video_codec_ = "?";
        enable_gamepad_ = false;
        enable_keyboard_ = false;
        enable_mouse_ = false;
    });
}

void MainWindow::errorMessageBox(const QString& message) {
    postToUiThread([message]() {
        QMessageBox msgbox;
        msgbox.setText(message);
        msgbox.setIcon(QMessageBox::Icon::Critical);
        msgbox.exec();
    });
}

void MainWindow::infoMessageBox(const QString& message) {
    postToUiThread([message]() {
        QMessageBox msgbox;
        msgbox.setText(message);
        msgbox.setIcon(QMessageBox::Icon::Information);
        msgbox.exec();
    });
}

void MainWindow::onNewVersion(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    postToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::server::NewVersion>(_msg);
        std::ostringstream oss;
        int64_t version = ltlib::combineVersion(msg->major(), msg->minor(), msg->patch());
        oss << "v" << msg->major() << "." << msg->minor() << "." << msg->patch();
        if (msg->force()) {
            // 强制更新
            QString message = tr("The new version %s has been released, this is a force update "
                                 "version, please download it from <a "
                                 "href='%s'>Github</a>.");
            std::vector<char> buffer(512);
            snprintf(buffer.data(), buffer.size(), message.toStdString().c_str(), oss.str().c_str(),
                     msg->url().c_str());
            QMessageBox msgbox;
            msgbox.setTextFormat(Qt::TextFormat::RichText);
            msgbox.setWindowTitle(tr("New Version"));
            msgbox.setText(buffer.data());
            msgbox.setStandardButtons(QMessageBox::Ok);
            msgbox.exec();
            ::exit(0);
        }
        QString message = tr("The new version %s has been released, please download it<br>from <a "
                             "href='%s'>Github</a>.");
        std::vector<char> buffer(512);
        snprintf(buffer.data(), buffer.size(), message.toStdString().c_str(), oss.str().c_str(),
                 msg->url().c_str());
        oss.clear();

        auto date = QDateTime::fromSecsSinceEpoch(msg->timestamp());
        QString details = tr("Version: ") % "v" % QString::number(msg->major()) % "." %
                          QString::number(msg->minor()) % "." % QString::number(msg->patch()) %
                          "\n\n" % tr("Released date: ") %
                          date.toLocalTime().toString("yyyy/MM/dd") % "\n\n" % tr("New features:") %
                          "\n";
        for (int i = 0; i < msg->features_size(); i++) {
            details = details % QString::number(i + 1) % ". " %
                      QString::fromStdString(msg->features().Get(i)) % "\n";
        }
        details = details % "\n" % tr("Bug fix:") % "\n";
        for (int i = 0; i < msg->bugfix_size(); i++) {
            details = details % QString::number(i + 1) % ". " %
                      QString::fromStdString(msg->bugfix().Get(i)) % "\n";
        }

        QMessageBox msgbox;
        msgbox.setTextFormat(Qt::TextFormat::RichText);
        msgbox.setWindowTitle(tr("New Version"));
        msgbox.setText(buffer.data());
        msgbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Ignore);
        msgbox.setDetailedText(details);
        int ret = msgbox.exec();
        switch (ret) {
        case QMessageBox::Ignore:
            params_.ignore_version(version);
            break;
        default:
            break;
        }
    });
}

void MainWindow::setLoginStatusInUIThread(lt::GUI::LoginStatus status) {
    if (status_presenter_ == nullptr) {
        LOG(ERR) << "Status presenter is null";
        return;
    }
    status_presenter_->setLoginStatus(status);
}

void MainWindow::setServiceStatusInUIThread(lt::GUI::ServiceStatus status) {
    if (status_presenter_ == nullptr) {
        LOG(ERR) << "Status presenter is null";
        return;
    }
    status_presenter_->setServiceStatus(status);
}

void MainWindow::switchToMainPage() {
    navigator_->switchToMainPage();
}

void MainWindow::switchToManagerPage() {
    navigator_->switchToManagerPage();
}

void MainWindow::switchToSettingPage() {
    navigator_->switchToSettingPage();
}

void MainWindow::switchToAboutPage() {
    navigator_->switchToAboutPage();
}

void MainWindow::setupClientIndicators() {
    QSizePolicy policy = link_indicator1_->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    link_indicator1_->setSizePolicy(policy);
    link_indicator1_->hide();
    link_indicator2_->hide();
    link_label_client1_->setToolTipDuration(1000 * 10);
    link_label_client1_->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(link_label_client1_, &QLabel::customContextMenuRequested, [this](const QPoint& pos) {
        QMenu* menu = new QMenu(this);

        QAction* gamepad = new QAction(gp_, tr("gamepad"), menu);
        QAction* keyboard = new QAction(kb_, tr("keyboard"), menu);
        QAction* mouse = new QAction(mouse_, tr("mouse"), menu);
        QAction* audio = new QAction(audio_, tr("audio"), menu);
        QAction* kick = new QAction(kick_, tr("kick"), menu);

        if (enable_gamepad_) {
            gamepad->setText(gamepad->text() + " √");
        }
        if (enable_keyboard_) {
            keyboard->setText(keyboard->text() + " √");
        }
        if (enable_mouse_) {
            mouse->setText(mouse->text() + " √");
        }
        if (enable_audio_) {
            audio->setText(audio->text() + " √");
        }

        connect(gamepad, &QAction::triggered, [this]() {
            enable_gamepad_ = !enable_gamepad_;
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(
                enable_gamepad_ ? ltproto::service2app::OperateConnection_Operation_EnableGamepad
                                : ltproto::service2app::OperateConnection_Operation_DisableGamepad);
            params_.on_operate_connection(msg);
        });
        connect(keyboard, &QAction::triggered, [this]() {
            enable_keyboard_ = !enable_keyboard_;
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(
                enable_keyboard_
                    ? ltproto::service2app::OperateConnection_Operation_EnableKeyboard
                    : ltproto::service2app::OperateConnection_Operation_DisableKeyboard);
            params_.on_operate_connection(msg);
        });
        connect(mouse, &QAction::triggered, [this]() {
            enable_mouse_ = !enable_mouse_;
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(
                enable_mouse_ ? ltproto::service2app::OperateConnection_Operation_EnableMouse
                              : ltproto::service2app::OperateConnection_Operation_DisableMouse);
            params_.on_operate_connection(msg);
        });
        connect(audio, &QAction::triggered, [this]() {
            enable_audio_ = !enable_audio_;
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(
                enable_audio_ ? ltproto::service2app::OperateConnection_Operation_EnableAudio
                              : ltproto::service2app::OperateConnection_Operation_DisableAudio);
            params_.on_operate_connection(msg);
        });
        connect(kick, &QAction::triggered, [this]() {
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(ltproto::service2app::OperateConnection_Operation_Kick);
            params_.on_operate_connection(msg);
        });

        menu->addAction(gamepad);
        menu->addAction(mouse);
        menu->addAction(keyboard);
        menu->addAction(audio);
        menu->addAction(kick);

        menu->exec(link_label_client1_->mapToGlobal(pos));
    });
}

QPushButton* MainWindow::indexToTabButton(int32_t index) {
    return navigator_->indexToTabButton(index);
}

void MainWindow::swapTabBtnStyleSheet(QPushButton* old_selected, QPushButton* new_selected) {
    MainWindowNavigator::swapTabBtnStyleSheet(old_selected, new_selected);
}

void MainWindow::onUpdateIndicator() {
    if (!peer_client_device_id_.has_value()) {
        return;
    }
    setPixmapForIndicator(enable_gamepad_, gamepad_hit_time_, link_label_gamepad1_, gp_white_,
                          gp_gray_, gp_red_, gp_green_);
    setPixmapForIndicator(enable_mouse_, mouse_hit_time_, link_label_mouse1_, mouse_white_,
                          mouse_gray_, mouse_red_, mouse_green_);
    setPixmapForIndicator(enable_keyboard_, keyboard_hit_time_, link_label_keyboard1_, kb_white_,
                          kb_gray_, kb_red_, kb_green_);
    QTimer::singleShot(50, this, &MainWindow::onUpdateIndicator);
}

int MainWindow::indexOfPageByObjectName(const QString& object_name) const {
    return navigator_->indexOfPageByObjectName(object_name);
}

void MainWindow::setPixmapForIndicator(bool enable, int64_t last_time, QLabel* label,
                                       const QPixmap& white, const QPixmap& gray,
                                       const QPixmap& red, const QPixmap& green) {
    constexpr int64_t kDurationMS = 100;
    const int64_t now = ltlib::steady_now_ms();
    if (enable) {
        if (now > last_time + kDurationMS) {
            label->setPixmap(white);
        }
        else {
            label->setPixmap(green);
        }
    }
    else {
        if (now > last_time + kDurationMS) {
            label->setPixmap(gray);
        }
        else {
            label->setPixmap(red);
        }
    }
}

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

void MainWindow::setClipboardText(const std::string& text) {
    postToUiThread([text]() {
        auto clipboard = QApplication::clipboard();
        auto text2 = QString::fromStdString(text);
        if (clipboard->text() == text2) {
            return;
        }
        clipboard->setText(QString::fromStdString(text));
    });
}

void MainWindow::setupClipboard() {
    auto clipboard = QApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, [clipboard, this]() {
        auto md = clipboard->mimeData();
        if (md == nullptr) {
            LOG(WARNING) << "Clipboard::mimeData is null";
            return;
        }
        for (auto& f : md->formats()) {
            if (f == "text/plain") {
                onClipboardPlainTextChanged();
                return;
            }
            else if (f == "application/x-qt-windows-mime;value=\"FileName\"") { // TODO:
                onClipboardFileChanged();
                return;
            }
        }
    });
}

void MainWindow::onClipboardPlainTextChanged() {
    auto clipboard = QApplication::clipboard();
    if (clipboard->text().isEmpty()) {
        return;
    }
    constexpr size_t kMaxSize = 128 * 1024;
    auto text = clipboard->text().toStdString();
    if (text.size() > kMaxSize) {
        LOG(INFO) << "Detected clipboard text change, but too large to send. size:" << text.size();
        return;
    }
    params_.on_clipboard_text(text);
}

void MainWindow::onClipboardFileChanged() {
    auto clipboard = QApplication::clipboard();
    std::string fullpath = clipboard->text().toStdString();
    auto pos = fullpath.find("file:///");
    if (pos != std::string::npos) {
        fullpath = fullpath.substr(8);
    }
    if (fullpath.empty()) {
        LOG(WARNING) << "Clipboard file path is empty";
        return;
    }

    std::filesystem::path path;
    try {
        auto u8path = reinterpret_cast<const char8_t*>(fullpath.c_str());
        path = std::filesystem::path{u8path};
    } catch (const std::exception& e) {
        LOG(WARNING) << "Construct std::filesystem::path from '" << fullpath << "' failed with "
                     << e.what();
        return;
    }
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }
    uint64_t file_size = 0;
    try {
        file_size = std::filesystem::file_size(path);
    } catch (const std::exception& e) {
        LOG(WARNING) << "std::filesystem::file_size(" << fullpath << ") failed with " << e.what();
        return;
    }
    if (file_size == 0) {
        LOG(INFO) << "Clipboard file size == 0";
        return;
    }
    params_.on_clipboard_file(fullpath, file_size);
}

void MainWindow::addOrUpdateTrustedDevice(int64_t device_id, int64_t time_s) {
    postToUiThread([this, device_id, time_s]() {
        addOrUpdateTrustedDevice(device_id, true, false, false, time_s);
    });
}

void MainWindow::addOrUpdateTrustedDevices() {
    auto trusted_devices = params_.get_trusted_devices();
    for (auto& device : trusted_devices) {
        addOrUpdateTrustedDevice(device.device_id, device.gamepad, device.mouse, device.keyboard,
                                 device.last_access_time_s);
    }
    // 简化逻辑，固定5秒查一次，不然内部要存很多状态
    QTimer::singleShot(5'000, this, &MainWindow::addOrUpdateTrustedDevices);
}

void MainWindow::addOrUpdateTrustedDevice(int64_t device_id, bool gamepad, bool mouse,
                                          bool keyboard, int64_t last_access_time) {
    if (trusted_devices_table_ == nullptr) {
        LOG(ERR) << "Trusted devices table is null";
        return;
    }

    int row = -1;
    for (int i = 0; i < trusted_devices_table_->rowCount(); i++) {
        if (trusted_devices_table_->item(i, 0)->data(Qt::DisplayRole).toLongLong() == device_id) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        row = trusted_devices_table_->rowCount();
        trusted_devices_table_->setRowCount(row + 1);
    }

    // id
    QTableWidgetItem* id_item = new QTableWidgetItem;
    trusted_devices_table_->setItem(row, 0, id_item);
    id_item->setData(Qt::DisplayRole, QVariant::fromValue(device_id));

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    const auto check_state_changed_signal = &QCheckBox::checkStateChanged;
#else
    const auto check_state_changed_signal = &QCheckBox::stateChanged;
#endif

    // gamepad
    QCheckBox* gamepad_item = new QCheckBox();
    gamepad_item->setChecked(gamepad);
    connect(gamepad_item, check_state_changed_signal, [this, gamepad_item, device_id]() {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Gamepad,
                                         gamepad_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 1, makeWidgetHCentered(gamepad_item));
    // mouse
    QCheckBox* mouse_item = new QCheckBox();
    mouse_item->setChecked(mouse);
    connect(mouse_item, check_state_changed_signal, [this, mouse_item, device_id]() {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Mouse,
                                         mouse_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 2, makeWidgetHCentered(mouse_item));
    // keyboard
    QCheckBox* keyboard_item = new QCheckBox();
    keyboard_item->setChecked(keyboard);
    connect(keyboard_item, check_state_changed_signal, [this, keyboard_item, device_id]() {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Keyboard,
                                         keyboard_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 3, makeWidgetHCentered(keyboard_item));
    // time
    QTableWidgetItem* time_item =
        new QTableWidgetItem(QDateTime::fromSecsSinceEpoch(last_access_time)
                                 .toLocalTime()
                                 .toString("yyyy.MM.dd hh:mm:ss"));
    trusted_devices_table_->setItem(row, 4, time_item);
    // delete
    QPushButton* delete_item = new QPushButton(tr("delete"));
    connect(delete_item, &QPushButton::clicked, [this, device_id]() {
        for (int i = 0; i < trusted_devices_table_->rowCount(); i++) {
            if (trusted_devices_table_->item(i, 0)->data(Qt::DisplayRole).toLongLong() ==
                device_id) {
                trusted_devices_table_->removeRow(i);
                break;
            }
        }
        params_.delete_trusted_device(device_id);
    });
    trusted_devices_table_->setCellWidget(row, 5, delete_item);
}

QWidget* MainWindow::makeWidgetHCentered(QWidget* input_widget) {
    QWidget* output_widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(output_widget);
    layout->addWidget(input_widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return output_widget;
}

