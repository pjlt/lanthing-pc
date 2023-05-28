#ifndef MAINPAGE_H
#define MAINPAGE_H

#include <QtWidgets/QWidget>

namespace Ui {
class MainPage;
}

namespace mdesk {
class App;
};

class MainPage : public QWidget {
    Q_OBJECT

public:
    explicit MainPage(QWidget* parent = nullptr);
    ~MainPage();

Q_SIGNALS:
    void onConnectBtnPressed1(const std::string& dev_id, const std::string& token);

private:
    void onConnectBtnPressed();

private:
    Ui::MainPage* ui;
};

#endif // MAINPAGE_H
