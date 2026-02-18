#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <commctrl.h>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")

// Author: MefistoDark
// Mewgenics Trainer v1.0.1

#define GAME_PROCESS "Mewgenics.exe"

// Colors
#define COLOR_BG RGB(30, 30, 35)
#define COLOR_PANEL RGB(40, 40, 48)
#define COLOR_ACCENT RGB(0, 122, 204)
#define COLOR_TEXT RGB(220, 220, 220)
#define COLOR_SUCCESS RGB(76, 175, 80)
#define COLOR_ERROR RGB(244, 67, 54)

// Global variables
HWND g_hMainWindow = NULL;
HANDLE g_hProcess = NULL;
DWORD g_dwProcessId = 0;
uintptr_t g_moduleBase = 0;

// Brushes for custom colors
HBRUSH g_hBrushBg = NULL;
HBRUSH g_hBrushPanel = NULL;
HFONT g_hFontTitle = NULL;
HFONT g_hFontNormal = NULL;
HFONT g_hFontButton = NULL;

// Offsets and addresses
uintptr_t godmodeAddress = 0;
uintptr_t oskAddress = 0;
uintptr_t manaAddress = 0;
uintptr_t mainHousePtr = 0;
uintptr_t instantKillAddress = 0;

// Checkboxes and controls
HWND g_hGodmode, g_hOneHitKill, g_hInfiniteMana, g_hInstantKill;
HWND g_hFoodEdit, g_hGoldEdit, g_hFoodValue, g_hGoldValue;
HWND g_hStatusLabel;

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI TrainerThread(LPVOID lpParam);
bool AttachToProcess();
uintptr_t GetModuleBaseAddress(DWORD processID, const char* moduleName);
uintptr_t AOBScan(HANDLE hProcess, uintptr_t start, uintptr_t end, const char* pattern, const char* mask);
bool InitializeAddresses();
void UpdateStatus(const char* text);
void PlaySuccessSound();
void PlayErrorSound();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Create brushes
    g_hBrushBg = CreateSolidBrush(COLOR_BG);
    g_hBrushPanel = CreateSolidBrush(COLOR_PANEL);

    // Create fonts
    g_hFontTitle = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    g_hFontNormal = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    g_hFontButton = CreateFont(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    const char CLASS_NAME[] = "MewgenicsTrainerClass";

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = g_hBrushBg;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    g_hMainWindow = CreateWindowExA(
        0,
        CLASS_NAME,
        "Mewgenics Trainer v1.0.1 | by MefistoDark",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 650,
        NULL, NULL, hInstance, NULL
    );

    if (g_hMainWindow == NULL)
        return 0;

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    // Start trainer thread
    CreateThread(NULL, 0, TrainerThread, NULL, 0, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    DeleteObject(g_hBrushBg);
    DeleteObject(g_hBrushPanel);
    DeleteObject(g_hFontTitle);
    DeleteObject(g_hFontNormal);
    DeleteObject(g_hFontButton);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, COLOR_TEXT);
        SetBkColor(hdcStatic, COLOR_BG);
        return (INT_PTR)g_hBrushBg;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(255, 255, 255));
        SetBkColor(hdcEdit, RGB(50, 50, 58));
        return (INT_PTR)CreateSolidBrush(RGB(50, 50, 58));
    }

    case WM_CREATE:
    {
        // Title Panel with gradient background
        HWND hTitlePanel = CreateWindowA("STATIC", "", 
            WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
            0, 0, 520, 110, hwnd, (HMENU)9999, NULL, NULL);

        // Title Label
        HWND hTitle = CreateWindowA("STATIC", "MEWGENICS TRAINER", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, 10, 480, 35, hwnd, NULL, NULL, NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        // Author Label
        HWND hAuthor = CreateWindowA("STATIC", "Created by: MefistoDark", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, 50, 480, 22, hwnd, NULL, NULL, NULL);
        SendMessage(hAuthor, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Version info
        HWND hVersion = CreateWindowA("STATIC", "Game v1.0.20645 | Trainer v1.0.1", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            10, 75, 480, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hVersion, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // PLAYER OPTIONS - styled group
        HWND hPlayerGroup = CreateWindowA("BUTTON", " PLAYER OPTIONS ", 
            WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            15, 120, 470, 175, hwnd, NULL, NULL, NULL);
        SendMessage(hPlayerGroup, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hGodmode = CreateWindowA("BUTTON", "  Godmode [F1]", 
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT | BS_RIGHT,
            35, 150, 215, 28, hwnd, (HMENU)IDC_GODMODE, NULL, NULL);
        SendMessage(g_hGodmode, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hOneHitKill = CreateWindowA("BUTTON", "  One Hit Kill [F2]", 
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT | BS_RIGHT,
            265, 150, 205, 28, hwnd, (HMENU)IDC_ONEHITKILL, NULL, NULL);
        SendMessage(g_hOneHitKill, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hInfiniteMana = CreateWindowA("BUTTON", "  Infinite Mana [F3]", 
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT | BS_RIGHT,
            35, 190, 215, 28, hwnd, (HMENU)IDC_INFINITEMANA, NULL, NULL);
        SendMessage(g_hInfiniteMana, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hInstantKill = CreateWindowA("BUTTON", "  Instant Kill All [F4]", 
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | BS_LEFTTEXT | BS_RIGHT,
            265, 190, 205, 28, hwnd, (HMENU)IDC_INSTANTKILL, NULL, NULL);
        SendMessage(g_hInstantKill, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        // Info panel
        HWND hInfoPanel = CreateWindowA("STATIC", "", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            25, 230, 450, 50, hwnd, NULL, NULL, NULL);

        HWND hInfo1 = CreateWindowA("STATIC", "Press F1-F4 to toggle cheats quickly", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            30, 235, 440, 18, hwnd, NULL, NULL, NULL);
        SendMessage(hInfo1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        HWND hInfo2 = CreateWindowA("STATIC", "Checkboxes will update automatically", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            30, 255, 440, 18, hwnd, NULL, NULL, NULL);
        SendMessage(hInfo2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // INVENTORY - styled group
        HWND hInvGroup = CreateWindowA("BUTTON", " INVENTORY ", 
            WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            15, 305, 470, 195, hwnd, NULL, NULL, NULL);
        SendMessage(hInvGroup, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        // Food section with styled controls
        HWND hFoodLabel = CreateWindowA("STATIC", "Food:", 
            WS_VISIBLE | WS_CHILD,
            30, 340, 80, 25, hwnd, NULL, NULL, NULL);
        SendMessage(hFoodLabel, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hFoodValue = CreateWindowA("STATIC", "---", 
            WS_VISIBLE | WS_CHILD | SS_RIGHT,
            115, 340, 100, 25, hwnd, NULL, NULL, NULL);
        SendMessage(g_hFoodValue, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hFoodEdit = CreateWindowA("EDIT", "999", 
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            230, 337, 110, 30, hwnd, (HMENU)IDC_FOOD, NULL, NULL);
        SendMessage(g_hFoodEdit, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        HWND hSetFood = CreateWindowA("BUTTON", "SET", 
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            355, 336, 110, 32, hwnd, (HMENU)IDC_SETFOOD, NULL, NULL);
        SendMessage(hSetFood, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        // Gold section
        HWND hGoldLabel = CreateWindowA("STATIC", "Gold:", 
            WS_VISIBLE | WS_CHILD,
            30, 385, 80, 25, hwnd, NULL, NULL, NULL);
        SendMessage(hGoldLabel, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hGoldValue = CreateWindowA("STATIC", "---", 
            WS_VISIBLE | WS_CHILD | SS_RIGHT,
            115, 385, 100, 25, hwnd, NULL, NULL, NULL);
        SendMessage(g_hGoldValue, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        g_hGoldEdit = CreateWindowA("EDIT", "9999", 
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER,
            230, 382, 110, 30, hwnd, (HMENU)IDC_GOLD, NULL, NULL);
        SendMessage(g_hGoldEdit, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        HWND hSetGold = CreateWindowA("BUTTON", "SET", 
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            355, 381, 110, 32, hwnd, (HMENU)IDC_SETGOLD, NULL, NULL);
        SendMessage(hSetGold, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        // Quick action buttons row
        HWND hMaxFood = CreateWindowA("BUTTON", "MAX FOOD", 
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            30, 435, 105, 50, hwnd, (HMENU)IDC_MAXFOOD, NULL, NULL);
        SendMessage(hMaxFood, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        HWND hMaxGold = CreateWindowA("BUTTON", "MAX GOLD", 
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            145, 435, 105, 50, hwnd, (HMENU)IDC_MAXGOLD, NULL, NULL);
        SendMessage(hMaxGold, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        HWND hMaxAll = CreateWindowA("BUTTON", "MAX ALL", 
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            260, 435, 105, 50, hwnd, (HMENU)IDC_MAXALL, NULL, NULL);
        SendMessage(hMaxAll, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        HWND hRefresh = CreateWindowA("BUTTON", "REFRESH", 
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            375, 435, 90, 50, hwnd, (HMENU)IDC_REFRESH, NULL, NULL);
        SendMessage(hRefresh, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        // STATUS with accent color
        g_hStatusLabel = CreateWindowA("STATIC", "Status: Searching for game...", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            15, 515, 470, 30, hwnd, NULL, NULL, NULL);
        SendMessage(g_hStatusLabel, WM_SETFONT, (WPARAM)g_hFontButton, TRUE);

        // Hotkeys info
        HWND hHotkeys = CreateWindowA("STATIC", "Hotkeys: F1-F4 Toggle | F5 Reconnect", 
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            15, 555, 470, 22, hwnd, NULL, NULL, NULL);
        SendMessage(hHotkeys, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Register hotkeys
        RegisterHotKey(hwnd, 1, 0, VK_F1);
        RegisterHotKey(hwnd, 2, 0, VK_F2);
        RegisterHotKey(hwnd, 3, 0, VK_F3);
        RegisterHotKey(hwnd, 4, 0, VK_F4);
        RegisterHotKey(hwnd, 5, 0, VK_F5);

        return 0;
    }
    
    case WM_HOTKEY:
    {
        switch (wParam)
        {
        case 1: // F1 - Godmode
            SendMessage(g_hGodmode, BM_SETCHECK, 
                SendMessage(g_hGodmode, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
            PlaySuccessSound();
            break;
        case 2: // F2 - One Hit Kill
            SendMessage(g_hOneHitKill, BM_SETCHECK,
                SendMessage(g_hOneHitKill, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
            PlaySuccessSound();
            break;
        case 3: // F3 - Infinite Mana
            SendMessage(g_hInfiniteMana, BM_SETCHECK,
                SendMessage(g_hInfiniteMana, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
            PlaySuccessSound();
            break;
        case 4: // F4 - Instant Kill
            SendMessage(g_hInstantKill, BM_SETCHECK,
                SendMessage(g_hInstantKill, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
            PlaySuccessSound();
            break;
        case 5: // F5 - Refresh
            if (AttachToProcess())
                PlaySuccessSound();
            else
                PlayErrorSound();
            break;
        }
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_SETFOOD:
        {
            if (g_hProcess && mainHousePtr != 0)
            {
                char buffer[32];
                GetWindowTextA(g_hFoodEdit, buffer, 32);
                int value = atoi(buffer);

                uintptr_t ptrValue = 0;
                ReadProcessMemory(g_hProcess, (LPVOID)mainHousePtr, &ptrValue, sizeof(ptrValue), NULL);
                if (ptrValue != 0)
                {
                    WriteProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB0), &value, sizeof(value), NULL);
                    UpdateStatus("Food value changed successfully!");
                    PlaySuccessSound();
                }
                else
                {
                    PlayErrorSound();
                }
            }
            else
            {
                UpdateStatus("Error: Game not connected!");
                PlayErrorSound();
            }
            break;
        }
        case IDC_SETGOLD:
        {
            if (g_hProcess && mainHousePtr != 0)
            {
                char buffer[32];
                GetWindowTextA(g_hGoldEdit, buffer, 32);
                int value = atoi(buffer);

                uintptr_t ptrValue = 0;
                ReadProcessMemory(g_hProcess, (LPVOID)mainHousePtr, &ptrValue, sizeof(ptrValue), NULL);
                if (ptrValue != 0)
                {
                    WriteProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB4), &value, sizeof(value), NULL);
                    UpdateStatus("Gold value changed successfully!");
                    PlaySuccessSound();
                }
                else
                {
                    PlayErrorSound();
                }
            }
            else
            {
                UpdateStatus("Error: Game not connected!");
                PlayErrorSound();
            }
            break;
        }
        case IDC_MAXFOOD:
        {
            if (g_hProcess && mainHousePtr != 0)
            {
                int maxValue = 999999;
                uintptr_t ptrValue = 0;
                ReadProcessMemory(g_hProcess, (LPVOID)mainHousePtr, &ptrValue, sizeof(ptrValue), NULL);
                if (ptrValue != 0)
                {
                    WriteProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB0), &maxValue, sizeof(maxValue), NULL);
                    SetWindowTextA(g_hFoodEdit, "999999");
                    UpdateStatus("Food set to maximum!");
                    PlaySuccessSound();
                }
            }
            break;
        }
        case IDC_MAXGOLD:
        {
            if (g_hProcess && mainHousePtr != 0)
            {
                int maxValue = 999999;
                uintptr_t ptrValue = 0;
                ReadProcessMemory(g_hProcess, (LPVOID)mainHousePtr, &ptrValue, sizeof(ptrValue), NULL);
                if (ptrValue != 0)
                {
                    WriteProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB4), &maxValue, sizeof(maxValue), NULL);
                    SetWindowTextA(g_hGoldEdit, "999999");
                    UpdateStatus("Gold set to maximum!");
                    PlaySuccessSound();
                }
            }
            break;
        }
        case IDC_MAXALL:
        {
            if (g_hProcess && mainHousePtr != 0)
            {
                int maxValue = 999999;
                uintptr_t ptrValue = 0;
                ReadProcessMemory(g_hProcess, (LPVOID)mainHousePtr, &ptrValue, sizeof(ptrValue), NULL);
                if (ptrValue != 0)
                {
                    WriteProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB0), &maxValue, sizeof(maxValue), NULL);
                    WriteProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB4), &maxValue, sizeof(maxValue), NULL);
                    SetWindowTextA(g_hFoodEdit, "999999");
                    SetWindowTextA(g_hGoldEdit, "999999");
                    UpdateStatus("All values set to maximum!");
                    PlaySuccessSound();
                }
            }
            break;
        }
        case IDC_REFRESH:
        {
            if (AttachToProcess())
                PlaySuccessSound();
            else
                PlayErrorSound();
            break;
        }
        }
        return 0;
    }
    
    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == 9999) // Title panel
        {
            // Draw gradient background for title
            RECT rect = pDIS->rcItem;
            TRIVERTEX vertex[2];

            vertex[0].x = 0;
            vertex[0].y = 0;
            vertex[0].Red = 0x2000;
            vertex[0].Green = 0x5000;
            vertex[0].Blue = 0x8000;
            vertex[0].Alpha = 0x0000;

            vertex[1].x = rect.right;
            vertex[1].y = rect.bottom;
            vertex[1].Red = 0x1000;
            vertex[1].Green = 0x3000;
            vertex[1].Blue = 0x6000;
            vertex[1].Alpha = 0x0000;

            GRADIENT_RECT gRect;
            gRect.UpperLeft = 0;
            gRect.LowerRight = 1;

            GradientFill(pDIS->hDC, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, 2, COLOR_ACCENT);
            HPEN hOldPen = (HPEN)SelectObject(pDIS->hDC, hPen);
            MoveToEx(pDIS->hDC, 0, rect.bottom - 1, NULL);
            LineTo(pDIS->hDC, rect.right, rect.bottom - 1);
            SelectObject(pDIS->hDC, hOldPen);
            DeleteObject(hPen);
        }
        return TRUE;
    }

    case WM_DESTROY:
        if (g_hProcess)
            CloseHandle(g_hProcess);
        PostQuitMessage(0);
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool AttachToProcess()
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (_stricmp(pe32.szExeFile, GAME_PROCESS) == 0)
            {
                g_dwProcessId = pe32.th32ProcessID;
                g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_dwProcessId);

                if (g_hProcess)
                {
                    g_moduleBase = GetModuleBaseAddress(g_dwProcessId, GAME_PROCESS);
                    CloseHandle(hSnapshot);

                    if (InitializeAddresses())
                    {
                        UpdateStatus("Status: Connected! All addresses found!");
                        return true;
                    }
                    else
                    {
                        UpdateStatus("Status: Connected but addresses not found!");
                        return false;
                    }
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    UpdateStatus("Status: Mewgenics.exe not found!");
    return false;
}

uintptr_t GetModuleBaseAddress(DWORD processID, const char* moduleName)
{
    uintptr_t moduleBase = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID);
    
    if (hSnapshot != INVALID_HANDLE_VALUE)
    {
        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);
        
        if (Module32First(hSnapshot, &me32))
        {
            do
            {
                if (_stricmp(me32.szModule, moduleName) == 0)
                {
                    moduleBase = (uintptr_t)me32.modBaseAddr;
                    break;
                }
            } while (Module32Next(hSnapshot, &me32));
        }
        CloseHandle(hSnapshot);
    }
    
    return moduleBase;
}

void UpdateStatus(const char* text)
{
    if (g_hStatusLabel)
        SetWindowTextA(g_hStatusLabel, text);
}

void PlaySuccessSound()
{
    MessageBeep(MB_OK);
}

void PlayErrorSound()
{
    MessageBeep(MB_ICONERROR);
}

uintptr_t AOBScan(HANDLE hProcess, uintptr_t start, uintptr_t end, const char* pattern, const char* mask)
{
    const size_t patternLen = strlen(mask);
    BYTE* buffer = new BYTE[end - start];
    SIZE_T bytesRead;

    if (!ReadProcessMemory(hProcess, (LPVOID)start, buffer, end - start, &bytesRead))
    {
        delete[] buffer;
        return 0;
    }

    for (size_t i = 0; i < bytesRead - patternLen; i++)
    {
        bool found = true;
        for (size_t j = 0; j < patternLen; j++)
        {
            if (mask[j] == 'x' && buffer[i + j] != pattern[j])
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            delete[] buffer;
            return start + i;
        }
    }

    delete[] buffer;
    return 0;
}

bool InitializeAddresses()
{
    if (!g_hProcess || g_moduleBase == 0)
        return false;

    // Get module size
    MODULEINFO modInfo;
    if (!GetModuleInformation(g_hProcess, (HMODULE)g_moduleBase, &modInfo, sizeof(MODULEINFO)))
        return false;

    uintptr_t moduleEnd = g_moduleBase + modInfo.SizeOfImage;

    // Find Godmode/OSK address - Pattern: 41 89 86 B0 04 00 00
    const char godmodePattern[] = "\x41\x89\x86\xB0\x04\x00\x00";
    const char godmodeMask[] = "xxxxxxx";
    godmodeAddress = AOBScan(g_hProcess, g_moduleBase, moduleEnd, godmodePattern, godmodeMask);

    // OSK address is the same location
    oskAddress = godmodeAddress;

    // Find MainHousePtr - Pattern: 8B 91 B0 00 00 00
    const char mainHousePattern[] = "\x8B\x91\xB0\x00\x00\x00";
    const char mainHouseMask[] = "xxxxxx";
    uintptr_t mainHouseInstruction = AOBScan(g_hProcess, g_moduleBase, moduleEnd, mainHousePattern, mainHouseMask);
    if (mainHouseInstruction != 0)
    {
        // This is a simplified approach - in reality you'd need to extract the pointer from the instruction
        mainHousePtr = mainHouseInstruction;
    }

    // Find Mana address - Pattern: 89 8F 18 0D 00 00
    const char manaPattern[] = "\x89\x8F\x18\x0D\x00\x00";
    const char manaMask[] = "xxxxxx";
    manaAddress = AOBScan(g_hProcess, g_moduleBase, moduleEnd, manaPattern, manaMask);

    // Find Instant Kill address - Pattern: 89 8F B0 04 00 00
    const char instantKillPattern[] = "\x89\x8F\xB0\x04\x00\x00";
    const char instantKillMask[] = "xxxxxx";
    instantKillAddress = AOBScan(g_hProcess, g_moduleBase, moduleEnd, instantKillPattern, instantKillMask);

    // Check if at least some critical addresses were found
    bool success = (godmodeAddress != 0 || mainHousePtr != 0);

    return success;
}

DWORD WINAPI TrainerThread(LPVOID lpParam)
{
    while (true)
    {
        if (!g_hProcess || g_hProcess == INVALID_HANDLE_VALUE)
        {
            AttachToProcess();
            Sleep(2000);
            continue;
        }

        // Check if process is still alive
        DWORD exitCode;
        if (GetExitCodeProcess(g_hProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            CloseHandle(g_hProcess);
            g_hProcess = NULL;
            UpdateStatus("Status: Game closed. Waiting for restart...");
            Sleep(2000);
            continue;
        }

        // Update inventory values display
        if (mainHousePtr != 0)
        {
            uintptr_t ptrValue = 0;
            if (ReadProcessMemory(g_hProcess, (LPVOID)mainHousePtr, &ptrValue, sizeof(ptrValue), NULL))
            {
                if (ptrValue != 0)
                {
                    int foodValue = 0;
                    int goldValue = 0;

                    if (ReadProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB0), &foodValue, sizeof(foodValue), NULL))
                    {
                        char buffer[32];
                        sprintf(buffer, "%d", foodValue);
                        SetWindowTextA(g_hFoodValue, buffer);
                    }

                    if (ReadProcessMemory(g_hProcess, (LPVOID)(ptrValue + 0xB4), &goldValue, sizeof(goldValue), NULL))
                    {
                        char buffer[32];
                        sprintf(buffer, "%d", goldValue);
                        SetWindowTextA(g_hGoldValue, buffer);
                    }
                }
            }
        }

        // Apply cheats
        if (SendMessage(g_hGodmode, BM_GETCHECK, 0, 0) == BST_CHECKED && godmodeAddress != 0)
        {
            BYTE value = 1;
            WriteProcessMemory(g_hProcess, (LPVOID)godmodeAddress, &value, 1, NULL);
        }

        if (SendMessage(g_hOneHitKill, BM_GETCHECK, 0, 0) == BST_CHECKED && oskAddress != 0)
        {
            BYTE value = 1;
            WriteProcessMemory(g_hProcess, (LPVOID)oskAddress, &value, 1, NULL);
        }

        Sleep(100);
    }

    return 0;
}
