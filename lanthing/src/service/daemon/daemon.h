#pragma once
#ifdef LT_WINDOWS
#include <service/daemon/daemon_win.h>
#else
#include <service/daemon/daemon_linux.h>
#endif
