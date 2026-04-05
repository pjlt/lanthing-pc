/*
 * BSD 3-Clause License
 */

#pragma once

class QWidget;
class QTableWidget;

struct MainWindowManagerPageView {
    QWidget* page = nullptr;
    QTableWidget* trusted_devices_table = nullptr;
};

class MainWindowManagerPage {
public:
    MainWindowManagerPageView createPage(QWidget* parent) const;
};
