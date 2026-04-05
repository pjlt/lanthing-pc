/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

void MainWindow::switchToMainPage() {
    const int main_index = indexOfPageByObjectName(QStringLiteral("pageLink"));
    if (main_index < 0) {
        LOG(ERR) << "Main page not found";
        return;
    }
    if (ui->stackedWidget->currentIndex() != main_index) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()), ui->btnLinkTab);
        ui->stackedWidget->setCurrentIndex(main_index);
    }
}

void MainWindow::switchToManagerPage() {
    const int manager_index = indexOfPageByObjectName(QStringLiteral("pageMgr"));
    if (manager_index < 0) {
        LOG(ERR) << "Manager page not found";
        return;
    }
    if (ui->stackedWidget->currentIndex() != manager_index) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()),
                             ui->btnManagerTab);
        ui->stackedWidget->setCurrentIndex(manager_index);
    }
}

void MainWindow::switchToSettingPage() {
    const int settings_index = indexOfPageByObjectName(QStringLiteral("pageSettings"));
    if (settings_index < 0) {
        LOG(ERR) << "Settings page not found";
        return;
    }
    if (ui->stackedWidget->currentIndex() != settings_index) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()),
                             ui->btnSettingsTab);
        ui->stackedWidget->setCurrentIndex(settings_index);
    }
}

void MainWindow::switchToAboutPage() {
    const int about_index = indexOfPageByObjectName(QStringLiteral("pageAbout"));
    if (about_index < 0) {
        LOG(ERR) << "About page not found";
        return;
    }
    if (ui->stackedWidget->currentIndex() != about_index) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()), ui->btnAboutTab);
        ui->stackedWidget->setCurrentIndex(about_index);
    }
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
    if (ui->stackedWidget == nullptr || index < 0 || index >= ui->stackedWidget->count()) {
        LOG(ERR) << "Unknown tab index!";
        return ui->btnLinkTab;
    }

    QWidget* page = ui->stackedWidget->widget(index);
    if (page == nullptr) {
        LOG(ERR) << "Page is null for tab index " << index;
        return ui->btnLinkTab;
    }

    const QString page_name = page->objectName();
    if (page_name == QStringLiteral("pageLink")) {
        return ui->btnLinkTab;
    }
    if (page_name == QStringLiteral("pageMgr")) {
        return ui->btnManagerTab;
    }
    if (page_name == QStringLiteral("pageSettings")) {
        return ui->btnSettingsTab;
    }
    if (page_name == QStringLiteral("pageAbout")) {
        return ui->btnAboutTab;
    }

    LOG(ERR) << "Unknown page name: " << page_name.toStdString();
    return ui->btnLinkTab;
}

void MainWindow::swapTabBtnStyleSheet(QPushButton* old_selected, QPushButton* new_selected) {
    QString stylesheet = new_selected->styleSheet();
    new_selected->setStyleSheet(old_selected->styleSheet());
    old_selected->setStyleSheet(stylesheet);
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
    if (ui->stackedWidget == nullptr) {
        return -1;
    }

    for (int i = 0; i < ui->stackedWidget->count(); i++) {
        QWidget* page = ui->stackedWidget->widget(i);
        if (page != nullptr && page->objectName() == object_name) {
            return i;
        }
    }
    return -1;
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
