#include "menu.h"
#include "ui_menu.h"

QColor toColor(QString colorstr) {

    int r = colorstr.mid(1, 2).toInt(nullptr, 16);
    int g = colorstr.mid(3, 2).toInt(nullptr, 16);
    int b = colorstr.mid(5, 2).toInt(nullptr, 16);
    QColor color = QColor(r, g, b);
    return color;
}

Menu::Menu(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Menu) {
    ui->setupUi(parent);

    connect(ui->main_page_btn, &QPushButton::pressed, [this]() { emit pageSelect(0); });
    connect(ui->settting_page_btn, &QPushButton::pressed, [this]() { emit pageSelect(1); });
    login_progress_ = new qt_componets::ProgressWidget();
    login_progress_->setVisible(false);
    login_progress_->setProgressColor(toColor("#8198ff"));
    ui->login_status_layout->addStretch();
}

Menu::~Menu() {
    delete ui;
}

void Menu::setLoginStatus(LoginStatus status) {
    if (status == LoginStatus::LOGINING) {
        ui->login_status_layout->addWidget(login_progress_);
        login_progress_->setVisible(true);
        ui->info_label->setStyleSheet("QLabel{}");
    }
    else if (status == LoginStatus::LOGIN_SUCCESS) {
        ui->login_status_layout->removeWidget(login_progress_);
        login_progress_->setVisible(false);
        ui->info_label->setText("connected with server");
        ui->info_label->setStyleSheet("QLabel{}");
    }
    else if (status == LoginStatus::LOGIN_FAILED) {
        ui->login_status_layout->removeWidget(login_progress_);
        login_progress_->setVisible(false);
        ui->info_label->setText("disconnected with server");
        ui->info_label->setStyleSheet("QLabel{color: red}");
    }
}
