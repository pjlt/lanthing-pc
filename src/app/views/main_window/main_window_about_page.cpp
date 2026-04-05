/*
 * BSD 3-Clause License
 */

#include "main_window_about_page.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

namespace {
QString trMainWindow(const char* text) {
    return QCoreApplication::translate("MainWindow", text);
}
} // namespace

QWidget* MainWindowAboutPage::createPage(QWidget* parent) const {
    auto* page_about = new QWidget(parent);
    page_about->setObjectName("pageAbout");
    auto* page_layout = new QVBoxLayout(page_about);

    auto* frame_shortcut = new QFrame(page_about);
    frame_shortcut->setFrameShape(QFrame::StyledPanel);
    frame_shortcut->setFrameShadow(QFrame::Raised);
    auto* shortcut_layout = new QVBoxLayout(frame_shortcut);

    auto* label_shortcut = new QLabel(frame_shortcut);
    label_shortcut->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    label_shortcut->setText(trMainWindow("Shotcut key"));
    shortcut_layout->addWidget(label_shortcut);

    auto* fullscreen_row = new QHBoxLayout();
    auto* label_fullscreen_name = new QLabel(frame_shortcut);
    label_fullscreen_name->setText(trMainWindow("Switch Fullscreen"));
    auto* label_fullscreen_keys = new QLabel(frame_shortcut);
    label_fullscreen_keys->setText(QStringLiteral("Win+Shift+Z"));
    fullscreen_row->addWidget(label_fullscreen_name);
    fullscreen_row->addWidget(label_fullscreen_keys);
    shortcut_layout->addLayout(fullscreen_row);

    auto* mouse_mode_row = new QHBoxLayout();
    auto* label_mouse_mode_name = new QLabel(frame_shortcut);
    label_mouse_mode_name->setText(trMainWindow("Mouse mode"));
    auto* label_mouse_mode_keys = new QLabel(frame_shortcut);
    label_mouse_mode_keys->setText(QStringLiteral("Win+Shift+X"));
    mouse_mode_row->addWidget(label_mouse_mode_name);
    mouse_mode_row->addWidget(label_mouse_mode_keys);
    shortcut_layout->addLayout(mouse_mode_row);
    page_layout->addWidget(frame_shortcut);

    auto* frame_about = new QFrame(page_about);
    frame_about->setFrameShape(QFrame::StyledPanel);
    frame_about->setFrameShadow(QFrame::Raised);
    auto* about_layout = new QVBoxLayout(frame_about);

    auto* label_about = new QLabel(frame_about);
    label_about->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    label_about->setText(trMainWindow("Lanthing"));
    about_layout->addWidget(label_about);

    auto* label_about_content = new QLabel(frame_about);
    label_about_content->setText(
        trMainWindow("<html><head/><body><p>Lanthing is a remote control tool created by "
                 "<a href=\"https://github.com/pjlt\"><span style=\" text-decoration: "
                 "underline; color:#007af4;\">Project Lanthing</span></a>."
                 "</p></body></html>"));
    label_about_content->setOpenExternalLinks(true);
    about_layout->addWidget(label_about_content);
    page_layout->addWidget(frame_about);

    auto* frame_license = new QFrame(page_about);
    frame_license->setFrameShape(QFrame::StyledPanel);
    frame_license->setFrameShadow(QFrame::Raised);
    auto* license_layout = new QVBoxLayout(frame_license);

    auto* label_license = new QLabel(frame_license);
    label_license->setStyleSheet("font: 16pt \"Microsoft YaHei UI\";");
    label_license->setText(trMainWindow("License"));
    license_layout->addWidget(label_license);

    auto* label_license_content = new QLabel(frame_license);
    label_license_content->setText(
        trMainWindow("<html><head/><body><p>Lanthing release under <a "
                 "href=\"https://github.com/pjlt/lanthing-pc/blob/master/LICENSE\"><span "
                 "style=\" text-decoration: underline; color:#007af4;\">BSD-3-Clause "
                 "license</span></a>.</p><p>Thirdparty software licenses are listed in</p><p><a "
                 "href=\"https://github.com/pjlt/lanthing-pc/blob/master/third-party-licenses."
                 "txt\"><span style=\" text-decoration: underline; color:#007af4;\">https://"
                 "github.com/pjlt/lanthing-pc/blob/master/third-party-licenses.txt</span></a>"
                 "</p></body></html>"));
    label_license_content->setOpenExternalLinks(true);
    license_layout->addWidget(label_license_content);
    page_layout->addWidget(frame_license);

    return page_about;
}
