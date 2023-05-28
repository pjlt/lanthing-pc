#include "mainpage.h"

#include <QAction>

#include "app.h"
#include "ui_mainpage.h"

MainPage::MainPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MainPage) {

    ui->setupUi(parent);

    QAction* action1 = new QAction();
    action1->setIcon(QIcon(":/icons/icons/pc.png"));
    ui->device_id->addAction(action1, QLineEdit::LeadingPosition);

    QAction* action2 = new QAction();
    action2->setIcon(QIcon(":/icons/icons/lock.png"));
    ui->access_token->addAction(action2, QLineEdit::LeadingPosition);

    connect(ui->connect_btn, &QPushButton::pressed, [this]() { onConnectBtnPressed(); });
}

MainPage::~MainPage() {
    delete ui;
}

void MainPage::onConnectBtnPressed() {
    auto dev_id = ui->device_id->text();
    auto token = ui->access_token->text();

    emit onConnectBtnPressed1(dev_id.toStdString(), token.toStdString());
}
