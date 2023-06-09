#ifndef MENU_H
#define MENU_H

#include <QtWidgets/QWidget>

#include "views/components/progress_widget.h"

namespace Ui {
class Menu;
}

class Menu : public QWidget {
    Q_OBJECT

public:
    enum class LoginStatus : uint8_t {
        LOGINING = 0,
        LOGIN_SUCCESS = 1,
        LOGIN_FAILED = 2,
    };

public:
    explicit Menu(QWidget* parent = nullptr);
    ~Menu();

    void setLoginStatus(LoginStatus status);

Q_SIGNALS:
    void pageSelect(const int page_index);

private:
    Ui::Menu* ui;

    qt_componets::ProgressWidget* login_progress_ = nullptr;
};

#endif // MENU_H
