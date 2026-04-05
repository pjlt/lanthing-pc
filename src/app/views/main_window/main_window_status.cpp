/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

#include "main_window_status_presenter.h"

void MainWindow::setLoginStatus(lt::GUI::LoginStatus status) {
    postToUiThread([this, status]() { setLoginStatusInUIThread(status); });
}

void MainWindow::setServiceStatus(lt::GUI::ServiceStatus status) {
    postToUiThread([this, status]() { setServiceStatusInUIThread(status); });
}

void MainWindow::setDeviceID(int64_t device_id) {
    postToUiThread([this, device_id]() {
        device_id_ = device_id;
        std::string id = std::to_string(device_id);
        std::string id2;
        for (size_t i = 0; i < id.size(); i++) {
            id2.push_back(id[i]);
            if (i % 3 == 2 && i != id.size() - 1) {
                id2.push_back(' ');
            }
        }
        link_label_my_device_id_->setText(QString::fromStdString(id2));
    });
}

void MainWindow::setAccessToken(const std::string& access_token) {
    postToUiThread([this, access_token]() {
        access_token_text_ = access_token;
        if (token_showing_) {
            link_label_my_access_token_->setText(QString::fromStdString(access_token));
        }
    });
}

void MainWindow::onConfirmConnection(int64_t device_id) {
    postToUiThread([this, device_id]() {
        QMessageBox msgbox;
        msgbox.setWindowTitle(tr("New Connection"));
        std::string id_str = std::to_string(device_id);
        QString message = tr("Device %s is requesting connection");
        std::vector<char> buffer(128);
        snprintf(buffer.data(), buffer.size(), message.toStdString().c_str(), id_str.c_str());
        msgbox.setText(buffer.data());
        auto btn_accept = msgbox.addButton(tr("Accept"), QMessageBox::ButtonRole::YesRole);
        auto btn_accept_next_time =
            msgbox.addButton(tr("Accept, as well as next time"), QMessageBox::ButtonRole::YesRole);
        auto btn_reject = msgbox.addButton(tr("Reject"), QMessageBox::ButtonRole::RejectRole);
        msgbox.exec();
        auto clicked_btn = msgbox.clickedButton();
        lt::GUI::ConfirmResult result = lt::GUI::ConfirmResult::Reject;
        if (clicked_btn == btn_accept) {
            result = lt::GUI::ConfirmResult::Accept;
            LOG(INFO) << "User accept";
        }
        else if (clicked_btn == btn_accept_next_time) {
            result = lt::GUI::ConfirmResult::AcceptWithNextTime;
            LOG(INFO) << "User accept, as well as next time";
        }
        else if (clicked_btn == btn_reject) {
            result = lt::GUI::ConfirmResult::Reject;
            LOG(INFO) << "User reject";
        }
        else {
            result = lt::GUI::ConfirmResult::Reject;
            LOG(INFO) << "Unknown button, treat as reject";
        }
        params_.on_user_confirmed_connection(device_id, result);
    });
}

void MainWindow::onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    postToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::service2app::ConnectionStatus>(_msg);
        if (!peer_client_device_id_.has_value()) {
            LOG(WARNING) << "Received ConnectionStatus, but we are not serving any client, "
                            "received device_id:"
                         << msg->device_id();
            return;
        }
        if (peer_client_device_id_.value() != msg->device_id()) {
            LOG(WARNING) << "Received ClientStatus with " << msg->device_id()
                         << ", but we are serving " << peer_client_device_id_.value();
            return;
        }
        double Mbps = static_cast<double>(msg->bandwidth_bps()) / 1000. / 1000.;
        int32_t delay_ms = msg->delay_ms();
        p2p_ = msg->p2p();
        mouse_hit_time_ = msg->hit_mouse() ? ltlib::steady_now_ms() : mouse_hit_time_;
        keyboard_hit_time_ = msg->hit_keyboard() ? ltlib::steady_now_ms() : keyboard_hit_time_;
        gamepad_hit_time_ = msg->hit_gamepad() ? ltlib::steady_now_ms() : gamepad_hit_time_;
        std::ostringstream oss;
        oss << msg->device_id() << " " << delay_ms * 2 << "ms " << std::fixed
            << std::setprecision(1) << Mbps << "Mbps " << video_codec_ << " "
            << (p2p_ ? "P2P " : "Relay ") << (gpu_decode_ ? "GPU:" : "CPU:")
            << (gpu_encode_ ? "GPU " : "CPU ");
        link_label_client1_->setToolTip(QString::fromStdString(oss.str()));
    });
}

void MainWindow::onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    postToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::service2app::AcceptedConnection>(_msg);
        if (peer_client_device_id_.has_value()) {
            if (peer_client_device_id_.value() == msg->device_id()) {
                LOG(WARNING) << "Received same AccpetedConnection " << msg->device_id();
                return;
            }
            else {
                // 暂时只支持一个客户端
                LOGF(ERR,
                     "Received AcceptedConnection(%" PRId64
                     "), but we are serving another client(%" PRId64 ")",
                     msg->device_id(), peer_client_device_id_.value());
                return;
            }
        }
        enable_gamepad_ = msg->enable_gamepad();
        enable_keyboard_ = msg->enable_keyboard();
        enable_mouse_ = msg->enable_mouse();
        enable_audio_ = msg->enable_audio();
        gpu_encode_ = msg->gpu_encode();
        gpu_decode_ = msg->gpu_decode();
        p2p_ = msg->p2p();
        video_codec_ = ltproto::common::VideoCodecType_Name(msg->video_codec());
        // 为了防止protobuf名字冲突，未知类型定义为"UnknownVCT"(UnknownVideoCodecType)，但是这个名字展示给用户并不好看
        if (msg->video_codec() == ltproto::common::UnknownVCT) {
            video_codec_ = "?";
        }
        peer_client_device_id_ = msg->device_id();
        std::ostringstream oss;
        oss << msg->device_id() << " ?ms ?Mbps " << video_codec_ << " "
            << (p2p_ ? "P2P " : "Relay ") << (gpu_encode_ ? "GPU:" : "CPU:")
            << (gpu_decode_ ? "GPU " : "CPU ");
        link_label_client1_->setToolTip(QString::fromStdString(oss.str()));
        link_indicator1_->show();
        QTimer::singleShot(50, this, &MainWindow::onUpdateIndicator);
    });
}

void MainWindow::onDisconnectedConnection(int64_t device_id) {
    postToUiThread([this, device_id]() {
        if (!peer_client_device_id_.has_value()) {
            LOG(ERR) << "Received DisconnectedClient, but no connected client";
            return;
        }
        if (peer_client_device_id_.value() != device_id) {
            LOGF(ERR,
                 "Received DisconnectedClient, but device_id(%" PRId64
                 ") != peer_client_device_id_(%" PRId64 ")",
                 device_id, peer_client_device_id_.value());
            return;
        }
        link_indicator1_->hide();
        peer_client_device_id_ = std::nullopt;
        gpu_encode_ = false;
        gpu_decode_ = false;
        p2p_ = false;
        bandwidth_bps_ = false;
        video_codec_ = "?";
        enable_gamepad_ = false;
        enable_keyboard_ = false;
        enable_mouse_ = false;
    });
}

void MainWindow::errorMessageBox(const QString& message) {
    postToUiThread([message]() {
        QMessageBox msgbox;
        msgbox.setText(message);
        msgbox.setIcon(QMessageBox::Icon::Critical);
        msgbox.exec();
    });
}

void MainWindow::infoMessageBox(const QString& message) {
    postToUiThread([message]() {
        QMessageBox msgbox;
        msgbox.setText(message);
        msgbox.setIcon(QMessageBox::Icon::Information);
        msgbox.exec();
    });
}

void MainWindow::onNewVersion(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    postToUiThread([this, _msg]() {
        auto msg = std::static_pointer_cast<ltproto::server::NewVersion>(_msg);
        std::ostringstream oss;
        int64_t version = ltlib::combineVersion(msg->major(), msg->minor(), msg->patch());
        oss << "v" << msg->major() << "." << msg->minor() << "." << msg->patch();
        if (msg->force()) {
            // 强制更新
            QString message = tr("The new version %s has been released, this is a force update "
                                 "version, please download it from <a "
                                 "href='%s'>Github</a>.");
            std::vector<char> buffer(512);
            snprintf(buffer.data(), buffer.size(), message.toStdString().c_str(), oss.str().c_str(),
                     msg->url().c_str());
            QMessageBox msgbox;
            msgbox.setTextFormat(Qt::TextFormat::RichText);
            msgbox.setWindowTitle(tr("New Version"));
            msgbox.setText(buffer.data());
            msgbox.setStandardButtons(QMessageBox::Ok);
            msgbox.exec();
            ::exit(0);
        }
        QString message = tr("The new version %s has been released, please download it<br>from <a "
                             "href='%s'>Github</a>.");
        std::vector<char> buffer(512);
        snprintf(buffer.data(), buffer.size(), message.toStdString().c_str(), oss.str().c_str(),
                 msg->url().c_str());
        oss.clear();

        auto date = QDateTime::fromSecsSinceEpoch(msg->timestamp());
        QString details = tr("Version: ") % "v" % QString::number(msg->major()) % "." %
                          QString::number(msg->minor()) % "." % QString::number(msg->patch()) %
                          "\n\n" % tr("Released date: ") %
                          date.toLocalTime().toString("yyyy/MM/dd") % "\n\n" % tr("New features:") %
                          "\n";
        for (int i = 0; i < msg->features_size(); i++) {
            details = details % QString::number(i + 1) % ". " %
                      QString::fromStdString(msg->features().Get(i)) % "\n";
        }
        details = details % "\n" % tr("Bug fix:") % "\n";
        for (int i = 0; i < msg->bugfix_size(); i++) {
            details = details % QString::number(i + 1) % ". " %
                      QString::fromStdString(msg->bugfix().Get(i)) % "\n";
        }

        QMessageBox msgbox;
        msgbox.setTextFormat(Qt::TextFormat::RichText);
        msgbox.setWindowTitle(tr("New Version"));
        msgbox.setText(buffer.data());
        msgbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Ignore);
        msgbox.setDetailedText(details);
        int ret = msgbox.exec();
        switch (ret) {
        case QMessageBox::Ignore:
            params_.ignore_version(version);
            break;
        default:
            break;
        }
    });
}

void MainWindow::setLoginStatusInUIThread(lt::GUI::LoginStatus status) {
    if (status_presenter_ == nullptr) {
        LOG(ERR) << "Status presenter is null";
        return;
    }
    status_presenter_->setLoginStatus(status);
}

void MainWindow::setServiceStatusInUIThread(lt::GUI::ServiceStatus status) {
    if (status_presenter_ == nullptr) {
        LOG(ERR) << "Status presenter is null";
        return;
    }
    status_presenter_->setServiceStatus(status);
}
