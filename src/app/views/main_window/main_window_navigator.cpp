/*
 * BSD 3-Clause License
 */

#include "main_window_navigator.h"

#include "ui_main_window.h"

#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstackedwidget.h>

#include <ltlib/logging.h>

MainWindowNavigator::MainWindowNavigator(Ui_MainWindow* ui)
    : ui_(ui) {
}

void MainWindowNavigator::switchToMainPage() {
    switchToPage(QStringLiteral("pageLink"), ui_->btnLinkTab, "Main");
}

void MainWindowNavigator::switchToManagerPage() {
    switchToPage(QStringLiteral("pageMgr"), ui_->btnManagerTab, "Manager");
}

void MainWindowNavigator::switchToSettingPage() {
    switchToPage(QStringLiteral("pageSettings"), ui_->btnSettingsTab, "Settings");
}

void MainWindowNavigator::switchToAboutPage() {
    switchToPage(QStringLiteral("pageAbout"), ui_->btnAboutTab, "About");
}

QPushButton* MainWindowNavigator::indexToTabButton(int32_t index) const {
    if (ui_->stackedWidget == nullptr || index < 0 || index >= ui_->stackedWidget->count()) {
        LOG(ERR) << "Unknown tab index!";
        return ui_->btnLinkTab;
    }

    QWidget* page = ui_->stackedWidget->widget(index);
    if (page == nullptr) {
        LOG(ERR) << "Page is null for tab index " << index;
        return ui_->btnLinkTab;
    }

    const QString page_name = page->objectName();
    if (page_name == QStringLiteral("pageLink")) {
        return ui_->btnLinkTab;
    }
    if (page_name == QStringLiteral("pageMgr")) {
        return ui_->btnManagerTab;
    }
    if (page_name == QStringLiteral("pageSettings")) {
        return ui_->btnSettingsTab;
    }
    if (page_name == QStringLiteral("pageAbout")) {
        return ui_->btnAboutTab;
    }

    LOG(ERR) << "Unknown page name: " << page_name.toStdString();
    return ui_->btnLinkTab;
}

void MainWindowNavigator::swapTabBtnStyleSheet(QPushButton* old_selected,
                                                QPushButton* new_selected) {
    QString stylesheet = new_selected->styleSheet();
    new_selected->setStyleSheet(old_selected->styleSheet());
    old_selected->setStyleSheet(stylesheet);
}

int MainWindowNavigator::indexOfPageByObjectName(const QString& object_name) const {
    if (ui_->stackedWidget == nullptr) {
        return -1;
    }

    for (int i = 0; i < ui_->stackedWidget->count(); i++) {
        QWidget* page = ui_->stackedWidget->widget(i);
        if (page != nullptr && page->objectName() == object_name) {
            return i;
        }
    }
    return -1;
}

bool MainWindowNavigator::switchToPage(const QString& object_name, QPushButton* tab_button,
                                       const char* page_label) const {
    const int index = indexOfPageByObjectName(object_name);
    if (index < 0) {
        LOG(ERR) << page_label << " page not found";
        return false;
    }

    if (ui_->stackedWidget->currentIndex() != index) {
        swapTabBtnStyleSheet(indexToTabButton(ui_->stackedWidget->currentIndex()), tab_button);
        ui_->stackedWidget->setCurrentIndex(index);
    }

    return true;
}
