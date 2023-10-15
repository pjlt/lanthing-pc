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

#include "ui_mainwindow.h"
#include <views/link/mainpage.h>
#include <views/menu/menu.h>
#include <views/setting/settingpage.h>

#include <QtCore/qtimer.h>
#include <QtWidgets/QLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qscrollarea.h>

MainWindow::MainWindow(const lt::GUI::Params& params, QWidget* parent)
    : QMainWindow(parent)
    , params_(params)
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
    auto* setting_scroll = new QScrollArea();
    setting_scroll->setWidget(setting_page);
    setting_scroll->setFrameShape(QFrame::Shape::NoFrame);
    setting_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pages_layout->addWidget(main_page);      // index 0
    pages_layout->addWidget(setting_scroll); // index 1
    switch_to_main_page_ = [pages_layout]() { pages_layout->setCurrentIndex(0); };
    switch_to_setting_page_ = [pages_layout]() { pages_layout->setCurrentIndex(1); };

    layout->addLayout(pages_layout);

    menu_ui = new Menu(menu);
    main_page_ui = new MainPage(params_.get_history_device_ids(), main_page);

    auto loadded_settigns = params_.get_settings();
    PreloadSettings settings;
    settings.refresh_access_token = loadded_settigns.auto_refresh_access_token;
    settings.run_as_daemon = loadded_settigns.run_as_daemon;
    settings.relay_server = loadded_settigns.relay_server;
    setting_page_ui = new SettingPage(settings, setting_page);

    connect(menu_ui, &Menu::pageSelect,
            [pages_layout](const int index) { pages_layout->setCurrentIndex(index); });
    connect(
        main_page_ui, &MainPage::onConnectBtnPressed1,
        [this](const std::string& dev_id, const std::string& token) { doConnect(dev_id, token); });
    connect(main_page_ui, &MainPage::onOperateConnection, this, &MainWindow::onOperateConnection);
    connect(setting_page_ui, &SettingPage::refreshAccessTokenStateChanged,
            [this](bool checked) { params_.enable_auto_refresh_access_token(checked); });
    connect(setting_page_ui, &SettingPage::runAsDaemonStateChanged,
            [this](bool checked) { params_.enable_run_as_service(checked); });
    connect(setting_page_ui, &SettingPage::relayServerChanged,
            [this](const std::string& svr) { params_.set_relay_server(svr); });

    menu_ui->setLoginStatus(Menu::LoginStatus::LOGINING);
    setFixedSize(sizeHint());
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

void MainWindow::setLoginStatus(lt::GUI::ErrCode code) {
    DispatchToMainThread([this, code]() {
        switch (code) {
        case lt::GUI::ErrCode::OK:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_SUCCESS);
            break;
        case lt::GUI::ErrCode::FALIED:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_FAILED);
            break;
        case lt::GUI::ErrCode::CONNECTING:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGINING);
            break;
        default:
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_FAILED);
            LOG(WARNING) << "Unknown LoginRet " << static_cast<int32_t>(code);
            break;
        }
    });
}

void MainWindow::setDeviceID(int64_t device_id) {
    DispatchToMainThread([this, device_id]() { main_page_ui->onUpdateLocalDeviceID(device_id); });
}

void MainWindow::setAccessToken(const std::string& access_token) {
    DispatchToMainThread(
        [this, access_token]() { main_page_ui->onUpdateLocalAccessToken(access_token); });
}

void MainWindow::onConfirmConnection(int64_t device_id) {
    DispatchToMainThread([this, device_id]() {
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

void MainWindow::onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg) {
    DispatchToMainThread([this, msg]() { main_page_ui->onConnectionStatus(msg); });
}

void MainWindow::onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> msg) {
    DispatchToMainThread([this, msg]() { main_page_ui->onAccptedConnection(msg); });
}

void MainWindow::onDisconnectedConnection(int64_t device_id) {
    DispatchToMainThread(
        [this, device_id]() { main_page_ui->onDisconnectedConnection(device_id); });
}

void MainWindow::errorMessageBox(const std::string& message) {
    DispatchToMainThread([this, message]() {
        QMessageBox msgbox;
        msgbox.setText(QString::fromStdString(message));
        msgbox.setIcon(QMessageBox::Icon::Critical);
        msgbox.exec();
    });
}

void MainWindow::doConnect(const std::string& dev_id, const std::string& token) {
    int64_t deviceID = std::atoll(dev_id.c_str());
    if (deviceID != 0) {
        params_.connect(deviceID, token);
    }
    else {
        LOG(FATAL) << "Parse deviceID(" << dev_id << ") to int64_t failed!";
    }
}

void MainWindow::onOperateConnection(std::shared_ptr<google::protobuf::MessageLite> msg) {
    params_.on_operate_connection(msg);
}
