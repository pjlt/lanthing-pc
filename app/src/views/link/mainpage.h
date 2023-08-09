#ifndef MAINPAGE_H
#define MAINPAGE_H

#include <vector>
#include <string>

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
    explicit MainPage(const std::vector<std::string>& history_device_ids,
                      QWidget* parent = nullptr);
    ~MainPage();
    void onUpdateLocalDeviceID(int64_t device_id);
    void onUpdateLocalAccessToken(const std::string& access_token);

Q_SIGNALS:
    void onConnectBtnPressed1(const std::string& dev_id, const std::string& token);

private:
    void onConnectBtnPressed();

private:
    Ui::MainPage* ui;
    std::vector<std::string> history_device_ids_;
};

#endif // MAINPAGE_H
