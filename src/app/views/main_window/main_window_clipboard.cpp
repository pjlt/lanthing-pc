/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

void MainWindow::setClipboardText(const std::string& text) {
    postToUiThread([text]() {
        auto clipboard = QApplication::clipboard();
        auto text2 = QString::fromStdString(text);
        if (clipboard->text() == text2) {
            return;
        }
        clipboard->setText(QString::fromStdString(text));
    });
}

void MainWindow::setupClipboard() {
    auto clipboard = QApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, [clipboard, this]() {
        auto md = clipboard->mimeData();
        if (md == nullptr) {
            LOG(WARNING) << "Clipboard::mimeData is null";
            return;
        }
        for (auto& f : md->formats()) {
            if (f == "text/plain") {
                onClipboardPlainTextChanged();
                return;
            }
            else if (f == "application/x-qt-windows-mime;value=\"FileName\"") { // TODO:
                onClipboardFileChanged();
                return;
            }
        }
    });
}

void MainWindow::onClipboardPlainTextChanged() {
    auto clipboard = QApplication::clipboard();
    if (clipboard->text().isEmpty()) {
        return;
    }
    constexpr size_t kMaxSize = 128 * 1024;
    auto text = clipboard->text().toStdString();
    if (text.size() > kMaxSize) {
        LOG(INFO) << "Detected clipboard text change, but too large to send. size:" << text.size();
        return;
    }
    params_.on_clipboard_text(text);
}

void MainWindow::onClipboardFileChanged() {
    auto clipboard = QApplication::clipboard();
    std::string fullpath = clipboard->text().toStdString();
    auto pos = fullpath.find("file:///");
    if (pos != std::string::npos) {
        fullpath = fullpath.substr(8);
    }
    if (fullpath.empty()) {
        LOG(WARNING) << "Clipboard file path is empty";
        return;
    }

    std::filesystem::path path;
    try {
        auto u8path = reinterpret_cast<const char8_t*>(fullpath.c_str());
        path = std::filesystem::path{u8path};
    } catch (const std::exception& e) {
        LOG(WARNING) << "Construct std::filesystem::path from '" << fullpath << "' failed with "
                     << e.what();
        return;
    }
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }
    uint64_t file_size = 0;
    try {
        file_size = std::filesystem::file_size(path);
    } catch (const std::exception& e) {
        LOG(WARNING) << "std::filesystem::file_size(" << fullpath << ") failed with " << e.what();
        return;
    }
    if (file_size == 0) {
        LOG(INFO) << "Clipboard file size == 0";
        return;
    }
    params_.on_clipboard_file(fullpath, file_size);
}
