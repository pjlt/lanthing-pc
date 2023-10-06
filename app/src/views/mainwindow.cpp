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

#include <ltlib/logging.h>
#include <ltlib/strings.h>

#include "app.h"
#include "link/mainpage.h"
#include "menu/menu.h"
#include "setting/settingpage.h"
#include "ui_mainwindow.h"

#include <QtCore/qtimer.h>
#include <QtWidgets/QLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/qmessagebox.h>

MainWindow::MainWindow(lt::App* a, QWidget* parent)
    : QMainWindow(parent)
    , app(a)
    , ui(new Ui::MainWindow) {
    assert(app);

    ui->setupUi(this);

    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout;
    widget->setLayout(layout);
    setCentralWidget(widget);

    auto* menu = new QWidget();
    layout->addWidget(menu);

    auto* pages_layout = new QStackedLayout();
    auto* main_page = new QWidget();
    auto* setting_page = new QWidget();
    pages_layout->addWidget(main_page);    // index 0
    pages_layout->addWidget(setting_page); // index 1
    switch_to_main_page_ = [pages_layout]() { pages_layout->setCurrentIndex(0); };
    switch_to_setting_page_ = [pages_layout]() { pages_layout->setCurrentIndex(1); };

    layout->addLayout(pages_layout);

    menu_ui = new Menu(menu);
    main_page_ui = new MainPage(app->getHistoryDeviceIDs(), main_page);

    auto loadded_settigns = a->getSettings();
    PreloadSettings settings;
    settings.refresh_access_token = loadded_settigns.auto_refresh_access_token;
    settings.run_as_daemon = loadded_settigns.run_as_daemon;
    settings.relay_server = loadded_settigns.relay_server;
    setting_page_ui = new SettingPage(settings, setting_page);

    connect(menu_ui, &Menu::pageSelect,
            [pages_layout](const int index) { pages_layout->setCurrentIndex(index); });
    connect(
        main_page_ui, &MainPage::onConnectBtnPressed1,
        [this](const std::string& dev_id, const std::string& token) { doInvite(dev_id, token); });
    connect(setting_page_ui, &SettingPage::refreshAccessTokenStateChanged,
            [this](bool checked) { app->enableRefreshAccessToken(checked); });
    connect(setting_page_ui, &SettingPage::runAsDaemonStateChanged,
            [this](bool checked) { app->enableRunAsDaemon(checked); });
    connect(setting_page_ui, &SettingPage::relayServerChanged,
            [this](const std::string& svr) { app->setRelayServer(svr); });

    // FIXME: 还没有实现"登录逻辑"
    menu_ui->setLoginStatus(Menu::LoginStatus::LOGINING);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::switchToMainPage() {
    switch_to_main_page_();
}

void MainWindow::switchToSettingPage() {
    switch_to_setting_page_();
}

void MainWindow::closeEvent(QCloseEvent* ev) {
    (void)ev;
    hide();
}

void DispatchToMainThread(std::function<void()> callback) {
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

void MainWindow::onLoginRet(ErrCode code, const std::string& err) {
    DispatchToMainThread([this, code, err]() {
        switch (code) {
        case ErrCode::OK:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_SUCCESS);
            break;
        case lt::UiCallback::ErrCode::FALIED:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_FAILED);
            break;
        case lt::UiCallback::ErrCode::CONNECTING:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGINING);
            break;
        default:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_FAILED);
            LOG(FATAL) << "Unknown LoginRet " << static_cast<int32_t>(code);
            break;
        }
    });
}

void MainWindow::onInviteRet(ErrCode code, const std::string& err) {
    (void)code;
    (void)err;
}

void MainWindow::onLocalDeviceID(int64_t device_id) {
    DispatchToMainThread([this, device_id]() { main_page_ui->onUpdateLocalDeviceID(device_id); });
}

void MainWindow::onLocalAccessToken(const std::string& access_token) {
    DispatchToMainThread(
        [this, access_token]() { main_page_ui->onUpdateLocalAccessToken(access_token); });
}

void MainWindow::doInvite(const std::string& dev_id, const std::string& token) {
    int64_t deviceID = std::atoll(dev_id.c_str());
    if (deviceID != 0) {
        app->connect(deviceID, token);
    }
    else {
        LOG(FATAL) << "Parse deviceID(" << dev_id << ") to int64_t failed!";
    }
}
