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
        char buf[1024] = {0};
        snprintf(buf, sizeof(buf) - 1, "login ret: %s", code == ErrCode::OK ? "ok" : "not ok");
        QMessageBox::information(this, "info", buf, QMessageBox::Discard);
    });
}

void MainWindow::onInviteRet(ErrCode code, const std::string& err) {
    ;
}

void MainWindow::doInvite(const std::string& dev_id, const std::string& token) {
    (void)token;
    int64_t deviceID;
    if (bool success = ltlib::String::getValue(dev_id, &deviceID)) {
        app->connect(deviceID);
    }
    else {
        LOG(FATAL) << "Parse deviceID(" << dev_id << ") to int64_t failed!";
    }
}
