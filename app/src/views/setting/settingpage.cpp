#include "settingpage.h"
#include "ui_settingpage.h"

SettingPage::SettingPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SettingPage) {
    ui->setupUi(parent);
}

SettingPage::~SettingPage() {
    delete ui;
}
