/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 */

#include "main_window_private.h"

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

bool MainWindow::eventFilter(QObject* obj, QEvent* evt) {
    if (obj == ui->windowBar && evt->type() == QEvent::MouseButtonPress) {
        QMouseEvent* ev = static_cast<QMouseEvent*>(evt);
        if (ev->buttons() & Qt::LeftButton) {
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
