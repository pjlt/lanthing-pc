/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mainwindow.h"

#include <cassert>

#include "ui_MainWindow.h"

#include <QMouseEvent>
#include <QtCore/qtimer.h>
#include <QtWidgets/QLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qscrollarea.h>
#include <qclipboard.h>
#include <qmenu.h>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/times.h>
#include <ltproto/service2app/accepted_connection.pb.h>
#include <ltproto/service2app/connection_status.pb.h>
#include <ltproto/service2app/operate_connection.pb.h>

#include <views/components/access_token_validator.h>

namespace {

void dispatchToUiThread(std::function<void()> callback) {
    // any thread
    QTimer* timer = new QTimer();
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        // main thread
        callback();
        timer->deleteLater();
    });
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

QColor toColor(QString colorstr) {

    int r = colorstr.mid(1, 2).toInt(nullptr, 16);
    int g = colorstr.mid(3, 2).toInt(nullptr, 16);
    int b = colorstr.mid(5, 2).toInt(nullptr, 16);
    QColor color = QColor(r, g, b);
    return color;
}

} // namespace

MainWindow::MainWindow(const lt::GUI::Params& params, QWidget* parent)
    : QMainWindow(parent)
    , params_(params)
    , relay_validator_(QRegularExpression("relay:(.+?:[0-9]+?):(.+?):(.+?)"))
    , ui(new Ui_MainWindow) {

    ui->setupUi(this);

    qApp->installEventFilter(this);

    loadPixmap();

    // 无边框
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    // 调整"已复制"标签的SizePolicy，让它在隐藏的时候保持占位
    QSizePolicy retain = ui->labelCopied->sizePolicy();
    retain.setRetainSizeWhenHidden(true);
    ui->labelCopied->setSizePolicy(retain);
    ui->labelCopied->hide();

    // 登录进度条
    login_progress_ = new qt_componets::ProgressWidget();
    login_progress_->setVisible(false);
    login_progress_->setProgressColor(toColor("#8198ff"));

    // 调整设备码输入样式
    history_device_ids_ = params.get_history_device_ids();
    QIcon pc_icon{":/res/png_icons/pc.png"};
    if (history_device_ids_.empty()) {
        ui->cbDeviceID->addItem(pc_icon, "");
    }
    else {
        for (const auto& id : history_device_ids_) {
            ui->cbDeviceID->addItem(pc_icon, QString::fromStdString(id));
        }
    }

    // 验证码
    QAction* lock_position = new QAction();
    lock_position->setIcon(QIcon(":/res/png_icons/lock.png"));
    ui->leditAccessToken->addAction(lock_position, QLineEdit::LeadingPosition);
    ui->leditAccessToken->setValidator(new AccesstokenValidator(this));

    // 客户端指示器
    setupClientIndicators();

    // '设置'页面
    auto settings = params.get_settings();
    ui->checkboxService->setChecked(settings.run_as_daemon);
    ui->checkboxRefreshPassword->setChecked(settings.auto_refresh_access_token);
    ui->leditRelay->setText(QString::fromStdString(settings.relay_server));
    ui->btnRelay->setEnabled(false);
    if (settings.windowed_fullscreen.has_value()) {
        ui->radioRealFullscreen->setChecked(!settings.windowed_fullscreen.value());
        ui->radioWindowedFullscreen->setChecked(settings.windowed_fullscreen.value());
    }
    else {
        ui->radioRealFullscreen->setChecked(false);
        ui->radioWindowedFullscreen->setChecked(false);
    }

    // 其它回调
    setupOtherCallbacks();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::switchToMainPage() {
    if (ui->stackedWidget->currentIndex() != 0) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()), ui->btnLinkTab);
        ui->stackedWidget->setCurrentIndex(0);
    }
}

void MainWindow::switchToSettingPage() {
    if (ui->stackedWidget->currentIndex() != 1) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()),
                             ui->btnManagerTab);
        ui->stackedWidget->setCurrentIndex(1);
    }
}

void MainWindow::switchToManagerPage() {
    if (ui->stackedWidget->currentIndex() != 2) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()),
                             ui->btnSettingsTab);
        ui->stackedWidget->setCurrentIndex(2);
    }
}

void MainWindow::switchToAboutPage() {
    if (ui->stackedWidget->currentIndex() != 3) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()), ui->btnAboutTab);
        ui->stackedWidget->setCurrentIndex(3);
    }
}

void MainWindow::setLoginStatus(lt::GUI::ErrCode code) {
    dispatchToUiThread([this, code]() {
        switch (code) {
        case lt::GUI::ErrCode::OK:
            ui->loginStatusLayout->removeWidget(login_progress_);
            login_progress_->setVisible(false);
            ui->labelLoginInfo->setText(tr("Connected with server"));
            ui->labelLoginInfo->setStyleSheet("QLabel{}");
            break;
        case lt::GUI::ErrCode::CONNECTING:
            ui->loginStatusLayout->addWidget(login_progress_);
            login_progress_->setVisible(true);
            ui->labelLoginInfo->setStyleSheet("QLabel{}");
            break;
        case lt::GUI::ErrCode::FALIED:
        default:
            ui->loginStatusLayout->removeWidget(login_progress_);
            login_progress_->setVisible(false);
            ui->labelLoginInfo->setText(tr("Disconnected with server"));
            ui->labelLoginInfo->setStyleSheet("QLabel{color: red}");
            LOG(WARNING) << "Unknown LoginRet " << static_cast<int32_t>(code);
            break;
        }
    });
}

void MainWindow::setDeviceID(int64_t device_id) {
    dispatchToUiThread([this, device_id]() {
        std::string id = std::to_string(device_id);
        std::string id2;
        for (size_t i = 0; i < id.size(); i++) {
            id2.push_back(id[i]);
            if (i % 3 == 2 && i != id.size() - 1) {
                id2.push_back(' ');
            }
        }
        ui->labelMyDeviceID->setText(QString::fromStdString(id2));
    });
}

void MainWindow::setAccessToken(const std::string& access_token) {
    dispatchToUiThread([this, access_token]() {
        access_token_text_ = access_token;
        if (token_showing_) {
            ui->labelMyAccessToken->setText(QString::fromStdString(access_token));
        }
    });
}

void MainWindow::onConfirmConnection(int64_t device_id) {
    dispatchToUiThread([this, device_id]() {
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
    dispatchToUiThread([this, _msg]() {
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
        double Mbps = static_cast<double>(msg->bandwidth_bps()) / 1024. / 1024.;
        int32_t delay_ms = msg->delay_ms();
        p2p_ = msg->p2p();
        mouse_hit_time_ = msg->hit_mouse() ? ltlib::steady_now_ms() : mouse_hit_time_;
        keyboard_hit_time_ = msg->hit_keyboard() ? ltlib::steady_now_ms() : keyboard_hit_time_;
        gamepad_hit_time_ = msg->hit_gamepad() ? ltlib::steady_now_ms() : gamepad_hit_time_;
        std::ostringstream oss;
        oss << msg->device_id() << " " << delay_ms << "ms " << std::fixed << std::setprecision(1)
            << Mbps << "Mbps " << video_codec_ << " " << (p2p_ ? "P2P " : "Relay ")
            << (gpu_encode_ ? "GPU:" : "CPU:") << (gpu_decode_ ? "GPU " : "CPU ");
        ui->labelClient1->setToolTip(QString::fromStdString(oss.str()));
    });
}

void MainWindow::onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    dispatchToUiThread([this, _msg]() {
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
        ui->labelClient1->setToolTip(QString::fromStdString(oss.str()));
        ui->indicator1->show();
        QTimer::singleShot(50, this, &MainWindow::onUpdateIndicator);
    });
}

void MainWindow::onDisconnectedConnection(int64_t device_id) {
    dispatchToUiThread([this, device_id]() {
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
        ui->indicator1->hide();
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
    dispatchToUiThread([this, message]() {
        QMessageBox msgbox;
        msgbox.setText(message);
        msgbox.setIcon(QMessageBox::Icon::Critical);
        msgbox.exec();
    });
}

void MainWindow::infoMessageBox(const QString& message) {
    dispatchToUiThread([this, message]() {
        QMessageBox msgbox;
        msgbox.setText(message);
        msgbox.setIcon(QMessageBox::Icon::Information);
        msgbox.exec();
    });
}

bool MainWindow::eventFilter(QObject* obj, QEvent* evt) {
    if (obj == ui->windowBar && evt->type() == QEvent::MouseButtonPress) {
        QMouseEvent* ev = static_cast<QMouseEvent*>(evt);
        if (ev->buttons() & Qt::LeftButton) {

            // old_pos_ = ev->globalPosition() - ui->windowBar->geometry().topLeft();
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

void MainWindow::setupOtherCallbacks() {
    // 注意，有些按下就有效，有些要按下再释放
    connect(ui->btnLinkTab, &QPushButton::pressed, [this]() { switchToMainPage(); });
    connect(ui->btnSettingsTab, &QPushButton::pressed, [this]() { switchToSettingPage(); });
    connect(ui->btnManagerTab, &QPushButton::pressed, [this]() { switchToManagerPage(); });
    connect(ui->btnAboutTab, &QPushButton::pressed, [this]() { switchToAboutPage(); });
    connect(ui->btnMinimize, &QPushButton::clicked,
            [this]() { setWindowState(Qt::WindowState::WindowMinimized); });
    connect(ui->btnClose, &QPushButton::clicked, [this]() { hide(); });
    connect(ui->btnCopy, &QPushButton::pressed, this, &MainWindow::onCopyPressed);
    connect(ui->btnShowToken, &QPushButton::pressed, this, &MainWindow::onShowTokenPressed);
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectBtnClicked);
    connect(ui->checkboxService, &QCheckBox::stateChanged,
            [this](int) { params_.enable_run_as_service(ui->checkboxService->isChecked()); });
    connect(ui->checkboxRefreshPassword, &QCheckBox::stateChanged, [this](int) {
        params_.enable_auto_refresh_access_token(ui->checkboxRefreshPassword->isChecked());
    });
    connect(ui->radioWindowedFullscreen, &QRadioButton::toggled,
            [this](bool is_windowed) { params_.set_fullscreen_mode(is_windowed); });
    connect(ui->leditRelay, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.isEmpty()) {
            ui->btnRelay->setEnabled(true);
            return;
        }
        QString text = _text;
        int pos = text.length(); // -1; ???
        QValidator::State state = relay_validator_.validate(text, pos);
        ui->btnRelay->setEnabled(state == QValidator::State::Acceptable);
    });
    connect(ui->btnRelay, &QPushButton::clicked, [this]() {
        ui->btnRelay->setEnabled(false);
        params_.set_relay_server(ui->leditRelay->text().toStdString());
    });
}

void MainWindow::setupClientIndicators() {
    QSizePolicy policy = ui->indicator1->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    ui->indicator1->setSizePolicy(policy);
    ui->indicator1->hide();
    ui->indicator2->hide();
    ui->labelClient1->setToolTipDuration(1000 * 10);
    ui->labelClient1->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(ui->labelClient1, &QLabel::customContextMenuRequested, [this](const QPoint& pos) {
        QMenu* menu = new QMenu(this);

        QAction* gamepad = new QAction(gp_, tr("gamepad"), menu);
        QAction* keyboard = new QAction(kb_, tr("keyboard"), menu);
        QAction* mouse = new QAction(mouse_, tr("mouse"), menu);
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
        connect(kick, &QAction::triggered, [this]() {
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(ltproto::service2app::OperateConnection_Operation_Kick);
            params_.on_operate_connection(msg);
        });

        menu->addAction(gamepad);
        menu->addAction(mouse);
        menu->addAction(keyboard);
        menu->addAction(kick);

        menu->exec(ui->labelClient1->mapToGlobal(pos));
    });
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
}

QPushButton* MainWindow::indexToTabButton(int32_t index) {
    switch (index) {
    case 0:
        return ui->btnLinkTab;
    case 1:
        return ui->btnManagerTab;
    case 2:
        return ui->btnSettingsTab;
    case 3:
        return ui->btnAboutTab;
    default:
        // maybe fatal
        LOG(ERR) << "Unknown tab index!";
        return ui->btnLinkTab;
    };
}

void MainWindow::swapTabBtnStyleSheet(QPushButton* old_selected, QPushButton* new_selected) {
    QString stylesheet = new_selected->styleSheet();
    new_selected->setStyleSheet(old_selected->styleSheet());
    old_selected->setStyleSheet(stylesheet);
}

void MainWindow::onConnectBtnClicked() {
    auto dev_id = ui->cbDeviceID->currentText();
    auto token = ui->leditAccessToken->text().toStdString();
    int64_t deviceID = dev_id.toLongLong();
    if (deviceID != 0) {
        params_.connect(deviceID, token);
    }
    else {
        LOG(FATAL) << "Parse deviceID(" << dev_id.toStdString().c_str() << ") to int64_t failed!";
    }
}

void MainWindow::onShowTokenPressed() {
    if (token_showing_) {
        token_showing_ = false;
        ui->labelMyAccessToken->setText("••••••");
    }
    else {
        token_showing_ = true;
        token_last_show_time_ms_ = ltlib::steady_now_ms();
        ui->labelMyAccessToken->setText(QString::fromStdString(access_token_text_));
        QTimer::singleShot(5'100, std::bind(&MainWindow::onTimeoutHideToken, this));
    }
}

void MainWindow::onCopyPressed() {
    auto clipboard = QApplication::clipboard();
    QString device_id = ui->labelMyDeviceID->text();
    device_id = device_id.simplified();
    device_id.replace(" ", "");
    clipboard->setText(device_id);
    ui->labelCopied->show();
    QTimer::singleShot(2'000, [this]() { ui->labelCopied->hide(); });
}

void MainWindow::onUpdateIndicator() {
    if (!peer_client_device_id_.has_value()) {
        return;
    }
    setPixmapForIndicator(enable_gamepad_, gamepad_hit_time_, ui->labelGamepad1, gp_white_,
                          gp_gray_, gp_red_, gp_green_);
    setPixmapForIndicator(enable_mouse_, mouse_hit_time_, ui->labelMouse1, mouse_white_,
                          mouse_gray_, mouse_red_, mouse_green_);
    setPixmapForIndicator(enable_keyboard_, keyboard_hit_time_, ui->labelKeyboard1, kb_white_,
                          kb_gray_, kb_red_, kb_green_);
    QTimer::singleShot(50, this, &MainWindow::onUpdateIndicator);
}

void MainWindow::onTimeoutHideToken() {
    if (!token_showing_) {
        return;
    }
    int64_t now_ms = ltlib::steady_now_ms();
    if (token_last_show_time_ms_ + 5'000 <= now_ms) {
        token_showing_ = false;
        ui->labelMyAccessToken->setText("••••••");
    }
    else {
        QTimer::singleShot(token_last_show_time_ms_ + 5'100 - now_ms,
                           std::bind(&MainWindow::onTimeoutHideToken, this));
    }
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
