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

#include "mainpage.h"

#include <sstream>
#include <string>

#include <QAction>
#include <QValidator>
#include <qclipboard.h>
#include <qicon.h>
#include <qmenu.h>
#include <qtimer.h>

#include <ltlib/logging.h>
#include <ltproto/service2app/accepted_connection.pb.h>
#include <ltproto/service2app/connection_status.pb.h>
#include <ltproto/service2app/operate_connection.pb.h>

#include "app.h"
#include "ui_mainpage.h"

namespace {

class AccesstokenValidator : public QValidator {
public:
    AccesstokenValidator(QWidget* parent);
    State validate(QString&, int&) const override;
    void fixup(QString&) const override;
};

AccesstokenValidator::AccesstokenValidator(QWidget* parent)
    : QValidator(parent) {}

QValidator::State AccesstokenValidator::validate(QString& input, int& pos) const {
    input = input.trimmed();
    if (input.length() > 6) {
        input.remove(6, input.length() - 6);
        pos = std::min(pos, 6);
    }
    size_t size = static_cast<size_t>(input.size());
    for (size_t i = 0; i < size; i++) {
        if ((input[i] >= 'a' && input[i] <= 'z') || (input[i] >= '0' && input[i] <= '9')) {
            continue;
        }
        if (input[i] >= 'A' && input[i] <= 'Z') {
            input[i] = input[i].toLower();
            continue;
        }
        return State::Invalid;
    }
    return State::Acceptable;
}

void AccesstokenValidator::fixup(QString& input) const {
    input = input.toUpper();
}

std::string to_string(lt::VideoCodecType codec) {
    switch (codec) {
    case lt::VideoCodecType::H264:
        return "AVC";
    case lt::VideoCodecType::H265:
        return "HEVC";
    default:
        return "Unknown";
    }
}

} // namespace

// TODO: 整个构造函数，语义化
MainPage::MainPage(const std::vector<std::string>& history_device_ids, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MainPage)
    , history_device_ids_(history_device_ids) {

    ui->setupUi(parent);

    loadPixmap();

    ui->label_copy_success->hide();
    copy_label_ = new ClickableLabel(this);
    copy_label_->setPixmap(copy_);
    copy_label_->setScaledContents(true);
    copy_label_->setFixedSize(QSize(24, 24));
    ui->vlayout_copy_hide->addWidget(copy_label_);
    connect(copy_label_, &ClickableLabel::clicked, this, &MainPage::onCopyClicked);

    show_token_label_ = new ClickableLabel(this);
    show_token_label_->setPixmap(eye_close_);
    show_token_label_->setScaledContents(true);
    show_token_label_->setFixedSize(QSize(24, 24));
    ui->vlayout_copy_hide->addWidget(show_token_label_);
    connect(show_token_label_, &ClickableLabel::clicked, this, &MainPage::onShowTokenClicked);

    QIcon pc_icon{":/icons/icons/pc.png"};
    if (history_device_ids_.empty()) {
        // ui->device_id->insertItem(0, "");
        // ui->device_id->setItemIcon(0, QIcon(":/icons/icons/pc.png"));
        ui->device_id->addItem(pc_icon, "");
    }
    else {
        for (const auto& id : history_device_ids_) {
            ui->device_id->addItem(pc_icon, QString::fromStdString(id));
        }
    }

    QAction* action2 = new QAction();
    action2->setIcon(QIcon(":/icons/icons/lock.png"));
    ui->access_token->addAction(action2, QLineEdit::LeadingPosition);

    ui->indicator->hide();
    ui->client_indicator->setToolTipDuration(1000 * 100);
    ui->client_indicator->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(ui->client_indicator, &QLabel::customContextMenuRequested, [this](const QPoint& pos) {
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
            emit onOperateConnection(msg);
        });
        connect(keyboard, &QAction::triggered, [this]() {
            enable_keyboard_ = !enable_keyboard_;
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(
                enable_keyboard_
                    ? ltproto::service2app::OperateConnection_Operation_EnableKeyboard
                    : ltproto::service2app::OperateConnection_Operation_DisableKeyboard);
            emit onOperateConnection(msg);
        });
        connect(mouse, &QAction::triggered, [this]() {
            enable_mouse_ = !enable_mouse_;
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(
                enable_mouse_ ? ltproto::service2app::OperateConnection_Operation_EnableMouse
                              : ltproto::service2app::OperateConnection_Operation_DisableMouse);
            emit onOperateConnection(msg);
        });
        connect(kick, &QAction::triggered, [this]() {
            auto msg = std::make_shared<ltproto::service2app::OperateConnection>();
            msg->add_operations(ltproto::service2app::OperateConnection_Operation_Kick);
            emit onOperateConnection(msg);
        });

        menu->addAction(gamepad);
        menu->addAction(keyboard);
        menu->addAction(mouse);
        menu->addAction(kick);

        menu->exec(ui->client_indicator->mapToGlobal(pos));
    });

    connect(ui->connect_btn, &QPushButton::pressed, [this]() { onConnectBtnPressed(); });
    ui->access_token->setValidator(new AccesstokenValidator(this));
}

MainPage::~MainPage() {
    delete ui;
}

void MainPage::onUpdateLocalDeviceID(int64_t device_id) {
    std::string id = std::to_string(device_id);
    std::string id2;
    for (size_t i = 0; i < id.size(); i++) {
        id2.push_back(id[i]);
        if (i % 3 == 2 && i != id.size() - 1) {
            id2.push_back(' ');
        }
    }
    ui->my_device_id->setText(QString::fromStdString(id2));
}

void MainPage::onUpdateLocalAccessToken(const std::string& access_token) {
    access_token_text_ = access_token;
    if (token_showing_) {
        ui->my_access_token->setText(QString::fromStdString(access_token));
    }
}

void MainPage::onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::service2app::ConnectionStatus>(_msg);
    if (!peer_client_device_id_.has_value()) {
        LOG(WARNING)
            << "Received ConnectionStatus, but we are not serving any client, received device_id:"
            << msg->device_id();
        return;
    }
    if (peer_client_device_id_.value() != msg->device_id()) {
        LOG(WARNING) << "Received ClientStatus with " << msg->device_id() << ", but we are serving "
                     << peer_client_device_id_.value();
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
        << Mbps << "Mbps " << to_string(video_codec_) << " " << (p2p_ ? "P2P " : "Relay ")
        << (gpu_encode_ ? "GPU:" : "CPU:") << (gpu_decode_ ? "GPU " : "CPU ");
    ui->client_indicator->setToolTip(QString::fromStdString(oss.str()));
}

void MainPage::onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
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
    video_codec_ = video_codec_ = lt::VideoCodecType::Unknown;
    peer_client_device_id_ = msg->device_id();
    std::ostringstream oss;
    oss << msg->device_id() << " ?ms ?Mbps " << to_string(video_codec_) << " "
        << (p2p_ ? "P2P " : "Relay ") << (gpu_encode_ ? "GPU:" : "CPU:")
        << (gpu_decode_ ? "GPU " : "CPU ");
    ui->client_indicator->setToolTip(QString::fromStdString(oss.str()));
    ui->indicator->show();
    QTimer::singleShot(50, this, &MainPage::onUpdateIndicator);
}

void MainPage::onDisconnectedConnection(int64_t device_id) {
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
    ui->indicator->hide();
    peer_client_device_id_ = std::nullopt;
    gpu_encode_ = false;
    gpu_decode_ = false;
    p2p_ = false;
    bandwidth_bps_ = false;
    video_codec_ = lt::VideoCodecType::Unknown;
    enable_gamepad_ = false;
    enable_keyboard_ = false;
    enable_mouse_ = false;
}

void MainPage::onConnectBtnPressed() {
    auto dev_id = ui->device_id->currentText();
    auto token = ui->access_token->text();

    emit onConnectBtnPressed1(dev_id.toStdString(), token.toStdString());
}

void MainPage::onUpdateIndicator() {
    if (!peer_client_device_id_.has_value()) {
        return;
    }
    setPixmapForIndicator(enable_gamepad_, gamepad_hit_time_, ui->gamepad_indicator, gp_white_,
                          gp_gray_, gp_red_, gp_green_);
    setPixmapForIndicator(enable_mouse_, mouse_hit_time_, ui->mouse_indicator, mouse_white_,
                          mouse_gray_, mouse_red_, mouse_green_);
    setPixmapForIndicator(enable_keyboard_, keyboard_hit_time_, ui->keyboard_indicator, kb_white_,
                          kb_gray_, kb_red_, kb_green_);
    QTimer::singleShot(50, this, &MainPage::onUpdateIndicator);
}

void MainPage::onCopyClicked() {
    auto clipboard = QApplication::clipboard();
    QString device_id = ui->my_device_id->text();
    device_id = device_id.simplified();
    device_id.replace(" ", "");
    clipboard->setText(device_id);
    ui->label_copy_success->show();
    QTimer::singleShot(2'000, [this]() { ui->label_copy_success->hide(); });
}

void MainPage::onShowTokenClicked() {
    if (token_showing_) {
        token_showing_ = false;
        show_token_label_->setPixmap(eye_close_);
        ui->my_access_token->setText("••••••");
    }
    else {
        token_showing_ = true;
        token_last_show_time_ms_ = ltlib::steady_now_ms();
        show_token_label_->setPixmap(eye_open_);
        ui->my_access_token->setText(QString::fromStdString(access_token_text_));
        QTimer::singleShot(5'100, std::bind(&MainPage::onTimeoutHideToken, this));
    }
}

void MainPage::onTimeoutHideToken() {
    if (!token_showing_) {
        return;
    }
    int64_t now_ms = ltlib::steady_now_ms();
    if (token_last_show_time_ms_ + 5'000 <= now_ms) {
        token_showing_ = false;
        show_token_label_->setPixmap(eye_close_);
        ui->my_access_token->setText("••••••");
    }
    else {
        QTimer::singleShot(token_last_show_time_ms_ + 5'100 - now_ms,
                           std::bind(&MainPage::onTimeoutHideToken, this));
    }
}

void MainPage::loadPixmap() {
    copy_.load(":/icons/icons/copy.png");
    eye_close_.load(":/icons/icons/eye_close.png");
    eye_open_.load(":/icons/icons/eye_open.png");

    kick_.load(":/icons/icons/close.png");

    mouse_.load(":/icons/icons/mouse.png");
    mouse_white_.load(":/icons/icons/mouse_white.png");
    mouse_gray_.load(":/icons/icons/mouse_gray.png");
    mouse_red_.load(":/icons/icons/mouse_red.png");
    mouse_green_.load(":/icons/icons/mouse_green.png");

    kb_.load(":/icons/icons/keyboard.png");
    kb_white_.load(":/icons/icons/keyboard_white.png");
    kb_gray_.load(":/icons/icons/keyboard_gray.png");
    kb_red_.load(":/icons/icons/keyboard_red.png");
    kb_green_.load(":/icons/icons/keyboard_green.png");

    gp_.load(":/icons/icons/gamepad.png");
    gp_white_.load(":/icons/icons/gamepad_white.png");
    gp_gray_.load(":/icons/icons/gamepad_gray.png");
    gp_red_.load(":/icons/icons/gamepad_red.png");
    gp_green_.load(":/icons/icons/gamepad_green.png");
}

void MainPage::setPixmapForIndicator(bool enable, int64_t last_time, QLabel* label,
                                     const QPixmap& white, const QPixmap& gray, const QPixmap& red,
                                     const QPixmap& green) {
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
