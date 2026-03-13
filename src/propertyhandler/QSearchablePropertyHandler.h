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

#ifndef QSEARCHABLEPROPERTYHANDLER_H
#define QSEARCHABLEPROPERTYHANDLER_H

#include <windows.h>
#include <propsys.h>
#include <string>

// Property handler COM object for QSearchable .qs#### files.
// Implements IInitializeWithStream + IPropertyStore so that Windows Search
// can extract structured metadata (display name, title, keywords, etc.)
// from the INI-format item files.

class QSearchablePropertyHandler : public IInitializeWithStream, public IPropertyStore
{
public:
    QSearchablePropertyHandler();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream *pstream, DWORD grfMode) override;

    // IPropertyStore
    IFACEMETHODIMP GetCount(DWORD *cProps) override;
    IFACEMETHODIMP GetAt(DWORD iProp, PROPERTYKEY *pkey) override;
    IFACEMETHODIMP GetValue(REFPROPERTYKEY key, PROPVARIANT *pv) override;
    IFACEMETHODIMP SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
    IFACEMETHODIMP Commit() override;

private:
    ~QSearchablePropertyHandler();

    void parseIniFromStream(IStream *pstream);

    LONG m_refCount;
    bool m_initialized;

    std::wstring m_title;
    std::wstring m_displayName;
    std::wstring m_keywords;
    std::wstring m_contentDescription;
    std::wstring m_appName;
};

// Class factory for QSearchablePropertyHandler.
class QSearchableClassFactory : public IClassFactory
{
public:
    QSearchableClassFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override;
    IFACEMETHODIMP LockServer(BOOL fLock) override;

private:
    LONG m_refCount;
};

// Global DLL reference count.
extern LONG g_dllRefCount;

void DllAddRef();
void DllRelease();

#endif // QSEARCHABLEPROPERTYHANDLER_H
