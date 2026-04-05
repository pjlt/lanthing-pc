/*
 * BSD 3-Clause License
 */

#include "main_window_status_presenter.h"

#include <QtWidgets/qlabel.h>

#include <ltlib/logging.h>

MainWindowStatusPresenter::MainWindowStatusPresenter(QLabel* login_label, QLabel* service_label)
    : login_label_(login_label)
    , service_label_(service_label) {
}

void MainWindowStatusPresenter::setLoginStatus(lt::GUI::LoginStatus status) const {
    if (login_label_ == nullptr) {
        LOG(ERR) << "Login status label is null";
        return;
    }

    switch (status) {
    case lt::GUI::LoginStatus::Connected:
        login_label_->setText(QObject::tr("🟢Connected to server"));
        break;
    case lt::GUI::LoginStatus::Connecting:
        login_label_->setText(QObject::tr("🟡Connecting..."));
        break;
    case lt::GUI::LoginStatus::Disconnected:
        login_label_->setText(QObject::tr("🔴Disconnected from server"));
        break;
    default:
        login_label_->setText(QObject::tr("🔴Disconnected from server"));
        LOG(ERR) << "Unknown Login status " << static_cast<int32_t>(status);
        break;
    }
}

void MainWindowStatusPresenter::setServiceStatus(lt::GUI::ServiceStatus status) const {
    if (service_label_ == nullptr) {
        LOG(ERR) << "Service status label is null";
        return;
    }

    switch (status) {
    case lt::GUI::ServiceStatus::Up:
        service_label_->setText(QObject::tr("🟢Controlled module up"));
        break;
    case lt::GUI::ServiceStatus::Down:
        service_label_->setText(QObject::tr("🔴Controlled module down"));
        break;
    default:
        service_label_->setText(QObject::tr("🔴Controlled module down"));
        LOG(ERR) << "Unknown ServiceStatus " << static_cast<int32_t>(status);
        break;
    }
}
