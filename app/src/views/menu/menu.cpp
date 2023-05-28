#include "menu.h"
#include "ui_menu.h"

Menu::Menu(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Menu) {
    ui->setupUi(parent);

    connect(ui->main_page_btn, &QPushButton::pressed, [this]() { emit pageSelect(0); });

    connect(ui->settting_page_btn, &QPushButton::pressed, [this]() { emit pageSelect(1); });
}

Menu::~Menu() {
    delete ui;
}

void Menu::onMainPageClicked() {
    ;
}

void Menu::onSettingPageClicked() {
    ;
}
