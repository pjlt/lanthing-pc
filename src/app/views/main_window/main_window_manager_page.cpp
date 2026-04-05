/*
 * BSD 3-Clause License
 */

#include "main_window_manager_page.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qtablewidget.h>
#include <QtWidgets/qwidget.h>

namespace {
QString trMainWindow(const char* text) {
    return QCoreApplication::translate("MainWindow", text);
}
} // namespace

MainWindowManagerPageView MainWindowManagerPage::createPage(QWidget* parent) const {
    auto* page_mgr = new QWidget(parent);
    page_mgr->setObjectName("pageMgr");
    auto* layout = new QVBoxLayout(page_mgr);

    auto* title = new QLabel(page_mgr);
    title->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    title->setText(trMainWindow("Trusted clients:"));
    layout->addWidget(title);

    auto* table = new QTableWidget(page_mgr);
    table->setObjectName("tableWidget");
    table->setStyleSheet("QTableWidget {\n"
                         "\tbackground-color: transparent;\n"
                         "\tpadding: 10px;\n"
                         "\tborder-radius: 5px;\n"
                         "\tgridline-color: rgb(44, 49, 58);\n"
                         "\tborder-bottom: 1px solid rgb(44, 49, 60);\n"
                         "}\n"
                         "QTableWidget::item{\n"
                         "\tborder-color: rgb(44, 49, 60);\n"
                         "\tpadding-left: 5px;\n"
                         "\tpadding-right: 5px;\n"
                         "\tgridline-color: rgb(44, 49, 60);\n"
                         "}\n"
                         "QTableWidget::item:selected{\n"
                         "\tbackground-color: rgb(189, 147, 249);\n"
                         "}\n"
                         "QHeaderView::section{\n"
                         "\tbackground-color: rgb(33, 37, 43);\n"
                         "\tmax-width: 30px;\n"
                         "\tborder: 1px solid rgb(44, 49, 58);\n"
                         "\tborder-style: none;\n"
                         "    border-bottom: 1px solid rgb(44, 49, 60);\n"
                         "    border-right: 1px solid rgb(44, 49, 60);\n"
                         "}\n"
                         "QTableWidget::horizontalHeader {\n"
                         "\tbackground-color: rgb(33, 37, 43);\n"
                         "}\n"
                         "QHeaderView::section:horizontal\n"
                         "{\n"
                         "    border: 1px solid rgb(33, 37, 43);\n"
                         "\tbackground-color: rgb(33, 37, 43);\n"
                         "\tpadding: 3px;\n"
                         "\tborder-top-left-radius: 7px;\n"
                         "    border-top-right-radius: 7px;\n"
                         "}\n"
                         "QHeaderView::section:vertical\n"
                         "{\n"
                         "    border: 1px solid rgb(44, 49, 60);\n"
                         "}\n");
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setColumnCount(6);
    table->horizontalHeader()->setDefaultSectionSize(85);
    table->setHorizontalHeaderLabels({trMainWindow("DeviceID"), trMainWindow("Gamepad"),
                                      trMainWindow("Mouse"), trMainWindow("Keyboard"),
                                      trMainWindow("Last Time"), trMainWindow("Operate")});
    layout->addWidget(table);

    MainWindowManagerPageView view;
    view.page = page_mgr;
    view.trusted_devices_table = table;
    return view;
}
