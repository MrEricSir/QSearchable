// MIT License
//
// Copyright (c) 2026 Eric Gregory <mrericsir@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Standalone installer/uninstaller for the QSearchable property handler.
// No Qt dependency — pure Win32. Designed to be invoked with admin privileges
// by an MSI, NSIS, or ShellExecuteW(..., "runas", ...) from the Qt app.
//
// Usage:
//   QSearchableInstaller.exe install  <ext> <clsid> <progid> <appname> <apppath> <dllpath> [iconpath]
//   QSearchableInstaller.exe uninstall <ext> <clsid> <progid> <appname>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Registry helpers
// ---------------------------------------------------------------------------

static bool regSetString(HKEY root, const wchar_t *subkey,
                         const wchar_t *valueName, const wchar_t *data)
{
    HKEY hkey = nullptr;
    LONG res = RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_SET_VALUE,
                               nullptr, &hkey, nullptr);
    if (res != ERROR_SUCCESS) {
        return false;
    }

    res = RegSetValueExW(hkey, valueName, 0, REG_SZ,
                         reinterpret_cast<const BYTE *>(data),
                         static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(hkey);
    return res == ERROR_SUCCESS;
}

static bool regDeleteTree(HKEY root, const wchar_t *subkey)
{
    LONG res = RegDeleteTreeW(root, subkey);
    return res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND;
}

// ---------------------------------------------------------------------------
// Install
// ---------------------------------------------------------------------------

static int doInstall(const std::wstring &ext, const std::wstring &clsid,
                     const std::wstring &progId, const std::wstring &appName,
                     const std::wstring &appPath, const std::wstring &dllPath,
                     const std::wstring &iconPath)
{
    bool ok = true;

    // 1. CLSID\{guid}\InprocServer32 — COM server registration.
    {
        std::wstring key = L"Software\\Classes\\CLSID\\" + clsid + L"\\InprocServer32";
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, dllPath.c_str());
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), L"ThreadingModel", L"Both");
    }

    // 2. PropertyHandlers\.ext — tells the indexer to use our handler.
    {
        std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
                           L"PropertySystem\\PropertyHandlers\\." + ext;
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, clsid.c_str());
    }

    // 3. File extension -> ProgId association.
    {
        std::wstring key = L"Software\\Classes\\." + ext;
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, progId.c_str());
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(),
                           L"Content Type", L"application/x-qsearchable");
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(),
                           L"PerceivedType", L"document");
    }

    // 4. PersistentHandler — "Null" handler delegates to the property handler.
    {
        std::wstring key = L"Software\\Classes\\." + ext + L"\\PersistentHandler";
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr,
                           L"{098f2470-bae0-11cd-b579-08002b30bfeb}");
    }

    // 5. ProgId — friendly name, NeverShowExt, shell open command, icon.
    {
        std::wstring key = L"Software\\Classes\\" + progId;
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, appName.c_str());
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(),
                           L"FriendlyTypeName", appName.c_str());
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), L"NeverShowExt", L"");
    }

    // 6. Shell open command.
    {
        std::wstring key = L"Software\\Classes\\" + progId + L"\\shell\\open\\command";
        std::wstring cmd = L"\"" + appPath + L"\" \"%1\"";
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, cmd.c_str());
    }

    // 7. DefaultIcon.
    {
        std::wstring key = L"Software\\Classes\\" + progId + L"\\DefaultIcon";
        std::wstring icon = iconPath.empty() ? (appPath + L",0") : iconPath;
        ok &= regSetString(HKEY_LOCAL_MACHINE, key.c_str(), nullptr, icon.c_str());
    }

    // Notify the Shell that file associations have changed.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (!ok) {
        fprintf(stderr, "QSearchableInstaller: one or more registry writes failed.\n");
        fprintf(stderr, "Make sure this process is running with admin privileges.\n");
        return 1;
    }

    printf("QSearchableInstaller: install succeeded.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Uninstall
// ---------------------------------------------------------------------------

static int doUninstall(const std::wstring &ext, const std::wstring &clsid,
                       const std::wstring &progId, const std::wstring &appName)
{
    (void)appName;

    // Remove in reverse order.
    regDeleteTree(HKEY_LOCAL_MACHINE,
                  (L"Software\\Classes\\" + progId).c_str());
    regDeleteTree(HKEY_LOCAL_MACHINE,
                  (L"Software\\Classes\\." + ext).c_str());
    regDeleteTree(HKEY_LOCAL_MACHINE,
                  (L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
                   L"PropertySystem\\PropertyHandlers\\." + ext).c_str());
    regDeleteTree(HKEY_LOCAL_MACHINE,
                  (L"Software\\Classes\\CLSID\\" + clsid).c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    printf("QSearchableInstaller: uninstall succeeded.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

static void printUsage()
{
    fprintf(stderr,
            "Usage:\n"
            "  QSearchableInstaller install  <ext> <clsid> <progid> <appname> <apppath> <dllpath> [iconpath]\n"
            "  QSearchableInstaller uninstall <ext> <clsid> <progid> <appname>\n");
}

int wmain(int argc, wchar_t *argv[])
{
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::wstring mode = argv[1];

    if (mode == L"install") {
        if (argc < 8) {
            printUsage();
            return 1;
        }

        std::wstring iconPath;
        if (argc >= 9) {
            iconPath = argv[8];
        }

        return doInstall(argv[2], argv[3], argv[4], argv[5],
                         argv[6], argv[7], iconPath);
    } else if (mode == L"uninstall") {
        if (argc < 6) {
            printUsage();
            return 1;
        }

        return doUninstall(argv[2], argv[3], argv[4], argv[5]);
    } else {
        fprintf(stderr, "Unknown mode: %ls\n", mode.c_str());
        printUsage();
        return 1;
    }
}
