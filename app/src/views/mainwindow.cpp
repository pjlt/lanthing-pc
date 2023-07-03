#include "mainwindow.h"

#include <cassert>

#include <g3log/g3log.hpp>
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
    pages_layout->addWidget(main_page);
    pages_layout->addWidget(setting_page);

    layout->addLayout(pages_layout);

    menu_ui = new Menu(menu);
    main_page_ui = new MainPage(main_page);
    setting_page_ui = new SettingPage(setting_page);

    connect(menu_ui, &Menu::pageSelect,
            [pages_layout](const int index) { pages_layout->setCurrentIndex(index); });
    connect(
        main_page_ui, &MainPage::onConnectBtnPressed1,
        [this](const std::string& dev_id, const std::string& token) { doInvite(dev_id, token); });

    // FIXME: 还没有实现"登录逻辑"
    menu_ui->setLoginStatus(Menu::LoginStatus::LOGINING);
}

MainWindow::~MainWindow() {
    delete ui;
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
        if (code != ErrCode::OK) {
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_FAILED);
        }
        else if (code == ErrCode::OK) {
            menu_ui->setLoginStatus(Menu::LoginStatus::LOGIN_SUCCESS);
        }
    });
}

void MainWindow::onInviteRet(ErrCode code, const std::string& err) {
    ;
}

void MainWindow::onLocalDeviceID(int64_t device_id) {
    DispatchToMainThread([this, device_id]() { main_page_ui->onUpdateLocalDeviceID(device_id); });
}

void MainWindow::onLocalAccessToken(const std::string& access_token) {
    DispatchToMainThread(
        [this, access_token]() { main_page_ui->onUpdateLocalAccessToken(access_token); });
}

void MainWindow::doInvite(const std::string& dev_id, const std::string& token) {
    int64_t deviceID;
    if (bool success = ltlib::String::getValue(dev_id, &deviceID)) {
        app->connect(deviceID, token);
    }
    else {
        LOG(FATAL) << "Parse deviceID(" << dev_id << ") to int64_t failed!";
    }
}
