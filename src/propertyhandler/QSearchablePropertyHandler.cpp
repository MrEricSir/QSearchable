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

#include "QSearchablePropertyHandler.h"

#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>

#include <vector>

// ---------------------------------------------------------------------------
// QSearchablePropertyHandler
// ---------------------------------------------------------------------------

QSearchablePropertyHandler::QSearchablePropertyHandler()
    : m_refCount(1)
    , m_initialized(false)
{
    DllAddRef();
}

QSearchablePropertyHandler::~QSearchablePropertyHandler()
{
    DllRelease();
}

IFACEMETHODIMP QSearchablePropertyHandler::QueryInterface(REFIID riid, void **ppv)
{
    if (!ppv) {
        return E_POINTER;
    }

    *ppv = nullptr;

    if (riid == IID_IUnknown) {
        *ppv = static_cast<IInitializeWithStream *>(this);
    } else if (riid == IID_IInitializeWithStream) {
        *ppv = static_cast<IInitializeWithStream *>(this);
    } else if (riid == IID_IPropertyStore) {
        *ppv = static_cast<IPropertyStore *>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) QSearchablePropertyHandler::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) QSearchablePropertyHandler::Release()
{
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }

    return ref;
}

// ---------------------------------------------------------------------------
// INI parsing (no Qt dependency)
// ---------------------------------------------------------------------------

static std::wstring utf8ToWide(const char *data, int len)
{
    if (len <= 0) {
        return {};
    }

    int needed = MultiByteToWideChar(CP_UTF8, 0, data, len, nullptr, 0);
    if (needed <= 0) {
        return {};
    }

    std::wstring result(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, data, len, &result[0], needed);
    return result;
}

static std::wstring trim(const std::wstring &s)
{
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) {
        return {};
    }

    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

void QSearchablePropertyHandler::parseIniFromStream(IStream *pstream)
{
    // Read entire stream into memory (these files are tiny).
    STATSTG stat = {};
    if (FAILED(pstream->Stat(&stat, STATFLAG_NONAME))) {
        return;
    }

    ULONG size = static_cast<ULONG>(stat.cbSize.QuadPart);
    if (size == 0 || size > 65536) {
        return;
    }

    std::vector<char> buf(size);
    ULONG bytesRead = 0;

    LARGE_INTEGER zero = {};
    pstream->Seek(zero, STREAM_SEEK_SET, nullptr);

    if (FAILED(pstream->Read(buf.data(), size, &bytesRead))) {
        return;
    }

    std::wstring content = utf8ToWide(buf.data(), static_cast<int>(bytesRead));

    // Parse INI: find [General] section and extract key=value pairs.
    bool inGeneral = false;
    size_t pos = 0;

    while (pos < content.size()) {
        size_t lineEnd = content.find(L'\n', pos);
        if (lineEnd == std::wstring::npos) {
            lineEnd = content.size();
        }

        std::wstring line = trim(content.substr(pos, lineEnd - pos));
        pos = lineEnd + 1;

        if (line.empty()) {
            continue;
        }

        if (line.front() == L'[') {
            inGeneral = (_wcsicmp(line.c_str(), L"[General]") == 0);
            continue;
        }

        if (!inGeneral) {
            continue;
        }

        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) {
            continue;
        }

        std::wstring key = trim(line.substr(0, eq));
        std::wstring value = trim(line.substr(eq + 1));

        if (_wcsicmp(key.c_str(), L"Title") == 0)
            m_title = value;
        else if (_wcsicmp(key.c_str(), L"DisplayName") == 0)
            m_displayName = value;
        else if (_wcsicmp(key.c_str(), L"Keywords") == 0)
            m_keywords = value;
        else if (_wcsicmp(key.c_str(), L"ContentDescription") == 0)
            m_contentDescription = value;
    }
}

// ---------------------------------------------------------------------------
// IInitializeWithStream
// ---------------------------------------------------------------------------

IFACEMETHODIMP QSearchablePropertyHandler::Initialize(IStream *pstream, DWORD /*grfMode*/)
{
    if (m_initialized)
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

    if (!pstream)
        return E_POINTER;

    parseIniFromStream(pstream);
    m_initialized = true;
    return S_OK;
}

// ---------------------------------------------------------------------------
// IPropertyStore
// ---------------------------------------------------------------------------

static const PROPERTYKEY s_propertyKeys[] = {
    PKEY_ItemNameDisplay,   // 0 - controls the display name in search results
    PKEY_Title,             // 1
    PKEY_Keywords,          // 2
    PKEY_Comment,           // 3
    PKEY_Kind,              // 4
    PKEY_Search_Contents,   // 5 - full-text search content for the indexer
};

static const DWORD s_propertyCount = ARRAYSIZE(s_propertyKeys);

IFACEMETHODIMP QSearchablePropertyHandler::GetCount(DWORD *cProps)
{
    if (!cProps)
        return E_POINTER;
    *cProps = s_propertyCount;
    return S_OK;
}

IFACEMETHODIMP QSearchablePropertyHandler::GetAt(DWORD iProp, PROPERTYKEY *pkey)
{
    if (!pkey)
        return E_POINTER;
    if (iProp >= s_propertyCount)
        return E_INVALIDARG;
    *pkey = s_propertyKeys[iProp];
    return S_OK;
}

IFACEMETHODIMP QSearchablePropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT *pv)
{
    if (!pv)
        return E_POINTER;

    PropVariantInit(pv);

    if (IsEqualPropertyKey(key, PKEY_ItemNameDisplay)) {
        // Use DisplayName if set, otherwise fall back to Title.
        const std::wstring &name = m_displayName.empty() ? m_title : m_displayName;
        if (!name.empty())
            return InitPropVariantFromString(name.c_str(), pv);
    }
    else if (IsEqualPropertyKey(key, PKEY_Title)) {
        if (!m_title.empty())
            return InitPropVariantFromString(m_title.c_str(), pv);
    }
    else if (IsEqualPropertyKey(key, PKEY_Keywords)) {
        if (!m_keywords.empty()) {
            // Split comma-separated keywords into a string vector.
            std::vector<const wchar_t *> ptrs;
            std::vector<std::wstring> parts;

            size_t start = 0;
            while (start < m_keywords.size()) {
                size_t comma = m_keywords.find(L',', start);
                if (comma == std::wstring::npos) {
                    comma = m_keywords.size();
                }

                std::wstring part = trim(m_keywords.substr(start, comma - start));
                if (!part.empty()) {
                    parts.push_back(part);
                }

                start = comma + 1;
            }

            for (const auto &p : parts)
                ptrs.push_back(p.c_str());

            if (!ptrs.empty()) {
                return InitPropVariantFromStringVector(
                    ptrs.data(), static_cast<ULONG>(ptrs.size()), pv);
            }
        }
    }
    else if (IsEqualPropertyKey(key, PKEY_Comment)) {
        if (!m_contentDescription.empty())
            return InitPropVariantFromString(m_contentDescription.c_str(), pv);
    }
    else if (IsEqualPropertyKey(key, PKEY_Kind)) {
        return InitPropVariantFromString(L"document", pv);
    }
    else if (IsEqualPropertyKey(key, PKEY_Search_Contents)) {
        // Concatenate all text fields so the indexer can match any of them.
        std::wstring content;
        if (!m_title.empty()) {
            content += m_title;
        }
        if (!m_displayName.empty()) {
            if (!content.empty()) {
                content += L' ';
            }
            content += m_displayName;
        }
        if (!m_keywords.empty()) {
            if (!content.empty()) {
                content += L' ';
            }
            content += m_keywords;
        }
        if (!m_contentDescription.empty()) {
            if (!content.empty()) {
                content += L' ';
            }
            content += m_contentDescription;
        }
        if (!content.empty()) {
            return InitPropVariantFromString(content.c_str(), pv);
        }
    }

    return S_OK;
}

IFACEMETHODIMP QSearchablePropertyHandler::SetValue(REFPROPERTYKEY /*key*/,
                                                     REFPROPVARIANT /*propvar*/)
{
    return STG_E_ACCESSDENIED;
}

IFACEMETHODIMP QSearchablePropertyHandler::Commit()
{
    return STG_E_ACCESSDENIED;
}

// ---------------------------------------------------------------------------
// QSearchableClassFactory
// ---------------------------------------------------------------------------

QSearchableClassFactory::QSearchableClassFactory()
    : m_refCount(1)
{
    DllAddRef();
}

IFACEMETHODIMP QSearchableClassFactory::QueryInterface(REFIID riid, void **ppv)
{
    if (!ppv)
        return E_POINTER;

    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory *>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

IFACEMETHODIMP_(ULONG) QSearchableClassFactory::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

IFACEMETHODIMP_(ULONG) QSearchableClassFactory::Release()
{
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }

    return ref;
}

IFACEMETHODIMP QSearchableClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid,
                                                        void **ppv)
{
    if (pUnkOuter) {
        return CLASS_E_NOAGGREGATION;
    }

    auto *handler = new (std::nothrow) QSearchablePropertyHandler();
    if (!handler) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = handler->QueryInterface(riid, ppv);
    handler->Release();
    return hr;
}

IFACEMETHODIMP QSearchableClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
        DllAddRef();
    else
        DllRelease();
    return S_OK;
}
