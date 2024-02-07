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

#include "ui_mainwindow.h"

#include <QMouseEvent>
#include <QtCore/qtimer.h>
#include <QtWidgets/QLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qscrollarea.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qmenu.h>
#include <qstringbuilder.h>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/times.h>
#include <ltproto/server/new_version.pb.h>
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

// -Werror=unused-function
/*
QColor toColor(QString colorstr) {

    int r = colorstr.mid(1, 2).toInt(nullptr, 16);
    int g = colorstr.mid(3, 2).toInt(nullptr, 16);
    int b = colorstr.mid(5, 2).toInt(nullptr, 16);
    QColor color = QColor(r, g, b);
    return color;
}
*/

} // namespace

MainWindow::MainWindow(const lt::GUI::Params& params, QWidget* parent)
    : QMainWindow(parent)
    , params_(params)
    , ui(new Ui_MainWindow)
    , relay_validator_(QRegularExpression("relay:(.+?:[0-9]+?):(.+?):(.+?)")) {

    ui->setupUi(this);

    qApp->installEventFilter(this);

    loadPixmap();

    // 版本号
    std::stringstream oss_ver;
    oss_ver << "v" << LT_VERSION_MAJOR << "." << LT_VERSION_MINOR << "." << LT_VERSION_PATCH;
    ui->labelVersion->setText(QString::fromStdString(oss_ver.str()));

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
    QSizePolicy retain = ui->labelCopied->sizePolicy();
    retain.setRetainSizeWhenHidden(true);
    ui->labelCopied->setSizePolicy(retain);
    ui->labelCopied->hide();

    // 登录进度条
    // （因为新增显示“service状态”，再用ProgressWidget去显示“登录状态”就不合适，暂时没有想到更好的UI，先屏蔽）
    // login_progress_ = new qt_componets::ProgressWidget();
    // login_progress_->setVisible(false);
    // login_progress_->setProgressColor(toColor("#8198ff"));

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
    ui->cbDeviceID->setValidator(new QIntValidator(100'000'000, 999'999'999, this));

    // 验证码
    QAction* lock_position = new QAction();
    lock_position->setIcon(QIcon(":/res/png_icons/lock.png"));
    ui->leditAccessToken->addAction(lock_position, QLineEdit::LeadingPosition);
    ui->leditAccessToken->setValidator(new AccesstokenValidator(this));

    // 客户端指示器
    setupClientIndicators();

    // '设置'页面
    auto settings = params.get_settings();
    // ui->checkboxService->setChecked(settings.run_as_daemon);
    ui->checkboxService->hide();
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
    ui->btnPortRange->setEnabled(false);
    ui->leditMinPort->setValidator(new QIntValidator(1025, 65536, this));
    ui->leditMaxPort->setValidator(new QIntValidator(1025, 65536, this));
    if (settings.min_port != 0 && settings.max_port != 0) {
        ui->leditMinPort->setText(QString::number(settings.min_port));
        ui->leditMaxPort->setText(QString::number(settings.max_port));
    }
    ui->btnIgnoredNIC->setEnabled(false);
    if (!settings.ignored_nic.empty()) {
        ui->leditIgnoredNIC->setText(QString::fromStdString(settings.ignored_nic));
    }
    ui->btnStatusColor->setEnabled(false);
    ui->leditRed->setValidator(new QIntValidator(0, 255, this));
    ui->leditGreen->setValidator(new QIntValidator(0, 255, this));
    ui->leditBlue->setValidator(new QIntValidator(0, 255, this));
    if (settings.status_color.has_value()) {
        uint32_t color = settings.status_color.value();
        uint32_t red = (color & 0xff000000) >> 24;
        uint32_t green = (color & 0x00ff0000) >> 16;
        uint32_t blue = (color & 0x0000ff00) >> 8;
        ui->leditRed->setText(QString::number(red));
        ui->leditGreen->setText(QString::number(green));
        ui->leditBlue->setText(QString::number(blue));
    }
    ui->btnMouseAccel->setEnabled(false);
    ui->leditMouseAccel->setValidator(new QDoubleValidator(0.1, 3.0, 1, this));
    if (settings.rel_mouse_accel > 0 && settings.rel_mouse_accel <= 30) {
        double accel = settings.rel_mouse_accel / 10.0;
        ui->leditMouseAccel->setText(QString::number(accel, 'f', 1));
    }

    // 左下角状态栏
    setLoginStatusInUIThread(lt::GUI::LoginStatus::Connecting);
#if LT_WINDOWS
    setServiceStatusInUIThread(lt::GUI::ServiceStatus::Down);
#else  // LT_WINDOWS
    QSizePolicy sp_retain = ui->labelControlledInfo->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    ui->labelControlledInfo->setSizePolicy(sp_retain);
    ui->labelControlledInfo->hide();
#endif // LT_WINDOWS

    // 客户端表格
    addOrUpdateTrustedDevices();

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

void MainWindow::switchToManagerPage() {
    if (ui->stackedWidget->currentIndex() != 1) {
        swapTabBtnStyleSheet(indexToTabButton(ui->stackedWidget->currentIndex()),
                             ui->btnManagerTab);
        ui->stackedWidget->setCurrentIndex(1);
    }
}

void MainWindow::switchToSettingPage() {
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

void MainWindow::setLoginStatus(lt::GUI::LoginStatus status) {
    dispatchToUiThread([this, status]() { setLoginStatusInUIThread(status); });
}

void MainWindow::setServiceStatus(lt::GUI::ServiceStatus status) {
    dispatchToUiThread([this, status]() { setServiceStatusInUIThread(status); });
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
        oss << msg->device_id() << " " << delay_ms * 2 << "ms " << std::fixed
            << std::setprecision(1) << Mbps << "Mbps " << video_codec_ << " "
            << (p2p_ ? "P2P " : "Relay ") << (gpu_decode_ ? "GPU:" : "CPU:")
            << (gpu_encode_ ? "GPU " : "CPU ");
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

void MainWindow::addOrUpdateTrustedDevice(int64_t device_id, int64_t time_s) {
    dispatchToUiThread([this, device_id, time_s]() {
        addOrUpdateTrustedDevice(device_id, true, false, false, time_s);
    });
}

void MainWindow::onNewVersion(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    dispatchToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::server::NewVersion>(_msg);
        std::ostringstream oss;
        int64_t version = msg->major() * 1'000'000 + msg->minor() * 1'000 + msg->patch();
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
        params_.set_relay_server(ui->leditRelay->text().trimmed().toStdString());
    });
    connect(ui->leditMinPort, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.trimmed().isEmpty() && ui->leditMaxPort->text().trimmed().isEmpty()) {
            ui->btnPortRange->setEnabled(true);
            return;
        }
        if (_text.trimmed().isEmpty() || ui->leditMaxPort->text().trimmed().isEmpty()) {
            ui->btnPortRange->setEnabled(false);
            return;
        }
        int min_port = _text.trimmed().toInt();
        int max_port = ui->leditMaxPort->text().trimmed().toInt();
        if (min_port >= max_port) {
            ui->btnPortRange->setEnabled(false);
        }
        else {
            ui->btnPortRange->setEnabled(true);
        }
    });
    connect(ui->leditMaxPort, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.trimmed().isEmpty() && ui->leditMinPort->text().trimmed().isEmpty()) {
            ui->btnPortRange->setEnabled(true);
            return;
        }
        if (_text.trimmed().isEmpty() || ui->leditMinPort->text().trimmed().isEmpty()) {
            ui->btnPortRange->setEnabled(false);
            return;
        }
        int max_port = _text.trimmed().toInt();
        int min_port = ui->leditMinPort->text().trimmed().toInt();
        if (min_port >= max_port) {
            ui->btnPortRange->setEnabled(false);
        }
        else {
            ui->btnPortRange->setEnabled(true);
        }
    });
    connect(ui->btnPortRange, &QPushButton::clicked, [this]() {
        if (ui->leditMinPort->text().trimmed().isEmpty() &&
            ui->leditMaxPort->text().trimmed().isEmpty()) {
            params_.set_port_range(0, 0);
            ui->btnPortRange->setEnabled(false);
            return;
        }
        if (ui->leditMinPort->text().trimmed().isEmpty() ||
            ui->leditMaxPort->text().trimmed().isEmpty()) {
            return;
        }
        int min_port = ui->leditMinPort->text().trimmed().toInt();
        int max_port = ui->leditMaxPort->text().trimmed().toInt();
        if (min_port < max_port && min_port > 1024 && min_port < 65536 && max_port > 1025 &&
            max_port <= 65536) {
            params_.set_port_range(min_port, max_port);
            ui->btnPortRange->setEnabled(false);
        }
    });
    connect(ui->leditIgnoredNIC, &QLineEdit::textChanged,
            [this](const QString&) { ui->btnIgnoredNIC->setEnabled(true); });
    connect(ui->btnIgnoredNIC, &QPushButton::clicked, [this]() {
        ui->btnIgnoredNIC->setEnabled(false);
        params_.set_ignored_nic(ui->leditIgnoredNIC->text().trimmed().toStdString());
    });
    connect(ui->leditRed, &QLineEdit::textChanged,
            std::bind(&MainWindow::onLineEditStatusColorChanged, this, std::placeholders::_1));
    connect(ui->leditGreen, &QLineEdit::textChanged,
            std::bind(&MainWindow::onLineEditStatusColorChanged, this, std::placeholders::_1));
    connect(ui->leditBlue, &QLineEdit::textChanged,
            std::bind(&MainWindow::onLineEditStatusColorChanged, this, std::placeholders::_1));
    connect(ui->btnStatusColor, &QPushButton::clicked, [this]() {
        ui->btnStatusColor->setEnabled(false);
        if (ui->leditRed->text().isEmpty() && ui->leditGreen->text().isEmpty() &&
            ui->leditBlue->text().isEmpty()) {
            params_.set_status_color(-1);
        }
        else {
            uint32_t red = static_cast<uint32_t>(ui->leditRed->text().trimmed().toInt());
            uint32_t green = static_cast<uint32_t>(ui->leditGreen->text().trimmed().toInt());
            uint32_t blue = static_cast<uint32_t>(ui->leditBlue->text().trimmed().toInt());
            params_.set_status_color((red << 24) | (green << 16) | (blue << 8));
        }
    });
    connect(ui->leditMouseAccel, &QLineEdit::textChanged, [this](const QString&) {
        if (ui->leditMouseAccel->text().isEmpty()) {
            ui->btnMouseAccel->setEnabled(true);
            return;
        }
        double accel = ui->leditMouseAccel->text().trimmed().toDouble();
        int64_t accel_int = static_cast<int64_t>(accel * 10);
        if (accel_int >= 1 && accel_int <= 30) {
            ui->btnMouseAccel->setEnabled(true);
        }
        else {
            ui->btnMouseAccel->setEnabled(false);
        }
    });
    connect(ui->btnMouseAccel, &QPushButton::clicked, [this]() {
        ui->btnMouseAccel->setEnabled(false);
        if (ui->leditMouseAccel->text().isEmpty()) {
            params_.set_rel_mouse_accel(0);
        }
        else {
            double accel = ui->leditMouseAccel->text().trimmed().toDouble();
            int64_t accel_int = static_cast<int64_t>(accel * 10);
            if (accel_int >= 1 && accel_int <= 30) {
                params_.set_rel_mouse_accel(accel_int);
            }
            else {
                LOG(ERR) << "Set relative mouse accel '"
                         << ui->leditMouseAccel->text().toStdString() << "' failed";
            }
        }
    });
}

void MainWindow::setLoginStatusInUIThread(lt::GUI::LoginStatus status) {
    switch (status) {
    case lt::GUI::LoginStatus::Connected:
        // ui->statusBarLayout->removeWidget(login_progress_);
        // login_progress_->setVisible(false);
        ui->labelLoginInfo->setText(tr("🟢Connected to server"));
        break;
    case lt::GUI::LoginStatus::Connecting:
        // ui->statusBarLayout->addWidget(login_progress_);
        // login_progress_->setVisible(true);
        //  ui->labelLoginInfo->setStyleSheet("QLabel{}");
        ui->labelLoginInfo->setText(tr("🟡Connecting..."));
        break;
    case lt::GUI::LoginStatus::Disconnected:
    default:
        // ui->statusBarLayout->removeWidget(login_progress_);
        // login_progress_->setVisible(false);
        ui->labelLoginInfo->setText(tr("🔴Disconnected from server"));
        if (status != lt::GUI::LoginStatus::Disconnected) {
            LOG(ERR) << "Unknown Login status " << static_cast<int32_t>(status);
        }
        break;
    }
}

void MainWindow::setServiceStatusInUIThread(lt::GUI::ServiceStatus status) {
    switch (status) {
    case lt::GUI::ServiceStatus::Up:
        ui->labelControlledInfo->setText(tr("🟢Controlled module up"));
        break;
    case lt::GUI::ServiceStatus::Down:
    default:
        ui->labelControlledInfo->setText(tr("🔴Controlled module down"));
        if (status != lt::GUI::ServiceStatus::Down) {
            LOG(ERR) << "Unknown ServiceStatus " << static_cast<int32_t>(status);
        }
        break;
    }
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

    audio_.load(":/res/png_icons/audio.png");
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
    auto token = ui->leditAccessToken->text().trimmed().toStdString();
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
        ui->labelMyAccessToken->setText("******");
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
        ui->labelMyAccessToken->setText("******");
    }
    else {
        QTimer::singleShot(token_last_show_time_ms_ + 5'100 - now_ms,
                           std::bind(&MainWindow::onTimeoutHideToken, this));
    }
}

void MainWindow::onLineEditStatusColorChanged(const QString&) {
    if (ui->leditRed->text().isEmpty() && ui->leditGreen->text().isEmpty() &&
        ui->leditBlue->text().isEmpty()) {
        ui->btnStatusColor->setEnabled(true);
        return;
    }
    if (ui->leditRed->text().isEmpty() || ui->leditGreen->text().isEmpty() ||
        ui->leditBlue->text().isEmpty()) {
        ui->btnStatusColor->setEnabled(false);
        return;
    }
    uint32_t red = static_cast<uint32_t>(ui->leditRed->text().trimmed().toInt());
    uint32_t green = static_cast<uint32_t>(ui->leditGreen->text().trimmed().toInt());
    uint32_t blue = static_cast<uint32_t>(ui->leditBlue->text().trimmed().toInt());
    if (red > 255 || green > 255 || blue > 255) {
        ui->btnStatusColor->setEnabled(false);
        return;
    }
    ui->btnStatusColor->setEnabled(true);
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
    int row = -1;
    for (int i = 0; i < ui->tableWidget->rowCount(); i++) {
        if (ui->tableWidget->item(i, 0)->data(Qt::DisplayRole).toLongLong() == device_id) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        row = ui->tableWidget->rowCount();
        ui->tableWidget->setRowCount(row + 1);
    }

    // id
    QTableWidgetItem* id_item = new QTableWidgetItem;
    ui->tableWidget->setItem(row, 0, id_item);
    id_item->setData(Qt::DisplayRole, QVariant::fromValue(device_id));
    // gamepad
    QCheckBox* gamepad_item = new QCheckBox();
    gamepad_item->setChecked(gamepad);
    connect(gamepad_item, &QCheckBox::stateChanged, [this, gamepad_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Gamepad,
                                         gamepad_item->isChecked());
    });
    ui->tableWidget->setCellWidget(row, 1, makeWidgetHCentered(gamepad_item));
    // mouse
    QCheckBox* mouse_item = new QCheckBox();
    mouse_item->setChecked(mouse);
    connect(mouse_item, &QCheckBox::stateChanged, [this, mouse_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Mouse,
                                         mouse_item->isChecked());
    });
    ui->tableWidget->setCellWidget(row, 2, makeWidgetHCentered(mouse_item));
    // keyboard
    QCheckBox* keyboard_item = new QCheckBox();
    keyboard_item->setChecked(keyboard);
    connect(keyboard_item, &QCheckBox::stateChanged, [this, keyboard_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Keyboard,
                                         keyboard_item->isChecked());
    });
    ui->tableWidget->setCellWidget(row, 3, makeWidgetHCentered(keyboard_item));
    // time
    QTableWidgetItem* time_item =
        new QTableWidgetItem(QDateTime::fromSecsSinceEpoch(last_access_time)
                                 .toLocalTime()
                                 .toString("yyyy.MM.dd hh:mm:ss"));
    ui->tableWidget->setItem(row, 4, time_item);
    // delete
    QPushButton* delete_item = new QPushButton(tr("delete"));
    connect(delete_item, &QPushButton::clicked, [this, device_id]() {
        for (int i = 0; i < ui->tableWidget->rowCount(); i++) {
            if (ui->tableWidget->item(i, 0)->data(Qt::DisplayRole).toLongLong() == device_id) {
                ui->tableWidget->removeRow(i);
                break;
            }
        }
        params_.delete_trusted_device(device_id);
    });
    ui->tableWidget->setCellWidget(row, 5, delete_item);
}

QWidget* MainWindow::makeWidgetHCentered(QWidget* input_widget) {
    QWidget* output_widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(output_widget);
    layout->addWidget(input_widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return output_widget;
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
