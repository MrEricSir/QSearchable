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
// All identifiers (file extension, CLSID, ProgId) are derived deterministically
// from the application executable path. The installer only needs the exe path:
//
// Usage:
//   QSearchableWindowsInstaller.exe install   <exepath>
//   QSearchableWindowsInstaller.exe uninstall <exepath>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#include <searchapi.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

static std::string wideToUtf8(const std::wstring &wide)
{
    if (wide.empty()) {
        return {};
    }

    int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                     static_cast<int>(wide.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }

    std::string result(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                        static_cast<int>(wide.size()),
                        &result[0], needed, nullptr, nullptr);
    return result;
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

static std::wstring extractAppName(const std::wstring &exePath)
{
    // Find filename after last separator.
    size_t sep = exePath.find_last_of(L"\\/");
    std::wstring filename = (sep != std::wstring::npos)
                                ? exePath.substr(sep + 1)
                                : exePath;

    // Strip the last extension.
    size_t dot = filename.rfind(L'.');
    if (dot != std::wstring::npos) {
        filename = filename.substr(0, dot);
    }

    return filename;
}

static std::wstring extractDirPath(const std::wstring &exePath)
{
    size_t sep = exePath.find_last_of(L"\\/");
    if (sep != std::wstring::npos) {
        return exePath.substr(0, sep);
    }
    return L".";
}

// ---------------------------------------------------------------------------
// SHA-256 via BCrypt
// ---------------------------------------------------------------------------

static std::string computeSha256Hex(const std::wstring &input)
{
    std::string utf8 = wideToUtf8(input);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    BYTE hashBuf[32] = {};

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) != 0) {
        return {};
    }

    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }

    BCryptHashData(hHash, reinterpret_cast<PUCHAR>(utf8.data()),
                   static_cast<ULONG>(utf8.size()), 0);

    BCryptFinishHash(hHash, hashBuf, sizeof(hashBuf), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    // Convert to lowercase hex string.
    static const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (int i = 0; i < 32; ++i) {
        result.push_back(hex[hashBuf[i] >> 4]);
        result.push_back(hex[hashBuf[i] & 0x0F]);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Identifier derivation — must match WindowsSearchBackend logic exactly
// ---------------------------------------------------------------------------

static std::wstring deriveFileExtension(const std::string &hexHash)
{
    // "qs" + first 6 hex chars of SHA-256
    std::wstring ext = L"qs";
    for (int i = 0; i < 6 && i < static_cast<int>(hexHash.size()); ++i) {
        ext.push_back(static_cast<wchar_t>(hexHash[i]));
    }
    return ext;
}

static std::wstring deriveClsid(const std::string &hexHash)
{
    // Format as {8-4-4-4-12} from the hex hash
    if (hexHash.size() < 32) {
        return {};
    }

    wchar_t buf[64];
    swprintf_s(buf, L"{%.8S-%.4S-%.4S-%.4S-%.12S}",
               hexHash.c_str(),
               hexHash.c_str() + 8,
               hexHash.c_str() + 12,
               hexHash.c_str() + 16,
               hexHash.c_str() + 20);
    return buf;
}

static std::wstring deriveProgId(const std::wstring &appName)
{
    return L"QSearchable." + appName;
}

// ---------------------------------------------------------------------------
// Icon extraction — saves the exe's embedded icon as an .ico file
// ---------------------------------------------------------------------------

static std::wstring getLocalAppDataPath()
{
    wchar_t *path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        std::wstring result = path;
        CoTaskMemFree(path);
        return result;
    }
    return {};
}

static std::wstring extractAndSaveIcon(const std::wstring &exePath,
                                        const std::wstring &appName)
{
    std::wstring localAppData = getLocalAppDataPath();
    if (localAppData.empty()) {
        return {};
    }

    std::wstring iconDir = localAppData + L"\\QSearchable\\" + appName;
    SHCreateDirectoryExW(nullptr, iconDir.c_str(), nullptr);

    std::wstring iconPath = iconDir + L"\\app.ico";

    // Extract the large (32x32) icon from the exe.
    HICON hIcon = nullptr;
    UINT found = ExtractIconExW(exePath.c_str(), 0, &hIcon, nullptr, 1);
    if (found == 0 || !hIcon) {
        return {};
    }

    // Get icon bitmap info.
    ICONINFO iconInfo = {};
    if (!GetIconInfo(hIcon, &iconInfo)) {
        DestroyIcon(hIcon);
        return {};
    }

    if (!iconInfo.hbmColor) {
        // Monochrome icon — not supported for .ico extraction.
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
        DestroyIcon(hIcon);
        return {};
    }

    BITMAP bmp = {};
    GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp);

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    int bpp = 32;

    // GetDIBits requires a BITMAPINFO which includes space for a color table
    // after the header. For 32-bit BI_RGB there's no color table, but for the
    // 1-bit mask bitmap GetDIBits writes 2 RGBQUAD entries. Use a struct with
    // enough room for both cases to avoid stack corruption.
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } bmi = {};

    bmi.header.biSize = sizeof(BITMAPINFOHEADER);
    bmi.header.biWidth = width;
    bmi.header.biHeight = height;  // bottom-up
    bmi.header.biPlanes = 1;
    bmi.header.biBitCount = static_cast<WORD>(bpp);
    bmi.header.biCompression = BI_RGB;

    int stride = width * 4;
    std::vector<BYTE> colorBits(stride * height);

    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, iconInfo.hbmColor, 0, height,
              colorBits.data(), reinterpret_cast<BITMAPINFO *>(&bmi), DIB_RGB_COLORS);

    // Get the mask bitmap (1-bit per pixel, bottom-up).
    int maskStride = ((width + 31) / 32) * 4;
    std::vector<BYTE> maskBits(maskStride * height);

    memset(&bmi, 0, sizeof(bmi));
    bmi.header.biSize = sizeof(BITMAPINFOHEADER);
    bmi.header.biWidth = width;
    bmi.header.biHeight = height;
    bmi.header.biPlanes = 1;
    bmi.header.biBitCount = 1;
    bmi.header.biCompression = BI_RGB;

    GetDIBits(hdc, iconInfo.hbmMask, 0, height,
              maskBits.data(), reinterpret_cast<BITMAPINFO *>(&bmi), DIB_RGB_COLORS);

    ReleaseDC(nullptr, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);

    // Build the .ico file in memory.
    DWORD colorSize = static_cast<DWORD>(colorBits.size());
    DWORD maskSize = static_cast<DWORD>(maskBits.size());
    DWORD bmpHeaderSize = sizeof(BITMAPINFOHEADER);
    DWORD imageSize = bmpHeaderSize + colorSize + maskSize;

    // ICO header: 6 bytes
    // Directory entry: 16 bytes
    // Image data: BITMAPINFOHEADER + color bits + mask bits
    std::vector<BYTE> ico;
    ico.reserve(6 + 16 + imageSize);

    // ICO header
    auto writeWord = [&](WORD v) {
        ico.push_back(static_cast<BYTE>(v & 0xFF));
        ico.push_back(static_cast<BYTE>((v >> 8) & 0xFF));
    };
    auto writeDword = [&](DWORD v) {
        ico.push_back(static_cast<BYTE>(v & 0xFF));
        ico.push_back(static_cast<BYTE>((v >> 8) & 0xFF));
        ico.push_back(static_cast<BYTE>((v >> 16) & 0xFF));
        ico.push_back(static_cast<BYTE>((v >> 24) & 0xFF));
    };

    writeWord(0);       // reserved
    writeWord(1);       // type: icon
    writeWord(1);       // count: 1 image

    // Directory entry
    ico.push_back(static_cast<BYTE>(width < 256 ? width : 0));   // width
    ico.push_back(static_cast<BYTE>(height < 256 ? height : 0)); // height
    ico.push_back(0);   // color palette
    ico.push_back(0);   // reserved
    writeWord(1);       // color planes
    writeWord(static_cast<WORD>(bpp));  // bits per pixel
    writeDword(imageSize);              // image data size
    writeDword(6 + 16);                 // offset to image data

    // BITMAPINFOHEADER — height is doubled in ICO format (color + mask)
    BITMAPINFOHEADER icoHeader = {};
    icoHeader.biSize = sizeof(BITMAPINFOHEADER);
    icoHeader.biWidth = width;
    icoHeader.biHeight = height * 2;
    icoHeader.biPlanes = 1;
    icoHeader.biBitCount = static_cast<WORD>(bpp);
    icoHeader.biCompression = BI_RGB;
    icoHeader.biSizeImage = colorSize + maskSize;

    const BYTE *hdrBytes = reinterpret_cast<const BYTE *>(&icoHeader);
    ico.insert(ico.end(), hdrBytes, hdrBytes + sizeof(icoHeader));

    // Color bits
    ico.insert(ico.end(), colorBits.begin(), colorBits.end());

    // Mask bits
    ico.insert(ico.end(), maskBits.begin(), maskBits.end());

    // Write to file.
    HANDLE hFile = CreateFileW(iconPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return {};
    }

    DWORD written = 0;
    WriteFile(hFile, ico.data(), static_cast<DWORD>(ico.size()), &written, nullptr);
    CloseHandle(hFile);

    return iconPath;
}

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

static LONG regDeleteTree(HKEY root, const wchar_t *subkey)
{
    return RegDeleteTreeW(root, subkey);
}

// ---------------------------------------------------------------------------
// Data directory
// ---------------------------------------------------------------------------

static std::wstring getBaseDir(const std::wstring &appName)
{
    std::wstring localAppData = getLocalAppDataPath();
    if (localAppData.empty()) {
        return {};
    }
    return localAppData + L"\\QSearchable\\" + appName;
}

// ---------------------------------------------------------------------------
// Crawl scope — registers/unregisters the data directory with Windows Search
// ---------------------------------------------------------------------------

static bool registerCrawlScope(const std::wstring &baseDir)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    ISearchManager *searchManager = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(CSearchManager), nullptr, CLSCTX_ALL,
                                 IID_PPV_ARGS(&searchManager));
    if (FAILED(hr)) {
        fprintf(stderr, "QSearchableWindowsInstaller: failed to create ISearchManager (hr=0x%lx)\n",
                static_cast<unsigned long>(hr));
        CoUninitialize();
        return false;
    }

    ISearchCatalogManager *catalogManager = nullptr;
    hr = searchManager->GetCatalog(L"SystemIndex", &catalogManager);
    searchManager->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "QSearchableWindowsInstaller: failed to get SystemIndex catalog (hr=0x%lx)\n",
                static_cast<unsigned long>(hr));
        CoUninitialize();
        return false;
    }

    ISearchCrawlScopeManager *scopeManager = nullptr;
    hr = catalogManager->GetCrawlScopeManager(&scopeManager);
    if (FAILED(hr)) {
        catalogManager->Release();
        fprintf(stderr, "QSearchableWindowsInstaller: failed to get crawl scope manager (hr=0x%lx)\n",
                static_cast<unsigned long>(hr));
        CoUninitialize();
        return false;
    }

    std::wstring url = L"file:///" + baseDir;
    hr = scopeManager->AddUserScopeRule(url.c_str(), TRUE, TRUE, FALSE);
    if (FAILED(hr)) {
        fprintf(stderr, "QSearchableWindowsInstaller: AddUserScopeRule failed (hr=0x%lx)\n",
                static_cast<unsigned long>(hr));
    }
    scopeManager->SaveAll();
    scopeManager->Release();

    // Ask the indexer to crawl the directory immediately.
    catalogManager->ReindexSearchRoot(url.c_str());
    catalogManager->Release();

    CoUninitialize();
    return true;
}

static void unregisterCrawlScope(const std::wstring &baseDir)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    ISearchManager *searchManager = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(CSearchManager), nullptr, CLSCTX_ALL,
                                 IID_PPV_ARGS(&searchManager));
    if (FAILED(hr)) {
        CoUninitialize();
        return;
    }

    ISearchCatalogManager *catalogManager = nullptr;
    hr = searchManager->GetCatalog(L"SystemIndex", &catalogManager);
    searchManager->Release();
    if (FAILED(hr)) {
        CoUninitialize();
        return;
    }

    ISearchCrawlScopeManager *scopeManager = nullptr;
    hr = catalogManager->GetCrawlScopeManager(&scopeManager);
    catalogManager->Release();
    if (FAILED(hr)) {
        CoUninitialize();
        return;
    }

    std::wstring url = L"file:///" + baseDir;
    scopeManager->RemoveScopeRule(url.c_str());
    scopeManager->SaveAll();
    scopeManager->Release();

    CoUninitialize();
}

// ---------------------------------------------------------------------------
// App Paths — registers the exe so Windows can find it and its DLLs
// ---------------------------------------------------------------------------

static void registerAppPaths(const std::wstring &exePath, const std::wstring &appDir)
{
    // Extract just the filename from the full exe path.
    size_t sep = exePath.find_last_of(L"\\/");
    std::wstring exeFileName = (sep != std::wstring::npos)
                                   ? exePath.substr(sep + 1)
                                   : exePath;

    std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeFileName;

    HKEY hkey = nullptr;
    LONG res = RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0,
                                KEY_SET_VALUE, nullptr, &hkey, nullptr);
    if (res != ERROR_SUCCESS) {
        return;
    }

    // Default value: full path to the exe.
    RegSetValueExW(hkey, nullptr, 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(exePath.c_str()),
                   static_cast<DWORD>((exePath.size() + 1) * sizeof(wchar_t)));

    // Path value: directories to search for DLLs.
    RegSetValueExW(hkey, L"Path", 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(appDir.c_str()),
                   static_cast<DWORD>((appDir.size() + 1) * sizeof(wchar_t)));

    RegCloseKey(hkey);
}

static void unregisterAppPaths(const std::wstring &exePath)
{
    size_t sep = exePath.find_last_of(L"\\/");
    std::wstring exeFileName = (sep != std::wstring::npos)
                                   ? exePath.substr(sep + 1)
                                   : exePath;

    std::wstring key = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeFileName;
    regDeleteTree(HKEY_CURRENT_USER, key.c_str());
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
        fprintf(stderr, "QSearchableWindowsInstaller: one or more registry writes failed.\n");
        fprintf(stderr, "Make sure this process is running with admin privileges.\n");
        return 1;
    }

    printf("QSearchableWindowsInstaller: install succeeded.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Uninstall
// ---------------------------------------------------------------------------

static int doUninstall(const std::wstring &ext, const std::wstring &clsid,
                       const std::wstring &progId)
{
    // Remove in reverse order. Continue on failure so we clean up as much
    // as possible, but track the first error for the exit code.
    int firstErr = 0;
    auto tryDelete = [&firstErr](const wchar_t *subkey) {
        LONG res = regDeleteTree(HKEY_LOCAL_MACHINE, subkey);
        if (res != ERROR_SUCCESS && res != ERROR_FILE_NOT_FOUND && firstErr == 0) {
            firstErr = static_cast<int>(res);
        }
    };

    tryDelete((L"Software\\Classes\\" + progId).c_str());
    tryDelete((L"Software\\Classes\\." + ext).c_str());
    tryDelete((L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
               L"PropertySystem\\PropertyHandlers\\." + ext).c_str());
    tryDelete((L"Software\\Classes\\CLSID\\" + clsid).c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    if (firstErr != 0) {
        fprintf(stderr, "QSearchableWindowsInstaller: uninstall failed (Windows error %d).\n",
                firstErr);
        return firstErr;
    }

    printf("QSearchableWindowsInstaller: uninstall succeeded.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

static void printUsage()
{
    fprintf(stderr,
            "Usage:\n"
            "  QSearchableWindowsInstaller install   <exepath>\n"
            "  QSearchableWindowsInstaller uninstall <exepath>\n");
}

int wmain(int argc, wchar_t *argv[])
{
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::wstring mode = argv[1];
    std::wstring exePath = argv[2];

    // Derive all identifiers from the exe path.
    std::wstring appName = extractAppName(exePath);
    if (appName.empty()) {
        fprintf(stderr, "QSearchableWindowsInstaller: could not determine app name from path.\n");
        return 1;
    }

    std::string hexHash = computeSha256Hex(appName);
    if (hexHash.size() < 32) {
        fprintf(stderr, "QSearchableWindowsInstaller: SHA-256 computation failed.\n");
        return 1;
    }

    std::wstring ext = deriveFileExtension(hexHash);
    std::wstring clsid = deriveClsid(hexHash);
    std::wstring progId = deriveProgId(appName);

    if (mode == L"install") {
        std::wstring appDir = extractDirPath(exePath);
        std::wstring dllPath = appDir + L"\\QSearchablePropertyHandler.dll";
        std::wstring baseDir = getBaseDir(appName);

        // Create the data directory so the crawl scope has something to point to.
        if (!baseDir.empty()) {
            SHCreateDirectoryExW(nullptr, baseDir.c_str(), nullptr);
        }

        // Extract the app icon from the exe and save as .ico.
        std::wstring iconPath = extractAndSaveIcon(exePath, appName);

        int result = doInstall(ext, clsid, progId, appName, exePath, dllPath, iconPath);
        if (result != 0) {
            return result;
        }

        // Register the data directory as a crawl scope so Windows Search
        // indexes the item files, and register App Paths so Windows can
        // find the exe and its DLLs when opening search results.
        registerCrawlScope(baseDir);
        registerAppPaths(exePath, appDir);

        return 0;
    } else if (mode == L"uninstall") {
        std::wstring baseDir = getBaseDir(appName);

        // Unregister crawl scope and App Paths before removing registry keys.
        unregisterCrawlScope(baseDir);
        unregisterAppPaths(exePath);

        int result = doUninstall(ext, clsid, progId);

        // Remove the data directory and all indexed item files.
        if (!baseDir.empty()) {
            // Use SHFileOperation for recursive delete (no Qt available).
            std::wstring doubleNull = baseDir + L'\0';
            SHFILEOPSTRUCTW op = {};
            op.wFunc = FO_DELETE;
            op.pFrom = doubleNull.c_str();
            op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
            SHFileOperationW(&op);
        }

        return result;
    } else {
        fprintf(stderr, "Unknown mode: %ls\n", mode.c_str());
        printUsage();
        return 1;
    }
}
