// LinkZoneRoot.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "LinkZoneRoot.h"

#include <Commctrl.h>
#include <winioctl.h>
#include <Ntddscsi.h>

#include <cstdlib>
#include <vector>
#include <string>

static const wchar_t* GetErrMsg(const wchar_t* prefix)
{
    static wchar_t buf[1024];
    DWORD err = GetLastError();
    _snwprintf_s(buf, _TRUNCATE, L"%s: 0x%08x: ", prefix, err);

    size_t len = wcslen(buf);
    size_t remain = sizeof(buf) / sizeof(buf[0]) - len;
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buf+len, remain, NULL);
    return buf;
}

static std::vector<std::wstring> GetRemovableDrives()
{
    std::vector<std::wstring> drives;
    DWORD driveMask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i)
    {
        if (driveMask & (1 << i))
        {
            const wchar_t driveStr[] = { (wchar_t)('A' + i), ':', '\0' };
            UINT ret = GetDriveTypeW(driveStr);
            if (ret == DRIVE_REMOVABLE ||  ret == DRIVE_CDROM)
            {
                drives.push_back(driveStr);
            }
        }
    }
    return drives;
}

void Execute(HWND hwnd, std::wstring buf)
{
    buf = L"\\\\.\\" + buf;
    HANDLE hFile = CreateFileW(
        buf.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (FAILED(hFile))
    {
        MessageBox(hwnd, GetErrMsg(L"Could not open drive"), NULL, MB_ICONEXCLAMATION | MB_OK);
        return;
    }
    
    SCSI_PASS_THROUGH_DIRECT data = { 0 };
    data.Length = sizeof(data);
    data.CdbLength = 12;
    data.DataIn = SCSI_IOCTL_DATA_IN;
    data.TimeOutValue = 0x64;
    data.Cdb[0] = 0x16;
    data.Cdb[1] = 0xf9;

    BOOL res = DeviceIoControl(hFile, IOCTL_SCSI_PASS_THROUGH_DIRECT, &data, sizeof(data), NULL, 0, NULL, NULL);
    if (!res)
    {
        MessageBox(hwnd, GetErrMsg(L"DeviceIoControl failed"), NULL, MB_ICONEXCLAMATION | MB_OK);
    }

    CloseHandle(hFile);
}

static INT_PTR CALLBACK MainDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        //Set dialogue icons
        SendMessage(hwnd, WM_SETICON, ICON_SMALL,
            (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_LINKZONEROOT)));
        SendMessage(hwnd, WM_SETICON, ICON_BIG,
            (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_LINKZONEROOT)));
        //Scan for drives
        SendMessage(hwnd, WM_COMMAND, MAKELONG(IDC_RESCAN, 0), 0);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_RESCAN:
        {
            HWND hwndDrive = GetDlgItem(hwnd, IDC_DRIVE);
            SendMessage(hwndDrive, CB_RESETCONTENT, 0, 0);
            for (const auto& drive : GetRemovableDrives())
            {
                SendMessage(hwndDrive, CB_ADDSTRING, 0, (LPARAM)drive.c_str());
            }
            SendMessage(hwndDrive, CB_SETCURSEL, 0, 0);
            break;
        }
        case IDC_EXECUTE:
        {
            HWND hwndDrive = GetDlgItem(hwnd, IDC_DRIVE);
            LRESULT cur = SendMessage(hwndDrive, CB_GETCURSEL, 0, 0);
            if (cur != CB_ERR)
            {
                LRESULT len = SendMessage(hwndDrive, CB_GETLBTEXTLEN, cur, 0);
                if (len != CB_ERR && len > 0)
                {
                    std::wstring buf(len, 0);
                    if (SendMessage(hwndDrive, CB_GETLBTEXT, cur, (LPARAM)buf.data()) != CB_ERR)
                    {
                        Execute(hwnd, std::move(buf));
                        // Rescan after executing
                        SendMessage(hwnd, WM_COMMAND, MAKELONG(IDC_RESCAN, 0), 0);
                    }
                }
            }
            break;
        }
        case IDOK: case IDCANCEL:
            EndDialog(hwnd, LOWORD(wParam));
            break;
        }
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icc;

    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_STANDARD_CLASSES;
    if (!InitCommonControlsEx(&icc))
    {
        MessageBox(nullptr, L"Could not initialise common controls.", NULL, MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    //Disable 'Select a drive' from showing if the drive's empty.
    SetErrorMode(SEM_FAILCRITICALERRORS);

    if (!DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDlgProc))
    {
        MessageBox(nullptr, L"Could not initialise main window.", NULL, MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    return 0;
}
