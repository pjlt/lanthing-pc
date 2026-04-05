/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

#include "main_window_navigator.h"

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
