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

#if defined(LT_WINDOWS)
#include <Windows.h>
#endif // LT_WINDOWS

#include "mainwindow.h"

#include <cassert>
#include <filesystem>

#include "ui_mainwindow.h"

#include <QMouseEvent>
#include <QtCore/qtimer.h>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qscrollarea.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qmenu.h>
#include <qmimedata.h>
#include <qstringbuilder.h>
#include <qthread.h>

#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltlib/system.h>
#include <ltlib/times.h>
#include <ltlib/versions.h>
#include <ltproto/app/file_chunk.pb.h>
#include <ltproto/app/file_chunk_ack.pb.h>
#include <ltproto/app/pull_file.pb.h>
#include <ltproto/common/clipboard.pb.h>
#include <ltproto/server/new_version.pb.h>
#include <ltproto/service2app/accepted_connection.pb.h>
#include <ltproto/service2app/connection_status.pb.h>
#include <ltproto/service2app/operate_connection.pb.h>

#include <app/views/components/access_token_validator.h>

namespace {
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

    // 登录进度条
    // （因为新增显示“service状态”，再用ProgressWidget去显示“登录状态”就不合适，暂时没有想到更好的UI，先屏蔽）
    // login_progress_ = new qt_componets::ProgressWidget();
    // login_progress_->setVisible(false);
    // login_progress_->setProgressColor(toColor("#8198ff"));

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

    auto* page_link = new QWidget(ui->stackedWidget);
    page_link->setObjectName("pageLink");
    page_link->setStyleSheet("\n"
                             "QComboBox#cbDeviceID {\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     border-radius: 5px;\n"
                             "     border: 2px solid rgb(33, 37, 43);\n"
                             "     padding-left: 10px;\n"
                             "     selection-color: rgb(255, 255, 255);\n"
                             "     selection-background-color: rgb(255, 121, 198);\n"
                             "}\n"
                             "QComboBox#cbDeviceID:hover {\n"
                             "     border: 2px solid rgb(64, 71, 88);\n"
                             "}\n"
                             "QComboBox#cbDeviceID:drop-down {\n"
                             "     subcontrol-origin: padding;\n"
                             "     subcontrol-position: top right;\n"
                             "     width: 25px;\n"
                             "     border-left-width: 3px;\n"
                             "     border-left-color: rgba(39, 44, 54, 150);\n"
                             "     border-left-style: solid;\n"
                             "     border-top-right-radius: 3px;\n"
                             "     border-bottom-right-radius: 3px;\n"
                             "     background-image: url(:/res/icons/cil-arrow-bottom.png);\n"
                             "     background-position: center;\n"
                             "     background-repeat: no-repeat;\n"
                             "}\n"
                             "QComboBox#cbDeviceID QAbstractItemView {\n"
                             "     color: rgb(255, 121, 198);\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     padding: 10px;\n"
                             "     selection-background-color: rgb(39, 44, 54);\n"
                             "}\n"
                             "#leditAccessToken {\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     border-radius: 5px;\n"
                             "     border: 2px solid rgb(33, 37, 43);\n"
                             "     padding-left: 10px;\n"
                             "     selection-color: rgb(255, 255, 255);\n"
                             "     selection-background-color: rgb(255, 121, 198);\n"
                             "}\n"
                             "#leditAccessToken:hover {\n"
                             "     border: 2px solid rgb(64, 71, 88);\n"
                             "}\n"
                             "#leditAccessToken:focus {\n"
                             "     border: 2px solid rgb(91, 101, 124);\n"
                             "}\n"
                             "QPushButton#btnConnect {\n"
                             "     border: 2px solid rgb(52, 59, 72);\n"
                             "     border-radius: 5px;\n"
                             "     background-color: rgb(52, 59, 72);\n"
                             "}\n"
                             "QPushButton#btnConnect:hover {\n"
                             "     background-color: rgb(57, 65, 80);\n"
                             "     border: 2px solid rgb(61, 70, 86);\n"
                             "}\n"
                             "QPushButton#btnConnect:pressed {\n"
                             "     background-color: rgb(35, 40, 49);\n"
                             "     border: 2px solid rgb(43, 50, 61);\n"
                             "}\n"
                             "#labelClient1 {\n"
                             "     border: none;\n"
                             "     background-color: transparent;\n"
                             "}\n"
                             "#labelClient1:hover {\n"
                             "     background-color: rgb(33, 37, 43);\n"
                             "     border: 2px solid rgb(64, 71, 88);\n"
                             "}\n");
    auto* root_layout = new QVBoxLayout(page_link);
    root_layout->setSpacing(0);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setStretch(0, 3);
    root_layout->setStretch(1, 3);
    root_layout->setStretch(2, 1);
    root_layout->setStretch(3, 0);

    auto* frame_identity = new QFrame(page_link);
    frame_identity->setObjectName("linkRow1");
    auto* identity_layout = new QVBoxLayout(frame_identity);
    identity_layout->setSpacing(0);
    identity_layout->setContentsMargins(30, 9, 30, 9);
    auto* id_row = new QHBoxLayout();
    id_row->setSpacing(0);
    auto* label_device_id = new QLabel(tr("Device ID"), frame_identity);
    label_device_id->setMinimumSize(QSize(157, 71));
    label_device_id->setStyleSheet("font: 13pt;");
    id_row->addWidget(label_device_id);
    link_label_my_device_id_ = new QLabel(frame_identity);
    link_label_my_device_id_->setStyleSheet("font: 13pt;");
    id_row->addWidget(link_label_my_device_id_);
    link_btn_copy_ = new QPushButton(frame_identity);
    link_btn_copy_->setObjectName("btnCopy");
    link_btn_copy_->setMaximumWidth(50);
    link_btn_copy_->setText(QString());
    link_btn_copy_->setIcon(QIcon(":/res/icons/cil-clone.png"));
    id_row->addWidget(link_btn_copy_);
    link_label_copied_ = new QLabel(tr("Copied"), frame_identity);
    link_label_copied_->setMinimumWidth(60);
    link_label_copied_->setMaximumWidth(60);
    link_label_copied_->setStyleSheet("font: 9pt;");
    id_row->addWidget(link_label_copied_);
    identity_layout->addLayout(id_row);

    auto* token_row = new QHBoxLayout();
    token_row->setSpacing(0);
    auto* label_access_token = new QLabel(tr("Access Token"), frame_identity);
    label_access_token->setMinimumSize(QSize(157, 71));
    label_access_token->setStyleSheet("font: 13pt;");
    token_row->addWidget(label_access_token);
    link_label_my_access_token_ = new QLabel(QStringLiteral("******"), frame_identity);
    link_label_my_access_token_->setStyleSheet("font: 13pt;");
    token_row->addWidget(link_label_my_access_token_);
    link_btn_show_token_ = new QPushButton(frame_identity);
    link_btn_show_token_->setObjectName("btnShowToken");
    link_btn_show_token_->setMaximumWidth(50);
    link_btn_show_token_->setText(QString());
    link_btn_show_token_->setIcon(QIcon(":/res/icons/cil-low-vision.png"));
    token_row->addWidget(link_btn_show_token_);
    link_btn_refresh_token_ = new QPushButton(frame_identity);
    link_btn_refresh_token_->setObjectName("btnRefreshToken");
    link_btn_refresh_token_->setMaximumWidth(60);
    link_btn_refresh_token_->setText(QString());
    link_btn_refresh_token_->setIcon(QIcon(":/res/icons/cil-reload.png"));
    token_row->addWidget(link_btn_refresh_token_);
    identity_layout->addLayout(token_row);
    root_layout->addWidget(frame_identity);

    auto* frame_connect = new QFrame(page_link);
    frame_connect->setObjectName("linkRow2");
    auto* connect_row = new QVBoxLayout(frame_connect);
    connect_row->setSpacing(6);
    connect_row->setContentsMargins(30, 0, 30, 0);
    link_cb_device_id_ = new QComboBox(frame_connect);
    link_cb_device_id_->setObjectName("cbDeviceID");
    link_cb_device_id_->setEditable(true);
    link_cb_device_id_->setMinimumHeight(50);
    link_ledit_access_token_ = new QLineEdit(frame_connect);
    link_ledit_access_token_->setObjectName("leditAccessToken");
    link_ledit_access_token_->setMinimumHeight(45);
    link_ledit_access_token_->setPlaceholderText(tr("Access token"));
    link_btn_connect_ = new QPushButton(frame_connect);
    link_btn_connect_->setObjectName("btnConnect");
    link_btn_connect_->setMinimumHeight(40);
    link_btn_connect_->setText(QString());
    link_btn_connect_->setIcon(QIcon(":/res/icons/cil-link.png"));
    connect_row->addWidget(link_cb_device_id_);
    connect_row->addWidget(link_ledit_access_token_);
    connect_row->addWidget(link_btn_connect_);
    root_layout->addWidget(frame_connect);

    auto* frame_clients = new QFrame(page_link);
    frame_clients->setObjectName("linkRow3");
    frame_clients->setStyleSheet("border:none;");
    auto* clients_row = new QHBoxLayout(frame_clients);
    clients_row->setSpacing(0);
    clients_row->setContentsMargins(30, 0, 30, 0);
    link_indicator1_ = new QFrame(frame_clients);
    link_indicator1_->setObjectName("indicator1");
    link_indicator1_->setStyleSheet("border:none;");
    auto* indicator_row = new QHBoxLayout(link_indicator1_);
    indicator_row->setSpacing(0);
    indicator_row->setContentsMargins(0, 0, 0, 0);
    link_label_client1_ = new QLabel(link_indicator1_);
    link_label_client1_->setObjectName("labelClient1");
    link_label_client1_->setPixmap(QPixmap(":/res/png_icons/pc2.png"));
    link_label_client1_->setScaledContents(true);
    link_label_client1_->setFixedSize(70, 70);
    indicator_row->addWidget(link_label_client1_);
    auto* indicator_icons_container = new QFrame(link_indicator1_);
    indicator_icons_container->setStyleSheet("border:none;");
    auto* indicator_icons = new QVBoxLayout(indicator_icons_container);
    indicator_icons->setSpacing(0);
    indicator_icons->setContentsMargins(0, 0, 0, 0);
    link_label_gamepad1_ = new QLabel(link_indicator1_);
    link_label_mouse1_ = new QLabel(link_indicator1_);
    link_label_keyboard1_ = new QLabel(link_indicator1_);
    link_label_gamepad1_->setPixmap(QPixmap(":/res/png_icons/gamepad_gray.png"));
    link_label_mouse1_->setPixmap(QPixmap(":/res/png_icons/mouse_gray.png"));
    link_label_keyboard1_->setPixmap(QPixmap(":/res/png_icons/keyboard_gray.png"));
    link_label_gamepad1_->setScaledContents(true);
    link_label_mouse1_->setScaledContents(true);
    link_label_keyboard1_->setScaledContents(true);
    link_label_gamepad1_->setFixedSize(20, 20);
    link_label_mouse1_->setFixedSize(20, 20);
    link_label_keyboard1_->setFixedSize(20, 20);
    link_label_gamepad1_->setStyleSheet("border:none;");
    link_label_mouse1_->setStyleSheet("border:none;");
    link_label_keyboard1_->setStyleSheet("border:none;");
    indicator_icons->addWidget(link_label_gamepad1_);
    indicator_icons->addWidget(link_label_mouse1_);
    indicator_icons->addWidget(link_label_keyboard1_);
    indicator_row->addWidget(indicator_icons_container);
    clients_row->addWidget(link_indicator1_);
    clients_row->addStretch(1);
    link_indicator2_ = new QFrame(frame_clients);
    link_indicator2_->setObjectName("indicator2");
    link_indicator2_->setStyleSheet("border:none;");
    clients_row->addWidget(link_indicator2_);
    root_layout->addWidget(frame_clients);

    auto* frame_status = new QFrame(page_link);
    frame_status->setObjectName("frame_20");
    frame_status->setStyleSheet("border: none; background-color: transparent;");
    auto* status_row = new QHBoxLayout(frame_status);
    status_row->setContentsMargins(30, 10, 30, 10);
    link_label_version_ = new QLabel(frame_status);
    link_label_version_->setObjectName("labelVersion");
    status_row->addStretch(1);
    status_row->addWidget(link_label_version_);
    root_layout->addWidget(frame_status);

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

    auto* page_settings = new QWidget(ui->stackedWidget);
    page_settings->setObjectName("pageSettings");
    page_settings->setStyleSheet("#pageSettings .QLineEdit {"
                                 "\n\tbackground-color: rgb(33, 37, 43);"
                                 "\n\tborder-radius: 5px;"
                                 "\n\tborder: 2px solid rgb(33, 37, 43);"
                                 "\n\tpadding-left: 10px;"
                                 "\n\tselection-color: rgb(255, 255, 255);"
                                 "\n\tselection-background-color: rgb(255, 121, 198);"
                                 "\n}"
                                 "\n#pageSettings .QLineEdit:hover {"
                                 "\n\tborder: 2px solid rgb(64, 71, 88);"
                                 "\n}"
                                 "\n#pageSettings .QLineEdit:focus {"
                                 "\n\tborder: 2px solid rgb(91, 101, 124);"
                                 "\n}"
                                 "\n"
                                 "\n#pageSettings .QPushButton {"
                                 "\n\tborder: 2px solid rgb(52, 59, 72);"
                                 "\n\tborder-radius: 5px;"
                                 "\n\tbackground-color: rgb(52, 59, 72);"
                                 "\n}"
                                 "\n#pageSettings .QPushButton:hover {"
                                 "\n\tbackground-color: rgb(57, 65, 80);"
                                 "\n\tborder: 2px solid rgb(61, 70, 86);"
                                 "\n}"
                                 "\n#pageSettings .QPushButton:pressed {"
                                 "\n\tbackground-color: rgb(35, 40, 49);"
                                 "\n\tborder: 2px solid rgb(43, 50, 61);"
                                 "\n}");

    auto* page_layout = new QVBoxLayout(page_settings);
    page_layout->setContentsMargins(30, 9, 9, 9);

    auto* scroll = new QScrollArea(page_settings);
    scroll->setWidgetResizable(true);
    auto* scroll_contents = new QWidget(scroll);
    scroll_contents->setMinimumSize(QSize(0, 850));
    auto* content_layout = new QVBoxLayout(scroll_contents);

    auto* gb_system = new QGroupBox(tr("System"), scroll_contents);
    auto* gb_system_layout = new QVBoxLayout(gb_system);
    gb_system_layout->setContentsMargins(9, 30, 9, 30);
    settings_checkbox_service_ = new QCheckBox(tr("Run as Service"), gb_system);
    settings_checkbox_refresh_password_ =
        new QCheckBox(tr("Auto refresh access token"), gb_system);
    settings_checkbox_share_clipboard_ = new QCheckBox(tr("Share Clipboard"), gb_system);
    gb_system_layout->addWidget(settings_checkbox_service_);
    gb_system_layout->addWidget(settings_checkbox_refresh_password_);
    gb_system_layout->addWidget(settings_checkbox_share_clipboard_);
    content_layout->addWidget(gb_system);

    auto* gb_mouse_mode = new QGroupBox(tr("Default Mouse Mode (Win+Shift+X)"), scroll_contents);
    auto* gb_mouse_mode_layout = new QVBoxLayout(gb_mouse_mode);
    gb_mouse_mode_layout->setContentsMargins(9, 30, 9, 30);
    settings_radio_absolute_mouse_ = new QRadioButton(tr("Absolute Mode"), gb_mouse_mode);
    settings_radio_relative_mouse_ = new QRadioButton(tr("Relative Mode"), gb_mouse_mode);
    gb_mouse_mode_layout->addWidget(settings_radio_absolute_mouse_);
    gb_mouse_mode_layout->addWidget(settings_radio_relative_mouse_);
    content_layout->addWidget(gb_mouse_mode);

    auto* gb_fullscreen = new QGroupBox(tr("Fullscreen Mode"), scroll_contents);
    auto* gb_fullscreen_layout = new QVBoxLayout(gb_fullscreen);
    gb_fullscreen_layout->setContentsMargins(9, 30, 9, 30);
    settings_radio_real_fullscreen_ = new QRadioButton(tr("Real Fullscreen"), gb_fullscreen);
    settings_radio_windowed_fullscreen_ =
        new QRadioButton(tr("Windowed Fullscreen"), gb_fullscreen);
    gb_fullscreen_layout->addWidget(settings_radio_real_fullscreen_);
    gb_fullscreen_layout->addWidget(settings_radio_windowed_fullscreen_);
    content_layout->addWidget(gb_fullscreen);

    auto* gb_transport = new QGroupBox(tr("Transport"), scroll_contents);
    auto* gb_transport_layout = new QVBoxLayout(gb_transport);
    gb_transport_layout->setContentsMargins(9, 30, 9, 30);
    settings_checkbox_tcp_ = new QCheckBox(tr("Enable TCP"), gb_transport);
    gb_transport_layout->addWidget(settings_checkbox_tcp_);
    auto* row_ports = new QHBoxLayout();
    settings_ledit_min_port_ = new QLineEdit(gb_transport);
    settings_ledit_min_port_->setPlaceholderText(tr("Min Port"));
    settings_ledit_max_port_ = new QLineEdit(gb_transport);
    settings_ledit_max_port_->setPlaceholderText(tr("Max Port"));
    settings_btn_port_range_ = new QPushButton(tr("Confirm"), gb_transport);
    row_ports->addWidget(settings_ledit_min_port_);
    row_ports->addWidget(settings_ledit_max_port_);
    row_ports->addWidget(settings_btn_port_range_);
    gb_transport_layout->addLayout(row_ports);
    content_layout->addWidget(gb_transport);

    auto* gb_network = new QGroupBox(tr("Network"), scroll_contents);
    auto* gb_network_layout = new QVBoxLayout(gb_network);
    gb_network_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_relay = new QHBoxLayout();
    settings_ledit_relay_ = new QLineEdit(gb_network);
    settings_ledit_relay_->setPlaceholderText(tr("relay:host:token:user"));
    settings_btn_relay_ = new QPushButton(tr("Confirm"), gb_network);
    row_relay->addWidget(settings_ledit_relay_);
    row_relay->addWidget(settings_btn_relay_);
    gb_network_layout->addLayout(row_relay);
    auto* row_nic = new QHBoxLayout();
    settings_ledit_ignored_nic_ = new QLineEdit(gb_network);
    settings_ledit_ignored_nic_->setPlaceholderText(tr("Ignored NIC list"));
    settings_btn_ignored_nic_ = new QPushButton(tr("Confirm"), gb_network);
    row_nic->addWidget(settings_ledit_ignored_nic_);
    row_nic->addWidget(settings_btn_ignored_nic_);
    gb_network_layout->addLayout(row_nic);
    content_layout->addWidget(gb_network);

    auto* gb_bandwidth = new QGroupBox(tr("Bandwidth"), scroll_contents);
    auto* gb_bandwidth_layout = new QVBoxLayout(gb_bandwidth);
    gb_bandwidth_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_mbps = new QHBoxLayout();
    settings_ledit_max_mbps_ = new QLineEdit(gb_bandwidth);
    settings_ledit_max_mbps_->setPlaceholderText(tr("Max Mbps (1-100)"));
    settings_btn_max_mbps_ = new QPushButton(tr("Confirm"), gb_bandwidth);
    row_mbps->addWidget(settings_ledit_max_mbps_);
    row_mbps->addWidget(settings_btn_max_mbps_);
    gb_bandwidth_layout->addLayout(row_mbps);
    content_layout->addWidget(gb_bandwidth);

    auto* gb_overlay = new QGroupBox(tr("Overlay"), scroll_contents);
    auto* gb_overlay_layout = new QVBoxLayout(gb_overlay);
    gb_overlay_layout->setContentsMargins(9, 30, 9, 30);
    settings_checkbox_overlay_ = new QCheckBox(tr("Show overlay"), gb_overlay);
    gb_overlay_layout->addWidget(settings_checkbox_overlay_);
    content_layout->addWidget(gb_overlay);

    auto* gb_status_color = new QGroupBox(tr("Status Color"), scroll_contents);
    auto* gb_status_color_layout = new QVBoxLayout(gb_status_color);
    gb_status_color_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_color = new QHBoxLayout();
    settings_ledit_red_ = new QLineEdit(gb_status_color);
    settings_ledit_red_->setPlaceholderText(QStringLiteral("R"));
    settings_ledit_green_ = new QLineEdit(gb_status_color);
    settings_ledit_green_->setPlaceholderText(QStringLiteral("G"));
    settings_ledit_blue_ = new QLineEdit(gb_status_color);
    settings_ledit_blue_->setPlaceholderText(QStringLiteral("B"));
    settings_btn_status_color_ = new QPushButton(tr("Confirm"), gb_status_color);
    row_color->addWidget(settings_ledit_red_);
    row_color->addWidget(settings_ledit_green_);
    row_color->addWidget(settings_ledit_blue_);
    row_color->addWidget(settings_btn_status_color_);
    gb_status_color_layout->addLayout(row_color);
    content_layout->addWidget(gb_status_color);

    auto* gb_mouse = new QGroupBox(tr("Relative Mouse Accel"), scroll_contents);
    auto* gb_mouse_layout = new QVBoxLayout(gb_mouse);
    gb_mouse_layout->setContentsMargins(9, 30, 9, 30);
    auto* row_accel = new QHBoxLayout();
    settings_ledit_mouse_accel_ = new QLineEdit(gb_mouse);
    settings_ledit_mouse_accel_->setPlaceholderText(tr("0.1 - 3.0"));
    settings_btn_mouse_accel_ = new QPushButton(tr("Confirm"), gb_mouse);
    row_accel->addWidget(settings_ledit_mouse_accel_);
    row_accel->addWidget(settings_btn_mouse_accel_);
    gb_mouse_layout->addLayout(row_accel);
    content_layout->addWidget(gb_mouse);

    content_layout->addStretch(1);
    scroll->setWidget(scroll_contents);
    page_layout->addWidget(scroll);

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

    auto* page_mgr = new QWidget(ui->stackedWidget);
    page_mgr->setObjectName("pageMgr");
    auto* layout = new QVBoxLayout(page_mgr);

    auto* title = new QLabel(page_mgr);
    title->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    title->setText(tr("Trusted clients:"));
    layout->addWidget(title);

    auto* table = new QTableWidget(page_mgr);
    table->setObjectName("tableWidget");
    table->setStyleSheet("QTableWidget {\n"
                         "\tbackground-color: transparent;\n"
                         "\tpadding: 10px;\n"
                         "\tborder-radius: 5px;\n"
                         "\tgridline-color: rgb(44, 49, 58);\n"
                         "\tborder-bottom: 1px solid rgb(44, 49, 60);\n"
                         "}\n"
                         "QTableWidget::item{\n"
                         "\tborder-color: rgb(44, 49, 60);\n"
                         "\tpadding-left: 5px;\n"
                         "\tpadding-right: 5px;\n"
                         "\tgridline-color: rgb(44, 49, 60);\n"
                         "}\n"
                         "QTableWidget::item:selected{\n"
                         "\tbackground-color: rgb(189, 147, 249);\n"
                         "}\n"
                         "QHeaderView::section{\n"
                         "\tbackground-color: rgb(33, 37, 43);\n"
                         "\tmax-width: 30px;\n"
                         "\tborder: 1px solid rgb(44, 49, 58);\n"
                         "\tborder-style: none;\n"
                         "    border-bottom: 1px solid rgb(44, 49, 60);\n"
                         "    border-right: 1px solid rgb(44, 49, 60);\n"
                         "}\n"
                         "QTableWidget::horizontalHeader {\n"
                         "\tbackground-color: rgb(33, 37, 43);\n"
                         "}\n"
                         "QHeaderView::section:horizontal\n"
                         "{\n"
                         "    border: 1px solid rgb(33, 37, 43);\n"
                         "\tbackground-color: rgb(33, 37, 43);\n"
                         "\tpadding: 3px;\n"
                         "\tborder-top-left-radius: 7px;\n"
                         "    border-top-right-radius: 7px;\n"
                         "}\n"
                         "QHeaderView::section:vertical\n"
                         "{\n"
                         "    border: 1px solid rgb(44, 49, 60);\n"
                         "}\n");
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setColumnCount(6);
    table->horizontalHeader()->setDefaultSectionSize(85);
    table->setHorizontalHeaderLabels({tr("DeviceID"), tr("Gamepad"), tr("Mouse"), tr("Keyboard"),
                                      tr("Last Time"), tr("Operate")});
    layout->addWidget(table);

    trusted_devices_table_ = table;

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

    auto* page_about = new QWidget(ui->stackedWidget);
    page_about->setObjectName("pageAbout");
    auto* page_layout = new QVBoxLayout(page_about);

    auto* frame_shortcut = new QFrame(page_about);
    frame_shortcut->setFrameShape(QFrame::StyledPanel);
    frame_shortcut->setFrameShadow(QFrame::Raised);
    auto* shortcut_layout = new QVBoxLayout(frame_shortcut);

    auto* label_shortcut = new QLabel(frame_shortcut);
    label_shortcut->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    label_shortcut->setText(tr("Shotcut key"));
    shortcut_layout->addWidget(label_shortcut);

    auto* fullscreen_row = new QHBoxLayout();
    auto* label_fullscreen_name = new QLabel(frame_shortcut);
    label_fullscreen_name->setText(tr("Switch Fullscreen"));
    auto* label_fullscreen_keys = new QLabel(frame_shortcut);
    label_fullscreen_keys->setText(QStringLiteral("Win+Shift+Z"));
    fullscreen_row->addWidget(label_fullscreen_name);
    fullscreen_row->addWidget(label_fullscreen_keys);
    shortcut_layout->addLayout(fullscreen_row);

    auto* mouse_mode_row = new QHBoxLayout();
    auto* label_mouse_mode_name = new QLabel(frame_shortcut);
    label_mouse_mode_name->setText(tr("Mouse mode"));
    auto* label_mouse_mode_keys = new QLabel(frame_shortcut);
    label_mouse_mode_keys->setText(QStringLiteral("Win+Shift+X"));
    mouse_mode_row->addWidget(label_mouse_mode_name);
    mouse_mode_row->addWidget(label_mouse_mode_keys);
    shortcut_layout->addLayout(mouse_mode_row);
    page_layout->addWidget(frame_shortcut);

    auto* frame_about = new QFrame(page_about);
    frame_about->setFrameShape(QFrame::StyledPanel);
    frame_about->setFrameShadow(QFrame::Raised);
    auto* about_layout = new QVBoxLayout(frame_about);

    auto* label_about = new QLabel(frame_about);
    label_about->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    label_about->setText(tr("Lanthing"));
    about_layout->addWidget(label_about);

    auto* label_about_content = new QLabel(frame_about);
    label_about_content->setText(
        tr("<html><head/><body><p>Lanthing is a remote control tool created by "
           "<a href=\"https://github.com/pjlt\"><span style=\" text-decoration: "
           "underline; color:#007af4;\">Project Lanthing</span></a>."
           "</p></body></html>"));
    label_about_content->setOpenExternalLinks(true);
    about_layout->addWidget(label_about_content);
    page_layout->addWidget(frame_about);

    auto* frame_license = new QFrame(page_about);
    frame_license->setFrameShape(QFrame::StyledPanel);
    frame_license->setFrameShadow(QFrame::Raised);
    auto* license_layout = new QVBoxLayout(frame_license);

    auto* label_license = new QLabel(frame_license);
    label_license->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    label_license->setText(tr("License"));
    license_layout->addWidget(label_license);

    auto* label_license_content = new QLabel(frame_license);
    label_license_content->setText(
        tr("<html><head/><body><p>Lanthing release under <a "
           "href=\"https://github.com/pjlt/lanthing-pc/blob/master/LICENSE\"><span "
           "style=\" text-decoration: underline; color:#007af4;\">BSD-3-Clause "
           "license</span></a>.</p><p>Thirdparty software licenses are listed in</p><p><a "
           "href=\"https://github.com/pjlt/lanthing-pc/blob/master/third-party-licenses."
           "txt\"><span style=\" text-decoration: underline; color:#007af4;\">https://"
           "github.com/pjlt/lanthing-pc/blob/master/third-party-licenses.txt</span></a>"
           "</p></body></html>"));
    label_license_content->setOpenExternalLinks(true);
    license_layout->addWidget(label_license_content);
    page_layout->addWidget(frame_license);

    if (old_page != nullptr) {
        ui->stackedWidget->removeWidget(old_page);
        old_page->deleteLater();
    }
    ui->stackedWidget->insertWidget(about_index, page_about);
}

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

void MainWindow::addOrUpdateTrustedDevice(int64_t device_id, int64_t time_s) {
    postToUiThread([this, device_id, time_s]() {
        addOrUpdateTrustedDevice(device_id, true, false, false, time_s);
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

void MainWindow::setLoginStatusInUIThread(lt::GUI::LoginStatus status) {
    switch (status) {
    case lt::GUI::LoginStatus::Connected:
        // ui->statusBarLayout->removeWidget(login_progress_);
        // login_progress_->setVisible(false);
        link_label_login_info_->setText(tr("🟢Connected to server"));
        break;
    case lt::GUI::LoginStatus::Connecting:
        // ui->statusBarLayout->addWidget(login_progress_);
        // login_progress_->setVisible(true);
        //  login label style reserved
        link_label_login_info_->setText(tr("🟡Connecting..."));
        break;
    case lt::GUI::LoginStatus::Disconnected:
    default:
        // ui->statusBarLayout->removeWidget(login_progress_);
        // login_progress_->setVisible(false);
        link_label_login_info_->setText(tr("🔴Disconnected from server"));
        if (status != lt::GUI::LoginStatus::Disconnected) {
            LOG(ERR) << "Unknown Login status " << static_cast<int32_t>(status);
        }
        break;
    }
}

void MainWindow::setServiceStatusInUIThread(lt::GUI::ServiceStatus status) {
    switch (status) {
    case lt::GUI::ServiceStatus::Up:
        link_label_controlled_info_->setText(tr("🟢Controlled module up"));
        break;
    case lt::GUI::ServiceStatus::Down:
    default:
        link_label_controlled_info_->setText(tr("🔴Controlled module down"));
        if (status != lt::GUI::ServiceStatus::Down) {
            LOG(ERR) << "Unknown ServiceStatus " << static_cast<int32_t>(status);
        }
        break;
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
    // gamepad
    QCheckBox* gamepad_item = new QCheckBox();
    gamepad_item->setChecked(gamepad);
    connect(gamepad_item, &QCheckBox::stateChanged, [this, gamepad_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Gamepad,
                                         gamepad_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 1, makeWidgetHCentered(gamepad_item));
    // mouse
    QCheckBox* mouse_item = new QCheckBox();
    mouse_item->setChecked(mouse);
    connect(mouse_item, &QCheckBox::stateChanged, [this, mouse_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Mouse,
                                         mouse_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 2, makeWidgetHCentered(mouse_item));
    // keyboard
    QCheckBox* keyboard_item = new QCheckBox();
    keyboard_item->setChecked(keyboard);
    connect(keyboard_item, &QCheckBox::stateChanged, [this, keyboard_item, device_id](int) {
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
