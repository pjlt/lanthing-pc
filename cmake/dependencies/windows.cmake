find_package(ViGEmClient REQUIRED PATHS third_party/prebuilt/vigemclient/${LT_THIRD_POSTFIX})
find_package(VPL REQUIRED PATHS third_party/prebuilt/onevpl/${LT_THIRD_POSTFIX})
add_subdirectory(third_party/wintoast)
add_subdirectory(third_party/prebuilt/openh264/win)
add_subdirectory(third_party/prebuilt/nbclipboard/${LT_THIRD_POSTFIX})
add_subdirectory(third_party/nvcodec)