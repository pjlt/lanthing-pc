#include <cstdint>
#include <string>
#include <iostream>

#include "client_ui.h"

int main()
{
    int64_t kMyDeviceID = 1234567;
    int64_t kPeerDeviceID = 7654321;
    lt::ui::ClientUI client_ui;
    if (!client_ui.start(kMyDeviceID, kPeerDeviceID)) {
        return -1;
    }
    client_ui.wait();
    return 0;
}