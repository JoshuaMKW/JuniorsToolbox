#include "p_windataobj.hpp"

#include "image/imagebuilder.hpp"
#include "strutil.hpp"

namespace Toolbox::UI {

    /*static void n() {
          for (const std::string &file_path : mime_urls.value()) {
              size_t path_size = file_path.size();
              std::mbstowcs(file_path_buffer, file_path.c_str(), path_size);
              file_path_buffer += path_size;
              *file_path_buffer++ = L'\0';
          }
    }*/

    //// Here we convert the image data of the MimeData
    //// to a BITMAP, and then to an HBITMAP in accordance
    //// with the HBITMAP storage specification.
    //// ---
    // std::optional<ImageData> result = data.get_image();
    // if (!result) {
    //     TOOLBOX_ERROR("MimeData had no BITMAP data but STGMEDIUM requested it");
    //     return E_FAIL;
    // }

    // HBITMAP bitmap = CreateHBITMAPFromImageData(result.value());

    static HBITMAP CopyHBITMAP(HBITMAP hbitmap) {
        BITMAP bmp = {};
        if (!GetObject(hbitmap, sizeof(BITMAP), &bmp)) {
            TOOLBOX_ERROR("Failed to get bitmap object");
            return nullptr;
        }

        HDC hdc_src = CreateCompatibleDC(NULL);
        if (!hdc_src) {
            TOOLBOX_ERROR("Failed to create compatible DC");
            return nullptr;
        }

        HDC hdc_dst = CreateCompatibleDC(NULL);
        if (!hdc_dst) {
            TOOLBOX_ERROR("Failed to create compatible DC");
            DeleteDC(hdc_src);
            return nullptr;
        }

        HBITMAP hbitmap_copy = CreateCompatibleBitmap(hdc_src, bmp.bmWidth, bmp.bmHeight);
        if (!hbitmap_copy) {
            TOOLBOX_ERROR("Failed to create compatible bitmap");
            DeleteDC(hdc_src);
            DeleteDC(hdc_dst);
            return nullptr;
        }

        HGDIOBJ old_src = SelectObject(hdc_src, hbitmap);
        HGDIOBJ old_dst = SelectObject(hdc_dst, hbitmap_copy);

        BitBlt(hdc_dst, 0, 0, bmp.bmWidth, bmp.bmHeight, hdc_src, 0, 0, SRCCOPY);

        SelectObject(hdc_src, old_src);
        SelectObject(hdc_dst, old_dst);
        DeleteDC(hdc_src);
        DeleteDC(hdc_dst);

        return hbitmap_copy;
    }

    static HBITMAP CreateHBITMAPFromImageData(const ImageData &data) {
        ImageBuilder::SwizzleMatrix mtx = {};
        mtx[ImageBuilder::RED]          = ImageBuilder::BLUE;
        mtx[ImageBuilder::GREEN]        = ImageBuilder::GREEN;
        mtx[ImageBuilder::BLUE]         = ImageBuilder::RED;
        mtx[ImageBuilder::ALPHA]        = ImageBuilder::ALPHA;

        ScopePtr<ImageData> bgra_img_data = ImageBuilder::ImageSwizzle(data, mtx);

        int width  = bgra_img_data->getWidth();
        int height = bgra_img_data->getHeight();

        // Setup the BITMAPINFO header
        // ---
        BITMAPINFO bi              = {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = width;
        bi.bmiHeader.biHeight      = -height;  // Having height negative enforces a top-down DIB.
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        bi.bmiHeader.biSizeImage   = 0;

        // Create the DIB section
        // ---
        void *dib_data = nullptr;

        HDC hdc        = GetDC(NULL);
        HBITMAP bitmap = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &dib_data, NULL, 0);
        ReleaseDC(NULL, hdc);

        if (!bitmap || !dib_data) {
            TOOLBOX_ERROR("Failed to create HBITMAP or DIB from ImageData");
        }

        memcpy_s(dib_data, static_cast<size_t>(width * height), bgra_img_data->getData(),
                 bgra_img_data->getSize());

        return bitmap;
    }

    static HRESULT ApplyFilesToStgMedium(STGMEDIUM *pmediumIn, STGMEDIUM *pmediumOut) {
        if (!pmediumIn || !pmediumOut) {
            TOOLBOX_ERROR("Invalid STGMEDIUM pointers provided");
            return E_INVALIDARG;
        }

        size_t src_size = GlobalSize(pmediumIn->hGlobal);

        // Lock the resources and begin populating with filepaths
        // ---
        DROPFILES *files_src = static_cast<DROPFILES *>(GlobalLock(pmediumIn->hGlobal));
        if (!files_src) {
            TOOLBOX_ERROR("Failed to lock global memory for DROPFILES structure");
            return E_FAIL;
        }

        HGLOBAL globl_out = GlobalAlloc(GHND, src_size);
        if (!globl_out) {
            TOOLBOX_ERROR("Failed to allocate global memory for destination STGMEDIUM");
            GlobalUnlock(pmediumIn->hGlobal);
            return E_FAIL;
        }

        DROPFILES *files_dst = static_cast<DROPFILES *>(GlobalLock(globl_out));
        if (!files_dst) {
            TOOLBOX_ERROR("Failed to lock global memory for DROPFILES structure");
            return E_FAIL;
        }

        // Copy the DROPFILES structure from source to destination
        memcpy_s(files_dst, src_size, files_src, src_size);

        GlobalUnlock(globl_out);
        GlobalUnlock(pmediumIn->hGlobal);

        // Assign the information to the STGMEDIUM
        // ---
        pmediumOut->tymed          = TYMED_HGLOBAL;
        pmediumOut->hGlobal        = globl_out;  // Use the same global memory handle
        pmediumOut->pUnkForRelease = nullptr;

        return S_OK;
    }

    static HRESULT ApplyFilesToStgMediumHere(STGMEDIUM *pmediumIn, STGMEDIUM *pmediumOut) {
        if (!pmediumIn || !pmediumOut) {
            TOOLBOX_ERROR("Invalid STGMEDIUM pointers provided");
            return E_INVALIDARG;
        }

        size_t src_size = GlobalSize(pmediumIn->hGlobal);
        size_t dst_size = GlobalSize(pmediumOut->hGlobal);
        if (src_size > dst_size) {
            TOOLBOX_ERROR("Source STGMEDIUM size is larger than destination STGMEDIUM size");
            return E_FAIL;
        }

        // Lock the resources and begin populating with filepaths
        // ---
        DROPFILES *files_src = static_cast<DROPFILES *>(GlobalLock(pmediumIn->hGlobal));
        if (!files_src) {
            TOOLBOX_ERROR("Failed to lock global memory for DROPFILES structure");
            return E_FAIL;
        }

        DROPFILES *files_dst = static_cast<DROPFILES *>(GlobalLock(pmediumOut->hGlobal));
        if (!files_dst) {
            TOOLBOX_ERROR("Failed to lock global memory for DROPFILES structure");
            return E_FAIL;
        }

        // Copy the DROPFILES structure from source to destination
        memcpy_s(files_dst, dst_size, files_src, src_size);

        GlobalUnlock(pmediumOut->hGlobal);
        GlobalUnlock(pmediumIn->hGlobal);

        // Assign the information to the STGMEDIUM
        // ---
        pmediumOut->tymed = TYMED_HGLOBAL;

        return S_OK;
    }

    static HRESULT ApplyBitmapToStgMedium(STGMEDIUM *pmediumIn, STGMEDIUM *pmediumOut) {
        HBITMAP bitmap = CopyHBITMAP(pmediumIn->hBitmap);

        pmediumOut->tymed          = TYMED_GDI;
        pmediumOut->hBitmap        = bitmap;
        pmediumOut->pUnkForRelease = nullptr;

        return S_OK;
    }

    void WindowsOleDataObject::setMimeData(const MimeData &mime_data) {
        if (mime_data.has_text()) {
            std::string text_data = mime_data.get_text().value();

            FORMATETC fmt_etc    = {};
            STGMEDIUM stg_medium = {};

            fmt_etc.cfFormat = CF_UNICODETEXT;
            fmt_etc.ptd      = nullptr;
            fmt_etc.dwAspect = DVASPECT_CONTENT;
            fmt_etc.lindex   = -1;
            fmt_etc.tymed    = TYMED_HGLOBAL;

            stg_medium.tymed = TYMED_HGLOBAL;
            stg_medium.hGlobal =
                GlobalAlloc(GMEM_MOVEABLE, (text_data.size() + 1) * sizeof(wchar_t));
            if (!stg_medium.hGlobal) {
                TOOLBOX_ERROR("Failed to allocate global memory for text data");
            } else {
                stg_medium.pUnkForRelease = nullptr;
                wchar_t *wtext            = static_cast<wchar_t *>(GlobalLock(stg_medium.hGlobal));
                if (wtext) {
                    size_t text_size = text_data.size();

                    // Convert the string from multibyte to wide character
                    String::asEncoding(text_data, "UTF-8", "UTF-16LE")
                        .and_then([&](const std::string &converted_text) {
                            text_size = converted_text.size();
                            memcpy_s(wtext, text_size, converted_text.data(), text_size);
                            return Result<std::string, String::EncodingError>();
                        })
                        .or_else([&](const String::EncodingError &err) {
                            TOOLBOX_ERROR(err.m_message[0]);
                            return Result<std::string, String::EncodingError>();
                        });

                    wtext[text_size / 2] = L'\0';  // Null-terminate the string
                    GlobalUnlock(stg_medium.hGlobal);
                    SetData(&fmt_etc, &stg_medium, TRUE);
                } else {
                    TOOLBOX_ERROR("Failed to lock global memory for text data");
                }
            }
        }

        if (mime_data.has_html()) {
            std::string text_data = mime_data.get_html().value();

            FORMATETC fmt_etc    = {};
            STGMEDIUM stg_medium = {};

            fmt_etc.cfFormat = CF_UNICODETEXT;
            fmt_etc.ptd      = nullptr;
            fmt_etc.dwAspect = DVASPECT_CONTENT;
            fmt_etc.lindex   = -1;
            fmt_etc.tymed    = TYMED_HGLOBAL;

            stg_medium.tymed = TYMED_HGLOBAL;
            stg_medium.hGlobal =
                GlobalAlloc(GMEM_MOVEABLE, (text_data.size() + 1) * sizeof(wchar_t));
            if (!stg_medium.hGlobal) {
                TOOLBOX_ERROR("Failed to allocate global memory for text data");
            } else {
                stg_medium.pUnkForRelease = nullptr;
                wchar_t *wtext            = static_cast<wchar_t *>(GlobalLock(stg_medium.hGlobal));
                if (wtext) {
                    size_t text_size = text_data.size();

                    // Convert the string from multibyte to wide character
                    String::asEncoding(text_data, "UTF-8", "UTF-16LE")
                        .and_then([&](const std::string &converted_text) {
                            text_size = converted_text.size();
                            memcpy_s(wtext, text_size, converted_text.data(), text_size);
                            return Result<std::string, String::EncodingError>();
                        })
                        .or_else([&](const String::EncodingError &err) {
                            TOOLBOX_ERROR(err.m_message[0]);
                            return Result<std::string, String::EncodingError>();
                        });

                    wtext[text_size / 2] = L'\0';  // Null-terminate the string
                    GlobalUnlock(stg_medium.hGlobal);
                    SetData(&fmt_etc, &stg_medium, TRUE);
                } else {
                    TOOLBOX_ERROR("Failed to lock global memory for text data");
                }
            }
        }

        if (mime_data.has_urls()) {
            std::vector<std::string> urls = mime_data.get_urls().value();

            FORMATETC fmt_etc    = {};
            STGMEDIUM stg_medium = {};

            fmt_etc.cfFormat = CF_HDROP;
            fmt_etc.ptd      = nullptr;
            fmt_etc.dwAspect = DVASPECT_CONTENT;
            fmt_etc.lindex   = -1;
            fmt_etc.tymed    = TYMED_HGLOBAL;

            stg_medium.tymed          = TYMED_HGLOBAL;
            stg_medium.pUnkForRelease = nullptr;

            size_t total_path_size = sizeof(DROPFILES);
            for (const std::string &path : urls) {
                total_path_size += (path.size() + 1) * sizeof(wchar_t);  // +1 for null-terminator
            }
            total_path_size += sizeof(wchar_t);  // Final null-terminator

            stg_medium.hGlobal = GlobalAlloc(GHND, total_path_size);
            if (!stg_medium.hGlobal) {
                TOOLBOX_ERROR("Failed to allocate global memory for DROPFILES structure");
                return;
            }

            DROPFILES *drop_files = static_cast<DROPFILES *>(GlobalLock(stg_medium.hGlobal));
            if (!drop_files) {
                TOOLBOX_ERROR("Failed to lock global memory for DROPFILES structure");
                return;
            }

            drop_files->pFiles = sizeof(DROPFILES);
            drop_files->pt.x   = 0;
            drop_files->pt.y   = 0;
            drop_files->fNC    = TRUE;
            drop_files->fWide  = TRUE;  // Use wide character format

            wchar_t *file_path_buffer = reinterpret_cast<wchar_t *>(
                reinterpret_cast<char *>(drop_files) + drop_files->pFiles);
            for (const std::string &file_path : urls) {
                size_t path_size = file_path.size();

                // Convert the string from multibyte to wide character
                String::asEncoding(file_path, "UTF-8", "UTF-16LE")
                    .and_then([&](const std::string &converted_text) {
                        path_size = converted_text.size();
                        memcpy_s(file_path_buffer, path_size, converted_text.data(), path_size);
                        return Result<std::string, String::EncodingError>();
                    })
                    .or_else([&](const String::EncodingError &err) {
                        TOOLBOX_ERROR(err.m_message[0]);
                        return Result<std::string, String::EncodingError>();
                    });

                file_path_buffer += path_size / 2;
                *file_path_buffer++ = L'\0';  // Null-terminate each file path
            }

            GlobalUnlock(stg_medium.hGlobal);
            SetData(&fmt_etc, &stg_medium, TRUE);
        }

        if (mime_data.has_image()) {
            FORMATETC fmt_etc    = {};
            STGMEDIUM stg_medium = {};

            fmt_etc.cfFormat = CF_BITMAP;
            fmt_etc.ptd      = nullptr;
            fmt_etc.dwAspect = DVASPECT_CONTENT;
            fmt_etc.lindex   = -1;
            fmt_etc.tymed    = TYMED_GDI;

            stg_medium.tymed   = TYMED_GDI;
            stg_medium.hBitmap = CreateHBITMAPFromImageData(mime_data.get_image().value());
            if (!stg_medium.hBitmap) {
                TOOLBOX_ERROR("Failed to create HBITMAP from image data");
            } else {
                stg_medium.pUnkForRelease = nullptr;
                SetData(&fmt_etc, &stg_medium, TRUE);
            }
        }
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::QueryInterface(REFIID riid, void **ppvObject) {
        if (!ppvObject) {
            return E_POINTER;
        }

        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG __stdcall) WindowsOleDataObject::AddRef() {
        return InterlockedIncrement(&m_ref_count);
    }

    STDMETHODIMP_(ULONG __stdcall) WindowsOleDataObject::Release() {
        ULONG ref_count = InterlockedDecrement(&m_ref_count);
        if (ref_count == 0) {
            delete this;
        }
        return ref_count;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::GetData(FORMATETC *pformatetcIn, STGMEDIUM *pmedium) {
        auto stg_it = std::find_if(m_entries.begin(), m_entries.end(),
                                   [pformatetcIn](const FormatEntry &entry) {
                                       return entry.m_fmt.cfFormat == pformatetcIn->cfFormat &&
                                              entry.m_fmt.tymed == pformatetcIn->tymed;
                                   });
        if (stg_it == m_entries.end()) {
            TOOLBOX_ERROR("No matching FORMATETC found in entries");
            return DV_E_FORMATETC;
        }

        if (pformatetcIn->cfFormat == CF_HDROP) {
            if (pformatetcIn->tymed != TYMED_HGLOBAL) {
                TOOLBOX_ERROR("FORMATETC asked for a CF_HDROP, but specified a non-TYMED_HGLOBAL");
                return DV_E_TYMED;
            }

            return ApplyFilesToStgMedium(&stg_it->m_stg, pmedium);
        }

        if (pformatetcIn->cfFormat == CF_BITMAP) {
            if (pformatetcIn->tymed != TYMED_GDI) {
                TOOLBOX_ERROR("FORMATETC asked for a CF_BITMAP, but specified a non-TYMED_GDI");
                return DV_E_TYMED;
            }

            return ApplyBitmapToStgMedium(&stg_it->m_stg, pmedium);
        }

        return DV_E_FORMATETC;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium) {
        return DV_E_FORMATETC;
    }

    STDMETHODIMP_(HRESULT __stdcall) WindowsOleDataObject::QueryGetData(FORMATETC *pformatetc) {
        if (pformatetc->cfFormat == CF_HDROP &&
            (pformatetc->tymed & TYMED_HGLOBAL) == TYMED_HGLOBAL) {
            return S_OK;
        }

        if (pformatetc->cfFormat == CF_BITMAP && (pformatetc->tymed & TYMED_GDI) == TYMED_GDI) {
            return S_OK;
        }

        static const UINT cfFileContents   = RegisterClipboardFormat(CFSTR_FILECONTENTS);
        static const UINT cfFileDescriptor = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);

        if (pformatetc->cfFormat == cfFileContents || pformatetc->cfFormat == cfFileDescriptor) {
            if ((pformatetc->tymed & TYMED_HGLOBAL) == TYMED_HGLOBAL) {
                return S_OK;
            }
        }

        return DV_E_FORMATETC;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::GetCanonicalFormatEtc(FORMATETC *pformatectIn, FORMATETC *pformatetcOut) {
        pformatetcOut->ptd = nullptr;
        return E_NOTIMPL;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::SetData(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) {
        if (pformatetc->tymed != TYMED_HGLOBAL) {
            return DV_E_TYMED;
        }

        if (fRelease) {
            // Caller does not request ownership, so we assume the data

            FORMATETC format_etc = *pformatetc;
            if (format_etc.ptd) {
#if 1
                format_etc.ptd = nullptr;  // We don't support ptd, so we set it to nullptr
#else
                format_etc.ptd = (DVTARGETDEVICE *)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));
                memcpy(format_etc.ptd, pformatetc->ptd, sizeof(DVTARGETDEVICE));
#endif
            }

            m_entries.push_back({format_etc, *pmedium});

            ReleaseStgMedium(pmedium);
            return S_OK;
        }

        // Caller requests ownership so we copy the STGMEDIUM
        if ((pmedium->tymed & TYMED_HGLOBAL) == TYMED_HGLOBAL) {
            STGMEDIUM new_medium = {};
            new_medium.tymed     = TYMED_HGLOBAL;

            new_medium.hGlobal = GlobalAlloc(GHND, GlobalSize(pmedium->hGlobal));
            if (!new_medium.hGlobal) {
                TOOLBOX_ERROR("Failed to allocate global memory for STGMEDIUM");
                return E_OUTOFMEMORY;
            }

            void *src_data = GlobalLock(pmedium->hGlobal);
            if (!src_data) {
                TOOLBOX_ERROR("Failed to lock global memory for STGMEDIUM");
                return E_FAIL;
            }

            void *dest_data = GlobalLock(new_medium.hGlobal);
            if (!dest_data) {
                GlobalUnlock(pmedium->hGlobal);
                TOOLBOX_ERROR("Failed to lock global memory for STGMEDIUM");
                return E_FAIL;
            }

            memcpy(dest_data, src_data, GlobalSize(pmedium->hGlobal));

            GlobalUnlock(pmedium->hGlobal);
            GlobalUnlock(new_medium.hGlobal);

            m_entries.push_back({*pformatetc, new_medium});
        } else if ((pmedium->tymed & TYMED_GDI) == TYMED_GDI) {
            STGMEDIUM new_medium = {};
            new_medium.tymed     = TYMED_GDI;
            new_medium.hBitmap   = CopyHBITMAP(pmedium->hBitmap);
            m_entries.push_back({*pformatetc, new_medium});
        } else {
            TOOLBOX_ERROR("Unsupported TYMED in SetData");
            return DV_E_TYMED;
        }

        return S_OK;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc) {
#if 1
      if (!ppenumFormatEtc) {
            return E_POINTER;
        }

        if (dwDirection != DATADIR_GET) {
            return S_FALSE;  // We only support getting data
        }

        std::vector<FORMATETC> formats;
        for (auto &entry : m_entries) {
            formats.push_back(entry.m_fmt);
        }

        return SHCreateStdEnumFmtEtc(static_cast<UINT>(formats.size()), formats.data(),
                                     ppenumFormatEtc);
#else
        return OLE_S_USEREG;
#endif
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink,
                                  DWORD *pdwConnection) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP_(HRESULT __stdcall) WindowsOleDataObject::DUnadvise(DWORD dwConnection) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    STDMETHODIMP_(HRESULT __stdcall)
    WindowsOleDataObject::EnumDAdvise(IEnumSTATDATA **ppenumAdvise) {
        return OLE_E_ADVISENOTSUPPORTED;
    }

}  // namespace Toolbox::UI
