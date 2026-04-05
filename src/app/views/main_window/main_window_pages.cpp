/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

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
