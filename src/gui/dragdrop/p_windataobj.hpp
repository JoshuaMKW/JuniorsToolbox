#pragma once

#include "core/core.hpp"
#include "core/mimedata/mimedata.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>
#else
#error "This header is Windows only!"
#endif

namespace Toolbox::UI {

    class WindowsOleDataObject : public IDataObject {
    public:
        WindowsOleDataObject()          = default;
        virtual ~WindowsOleDataObject() = default;

        // IUnknown
        STDMETHOD(QueryInterface)(REFIID riid, void **ppvObject) override;
        STDMETHOD_(ULONG, AddRef)() override;
        STDMETHOD_(ULONG, Release)() override;

        // IDataObject
        STDMETHOD(GetData)(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) override;
        STDMETHOD(GetDataHere)(FORMATETC *pformatetc, STGMEDIUM *pmedium) override;
        STDMETHOD(QueryGetData)(FORMATETC *pformatetc) override;
        STDMETHOD(GetCanonicalFormatEtc)
        (FORMATETC *pformatectIn, FORMATETC *pformatetcOut) override;
        STDMETHOD(SetData)(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) override;
        STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) override;
        STDMETHOD(DAdvise)
        (FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection) override;
        STDMETHOD(DUnadvise)(DWORD dwConnection) override;
        STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise) override;

        void setMimeData(MimeData &&mime_data) { m_mime_data = std::move(mime_data); }

    private:
        ULONG m_ref_count = 0;
        MimeData m_mime_data;
    };

}  // namespace Toolbox::UI