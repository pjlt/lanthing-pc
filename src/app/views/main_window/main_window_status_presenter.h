/*
 * BSD 3-Clause License
 */

#pragma once

#include <app/views/gui.h>

class QLabel;

class MainWindowStatusPresenter {
public:
    MainWindowStatusPresenter(QLabel* login_label, QLabel* service_label);

    void setLoginStatus(lt::GUI::LoginStatus status) const;

    void setServiceStatus(lt::GUI::ServiceStatus status) const;

private:
    QLabel* login_label_ = nullptr;
    QLabel* service_label_ = nullptr;
};
