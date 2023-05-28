#ifndef MENU_H
#define MENU_H

#include <QtWidgets/QWidget>

namespace Ui {
class Menu;
}

class Menu : public QWidget
{
    Q_OBJECT

public:
    explicit Menu(QWidget *parent = nullptr);
    ~Menu();
Q_SIGNALS:
    void pageSelect(const int page_index);

private:
    void onMainPageClicked();
    void onSettingPageClicked();

private:
    Ui::Menu* ui;
};

#endif // MENU_H
