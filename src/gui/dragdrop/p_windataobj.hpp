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

        // -- INTERFACE -- //

        void setMimeData(const MimeData &mime_data);

        // ----- END ----- //

        // IUnknown
        HRESULT QueryInterface(REFIID riid, void **ppvObject) override;
        ULONG AddRef() override;
        ULONG Release() override;

        // IDataObject
        HRESULT GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) override;
        HRESULT GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium) override;
        HRESULT QueryGetData(FORMATETC *pformatetc) override;
        HRESULT GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) override;
        HRESULT SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) override;
        HRESULT EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) override;
        HRESULT DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink,
                        DWORD *pdwConnection) override;
        HRESULT DUnadvise(DWORD dwConnection) override;
        HRESULT EnumDAdvise(IEnumSTATDATA **ppenumAdvise) override;

    private:
        struct FormatEntry {
            FORMATETC m_fmt;
            STGMEDIUM m_stg;
        };

        ULONG m_ref_count = 0;
        std::vector<FormatEntry> m_entries;
    };

}  // namespace Toolbox::UI