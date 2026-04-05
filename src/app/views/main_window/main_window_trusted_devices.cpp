/*
 * BSD 3-Clause License
 */

#include "main_window_private.h"

void MainWindow::addOrUpdateTrustedDevice(int64_t device_id, int64_t time_s) {
    postToUiThread([this, device_id, time_s]() {
        addOrUpdateTrustedDevice(device_id, true, false, false, time_s);
    });
}

void MainWindow::addOrUpdateTrustedDevices() {
    auto trusted_devices = params_.get_trusted_devices();
    for (auto& device : trusted_devices) {
        addOrUpdateTrustedDevice(device.device_id, device.gamepad, device.mouse, device.keyboard,
                                 device.last_access_time_s);
    }
    // 简化逻辑，固定5秒查一次，不然内部要存很多状态
    QTimer::singleShot(5'000, this, &MainWindow::addOrUpdateTrustedDevices);
}

void MainWindow::addOrUpdateTrustedDevice(int64_t device_id, bool gamepad, bool mouse,
                                          bool keyboard, int64_t last_access_time) {
    if (trusted_devices_table_ == nullptr) {
        LOG(ERR) << "Trusted devices table is null";
        return;
    }

    int row = -1;
    for (int i = 0; i < trusted_devices_table_->rowCount(); i++) {
        if (trusted_devices_table_->item(i, 0)->data(Qt::DisplayRole).toLongLong() == device_id) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        row = trusted_devices_table_->rowCount();
        trusted_devices_table_->setRowCount(row + 1);
    }

    // id
    QTableWidgetItem* id_item = new QTableWidgetItem;
    trusted_devices_table_->setItem(row, 0, id_item);
    id_item->setData(Qt::DisplayRole, QVariant::fromValue(device_id));
    // gamepad
    QCheckBox* gamepad_item = new QCheckBox();
    gamepad_item->setChecked(gamepad);
    connect(gamepad_item, &QCheckBox::stateChanged, [this, gamepad_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Gamepad,
                                         gamepad_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 1, makeWidgetHCentered(gamepad_item));
    // mouse
    QCheckBox* mouse_item = new QCheckBox();
    mouse_item->setChecked(mouse);
    connect(mouse_item, &QCheckBox::stateChanged, [this, mouse_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Mouse,
                                         mouse_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 2, makeWidgetHCentered(mouse_item));
    // keyboard
    QCheckBox* keyboard_item = new QCheckBox();
    keyboard_item->setChecked(keyboard);
    connect(keyboard_item, &QCheckBox::stateChanged, [this, keyboard_item, device_id](int) {
        params_.enable_device_permission(device_id, lt::GUI::DeviceType::Keyboard,
                                         keyboard_item->isChecked());
    });
    trusted_devices_table_->setCellWidget(row, 3, makeWidgetHCentered(keyboard_item));
    // time
    QTableWidgetItem* time_item =
        new QTableWidgetItem(QDateTime::fromSecsSinceEpoch(last_access_time)
                                 .toLocalTime()
                                 .toString("yyyy.MM.dd hh:mm:ss"));
    trusted_devices_table_->setItem(row, 4, time_item);
    // delete
    QPushButton* delete_item = new QPushButton(tr("delete"));
    connect(delete_item, &QPushButton::clicked, [this, device_id]() {
        for (int i = 0; i < trusted_devices_table_->rowCount(); i++) {
            if (trusted_devices_table_->item(i, 0)->data(Qt::DisplayRole).toLongLong() ==
                device_id) {
                trusted_devices_table_->removeRow(i);
                break;
            }
        }
        params_.delete_trusted_device(device_id);
    });
    trusted_devices_table_->setCellWidget(row, 5, delete_item);
}

QWidget* MainWindow::makeWidgetHCentered(QWidget* input_widget) {
    QWidget* output_widget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(output_widget);
    layout->addWidget(input_widget);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return output_widget;
}
