/*
 * BSD 3-Clause License
 */

#pragma once

#include <QtCore/qstring.h>

class QPushButton;
class QStackedWidget;

class Ui_MainWindow;

class MainWindowNavigator {
public:
    explicit MainWindowNavigator(Ui_MainWindow* ui);

    void switchToMainPage();

    void switchToSettingPage();

    void switchToManagerPage();

    void switchToAboutPage();

    QPushButton* indexToTabButton(int32_t index) const;

    static void swapTabBtnStyleSheet(QPushButton* old_selected, QPushButton* new_selected);

    int indexOfPageByObjectName(const QString& object_name) const;

private:
    bool switchToPage(const QString& object_name, QPushButton* tab_button,
                      const char* page_label) const;

private:
    Ui_MainWindow* ui_ = nullptr;
};
