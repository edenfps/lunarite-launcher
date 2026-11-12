// resource.h - identifiers for WZLauncher.rc (dialog 102, icon 135, controls 1000-1013).
#pragma once

#define IDD_WZLAUNCHER_DIALOG       102
#define IDI_WZLAUNCHER              135

#define IDC_BTN_UPDATE              1000    // 0x3E8  "Update Game"
#define IDC_BTN_MINIMIZE            1001    // 0x3E9  "Min"
#define IDC_BTN_EXIT                1002    // 0x3EA  "Exit"
#define IDC_EXPLORER                1003    // 0x3EB  WebBrowser ActiveX (see WZLauncher.rc)
#define IDC_STATIC_TITLE            1004    // 0x3EC  "Caption"
#define IDC_STATIC_TOTAL            1005    // 0x3ED  "Update State"
#define IDC_STATIC_FILE             1006    // 0x3EE  "File State"
#define IDC_PROGRESS_TOTAL          1007    // 0x3EF
#define IDC_PROGRESS_FILE           1008    // 0x3F0
#define IDC_BTN_STOP                1009    // 0x3F1  "Stop Update"
#define IDC_BTN_EXECUTE             1010    // 0x3F2  "Execute Game"
#define IDC_BTN_CONFIG              1011    // 0x3F3  "Game Config"
#define IDC_BTN_INSTALL             1012    // 0x3F4  "Install Game"
#define IDC_CHK_AUTOEXEC            1013    // 0x3F5  "Auto Start Game"
#define IDC_CHK_DXVK                1014    // Lunarite: "Vulkan (DXVK)" toggle

#ifndef IDC_STATIC
#define IDC_STATIC                  -1
#endif
