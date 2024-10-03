#include "core/clipboard.hpp"
#include "core/core.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#include <shlobj_core.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace Toolbox {

    SystemClipboard::SystemClipboard() {}
    SystemClipboard::~SystemClipboard() {}

#ifdef TOOLBOX_PLATFORM_WINDOWS

    Result<std::vector<std::string>, ClipboardError>
    SystemClipboard::getAvailableContentFormats() const {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<std::vector<std::string>>("Failed to open the clipboard!");
        }

        std::vector<std::string> types;
        UINT format = 0;
        while ((format = EnumClipboardFormats(format)) != CF_NULL) {
            std::string the_format = "";

            for (const auto &[mime, custom_fmt] : m_mime_to_format) {
                if (format == custom_fmt) {
                    the_format = mime;
                    break;
                }
            }

            if (the_format.empty()) {
                the_format = MimeForFormat(format);
            }

            if (!the_format.empty()) {
                types.push_back(the_format);
            }
        }

        if (!CloseClipboard()) {
            return make_clipboard_error<std::vector<std::string>>("Failed to close the clipboard!");
        }

        return types;
    }

    Result<std::string, ClipboardError> SystemClipboard::getText() {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<std::string>("Failed to open the clipboard!");
        }

        HANDLE clipboard_data = GetClipboardData(CF_TEXT);
        if (!clipboard_data) {
            return make_clipboard_error<std::string>("Failed to retrieve the data handle!");
        }

        const char *text_data = static_cast<const char *>(GlobalLock(clipboard_data));
        if (!text_data) {
            return make_clipboard_error<std::string>(
                "Failed to retrieve the buffer from the handle!");
        }

        std::string result = text_data;

        if (!GlobalUnlock(clipboard_data)) {
            return make_clipboard_error<std::string>("Failed to unlock the data handle!");
        }

        if (!CloseClipboard()) {
            return make_clipboard_error<std::string>("Failed to close the clipboard!");
        }

        return result;
    }

    Result<void, ClipboardError> SystemClipboard::setText(const std::string &text) {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<void>("Failed to open the clipboard!");
        }

        if (!EmptyClipboard()) {
            return make_clipboard_error<void>("Failed to clear the clipboard!");
        }

        HANDLE data_handle = GlobalAlloc(GMEM_DDESHARE, text.size() + 1);
        if (!data_handle) {
            return make_clipboard_error<void>("Failed to alloc new data handle!");
        }

        char *data_buffer = static_cast<char *>(GlobalLock(data_handle));
        if (!data_buffer) {
            return make_clipboard_error<void>("Failed to retrieve the buffer from the handle!");
        }

        strncpy_s(data_buffer, text.size() + 1, text.c_str(), text.size() + 1);

        if (!GlobalUnlock(data_handle)) {
            return make_clipboard_error<void>("Failed to unlock the data handle!");
        }

        SetClipboardData(CF_TEXT, data_handle);

        if (!CloseClipboard()) {
            return make_clipboard_error<void>("Failed to close the clipboard!");
        }
        return {};
    }

    Result<MimeData, ClipboardError> SystemClipboard::getContent(const std::string &type) {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<MimeData>("Failed to open the clipboard!");
        }

        UINT format = CF_NULL;
        if (m_mime_to_format.find(type) == m_mime_to_format.end()) {
            format = FormatForMime(type);
            if (format == CF_NULL) {
                m_mime_to_format[type] = RegisterClipboardFormat(type.c_str());
                format                 = m_mime_to_format[type];
            }
        } else {
            format = m_mime_to_format[type];
        }

        if (format == CF_NULL) {
            return make_clipboard_error<MimeData>("Failed to register the clipboard format!");
        }

        HANDLE clipboard_data = GetClipboardData(format);
        if (!clipboard_data) {
            return make_clipboard_error<MimeData>("Failed to retrieve the data handle!");
        }

        HANDLE data_handle = static_cast<HANDLE>(GlobalLock(clipboard_data));
        if (!data_handle) {
            return make_clipboard_error<MimeData>("Failed to retrieve the buffer from the handle!");
        }

        if (format == CF_TEXT) {
            const char *text_data = static_cast<const char *>(data_handle);
            std::string result    = text_data;

            if (!GlobalUnlock(clipboard_data)) {
                return make_clipboard_error<MimeData>("Failed to unlock the data handle!");
            }

            if (!CloseClipboard()) {
                return make_clipboard_error<MimeData>("Failed to close the clipboard!");
            }

            MimeData mime_data;
            mime_data.set_text(result);
            return mime_data;
        }

        if (format == CF_HDROP) {
            HDROP hdrop    = static_cast<HDROP>(data_handle);
            UINT num_files = DragQueryFile(hdrop, 0xFFFFFFFF, nullptr, 0);
            std::string uri_list;
            for (UINT i = 0; i < num_files; ++i) {
                UINT size = DragQueryFile(hdrop, i, nullptr, 0);
                std::string file(size, '\0');
                DragQueryFile(hdrop, i, file.data(), size + 1);
                uri_list += std::format("file:///{}", file);
                if (i != num_files - 1) {
                    uri_list += "\r\n";
                }
            }

            if (!GlobalUnlock(clipboard_data)) {
                return make_clipboard_error<MimeData>("Failed to unlock the data handle!");
            }

            if (!CloseClipboard()) {
                return make_clipboard_error<MimeData>("Failed to close the clipboard!");
            }

            MimeData mime_data;
            mime_data.set_urls(uri_list);
            return mime_data;
        }

        if (format == CF_UNICODETEXT) {
            const wchar_t *text_data = static_cast<const wchar_t *>(data_handle);
            std::wstring result      = text_data;

            if (!GlobalUnlock(clipboard_data)) {
                return make_clipboard_error<MimeData>("Failed to unlock the data handle!");
            }

            if (!CloseClipboard()) {
                return make_clipboard_error<MimeData>("Failed to close the clipboard!");
            }

            MimeData mime_data;
            mime_data.set_text(std::string(result.begin(), result.end()));
            return mime_data;
        }

        return make_clipboard_error<MimeData>("Unimplemented MIME type!");
    }

    Result<void, ClipboardError> SystemClipboard::setContent(const std::string &type,
                                                             const MimeData &mimedata) {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<void>("Failed to open the clipboard!");
        }

        if (!EmptyClipboard()) {
            return make_clipboard_error<void>("Failed to clear the clipboard!");
        }

        std::optional<Buffer> result = mimedata.get_data(type);
        if (!result) {
            return make_clipboard_error<void>(
                std::format("Failed to find MIME data type \"{}\"", type));
        }

        Buffer data_buf = std::move(result.value());

        UINT format = CF_NULL;
        if (m_mime_to_format.find(type) == m_mime_to_format.end()) {
            format = FormatForMime(type);
            if (format == CF_NULL) {
                m_mime_to_format[type] = RegisterClipboardFormat(type.c_str());
                format                 = m_mime_to_format[type];
            }
        } else {
            format = m_mime_to_format[type];
        }

        if (format == CF_NULL) {
            return make_clipboard_error<void>("Failed to register the clipboard format!");
        }

        size_t dest_buf_size = data_buf.size();
        if (format == CF_HDROP) {
            dest_buf_size *= 2;
            dest_buf_size += sizeof(DROPFILES);
        } else if (format == CF_TEXT) {
            dest_buf_size *= 2;
        }

        HANDLE data_handle = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, dest_buf_size);
        if (!data_handle) {
            return make_clipboard_error<void>("Failed to alloc new data handle!");
        }

        void *data_buffer = static_cast<void *>(GlobalLock(data_handle));
        if (!data_buffer) {
            return make_clipboard_error<void>("Failed to retrieve the buffer from the handle!");
        }

        if (format == CF_HDROP) {
            DROPFILES *drop_files_buf = static_cast<DROPFILES *>(data_buffer);
            drop_files_buf->fWide     = TRUE;
            drop_files_buf->pFiles    = sizeof(DROPFILES);

            wchar_t *path_list_ptr = (wchar_t *)((char *)data_buffer + sizeof(DROPFILES));
            std::mbstowcs(path_list_ptr, data_buf.buf<char>(), data_buf.size());

            for (size_t i = 0; i < data_buf.size(); ++i) {
                wchar_t &wchr = path_list_ptr[i];
                if (wchr == L'\n') {
                    wchr = L'\0';
                }
            }

            path_list_ptr[data_buf.size()] = L'\0';
        } else if (format == CF_TEXT) {
            wchar_t *path_list_ptr = (wchar_t *)data_buffer;
            std::mbstowcs(path_list_ptr, data_buf.buf<char>(), data_buf.size());
            path_list_ptr[data_buf.size()] = L'\0';
        } else {
            std::memcpy(data_buffer, data_buf.buf(), data_buf.size());
        }

        if (!GlobalUnlock(data_handle)) {
            return make_clipboard_error<void>("Failed to unlock the data handle!");
        }

        SetClipboardData(format, data_handle);

        if (!CloseClipboard()) {
            return make_clipboard_error<void>("Failed to close the clipboard!");
        }

        return Result<void, ClipboardError>();
    }

    UINT SystemClipboard::FormatForMime(std::string_view mimetype) {
        if (mimetype == "text/plain") {
            return CF_TEXT;
        }

        if (mimetype == "image/bmp") {
            return CF_BITMAP;
        }

        if (mimetype == "image/x-wmf") {
            return CF_METAFILEPICT;
        }

        if (mimetype == "application/vnd.ms-excel") {
            return CF_SYLK;
        }

        if (mimetype == "image/tiff") {
            return CF_TIFF;
        }

        if (mimetype == "audio/riff") {
            return CF_RIFF;
        }

        if (mimetype == "audio/wav") {
            return CF_WAVE;
        }

        if (mimetype == "text/uri-list") {
            return CF_HDROP;
        }

        return CF_NULL;
    }

    std::string SystemClipboard::MimeForFormat(UINT format) {
        switch (format) {
        case CF_TEXT:
            return "text/plain";
        case CF_BITMAP:
            return "image/bmp";
        case CF_METAFILEPICT:
            return "image/x-wmf";
        case CF_SYLK:
            return "application/vnd.ms-excel";
        case CF_DIF:
            return "application/vnd.ms-excel";
        case CF_TIFF:
            return "image/tiff";
        case CF_OEMTEXT:
            return "text/plain";
        case CF_DIB:
            return "";
        case CF_PALETTE:
            return "";
        case CF_PENDATA:
            return "";
        case CF_RIFF:
            return "audio/riff";
        case CF_WAVE:
            return "audio/wav";
        case CF_UNICODETEXT:
            return "text/plain";
        case CF_ENHMETAFILE:
            return "";
#if (WINVER >= 0x0400)
        case CF_HDROP:
            return "text/uri-list";
        case CF_LOCALE:
            return "";
#endif /* WINVER >= 0x0400 */
#if (WINVER >= 0x0500)
        case CF_DIBV5:
            return "";
#endif /* WINVER >= 0x0500 */
        default:
            return "";
        }
    }

#elif defined(TOOLBOX_PLATFORM_LINUX)
    Result<std::vector<std::string>, ClipboardError>
    SystemClipboard::getAvailableContentFormats() const {
        Display *dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            return make_clipboard_error<std::vector<std::string>>("Could not open X display");
        }
        Atom sel             = XInternAtom(dpy, "CLIPBOARD", False);
        Atom targets         = XInternAtom(dpy, "TARGETS", False);
        Atom target_property = XInternAtom(dpy, "CLIP_TYPES", False);
        Window root_window   = RootWindow(dpy, DefaultScreen(dpy));
        Window target_window = XCreateSimpleWindow(dpy, root_window, -10, -10, 1, 1, 0, 0, 0);
        XConvertSelection(dpy, sel, targets, target_property, target_window, CurrentTime);

        XEvent ev;
        XSelectionEvent *sev;
        // This looks pretty dangerous but this is how glfw does it,
        // and how the examples online do it, so :shrug:
        while (true) {
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case SelectionNotify:
                sev = (XSelectionEvent *)&ev.xselection;
                if (sev->property == None) {
                    return make_clipboard_error<std::vector<std::string>>(
                        "Conversion could not be performed.\n");
                } else {
                    std::vector<std::string> types;
                    unsigned long nitems;
                    unsigned char *prop_ret = nullptr;
                    // We won't use these.
                    Atom _type;
                    int _di;
                    unsigned long _dul;
                    XGetWindowProperty(dpy, target_window, target_property, 0, 1024 * sizeof(Atom),
                                       False, XA_ATOM, &_type, &_di, &nitems, &_dul, &prop_ret);

                    Atom *targets = (Atom *)prop_ret;
                    for (int i = 0; i < nitems; ++i) {
                        char *an = XGetAtomName(dpy, targets[i]);
                        types.push_back(std::string(an));
                        if (an)
                            XFree(an);
                    }
                    XFree(prop_ret);
                    XDeleteProperty(dpy, target_window, target_property);
                    return types;
                }
                break;
            }
        }
        return {};
    }

    Result<std::string, ClipboardError> SystemClipboard::getText() {
        return make_clipboard_error<std::string>("Not implemented yet");
    }

    Result<void, ClipboardError> SystemClipboard::setText(const std::string &text) {
        return make_clipboard_error<void>("Not implemented yet");
    }

    Result<MimeData, ClipboardError> SystemClipboard::getContent(const std::string &type) {
        Display *dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            return make_clipboard_error<MimeData>("Could not open X display");
        }
        Atom requested_type    = XInternAtom(dpy, type.c_str(), False);
        Atom sel               = XInternAtom(dpy, "CLIPBOARD", False);
        Window clipboard_owner = XGetSelectionOwner(dpy, sel);
        if (clipboard_owner == None) {
            return make_clipboard_error<MimeData>("Clipboard isn't owned by anyone");
        }
        Window root          = RootWindow(dpy, DefaultScreen(dpy));
        Window target_window = XCreateSimpleWindow(dpy, root, -10, -10, 1, 1, 0, 0, 0);
        Atom target_property = XInternAtom(dpy, "CLIP_CONTENTS", False);
        XConvertSelection(dpy, sel, requested_type, target_property, target_window, CurrentTime);
        XEvent ev;
        XSelectionEvent *sev;
        // This looks pretty dangerous but this is how glfw does it,
        // and how the examples online do it, so :shrug:
        while (true) {
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case SelectionNotify:
                sev = (XSelectionEvent *)&ev.xselection;
                if (sev->property == None) {
                    return make_clipboard_error<MimeData>("Conversion could not be performed.");
                } else {
                    Atom type_received;
                    unsigned long size;
                    unsigned char *prop_ret = nullptr;
                    // These are unused
                    int _di;
                    unsigned long _dul;
                    Atom _da;

                    XGetWindowProperty(dpy, target_window, target_property, 0, 0, False,
                                       AnyPropertyType, &type_received, &_di, &_dul, &size,
                                       &prop_ret);
                    XFree(prop_ret);

                    Atom incr = XInternAtom(dpy, "INCR", False);
                    if (type_received == incr) {
                        return make_clipboard_error<MimeData>(
                            "Data over 256kb, this isn't supported yet");
                    }
                    XGetWindowProperty(dpy, target_window, target_property, 0, size, False,
                                       AnyPropertyType, &_da, &_di, &_dul, &_dul, &prop_ret);
                    Buffer data_buffer;
                    if (!data_buffer.alloc(size)) {
                        return make_clipboard_error<MimeData>(
                            "Couldn't allocate buffer of big enough size");
                    }
                    std::memcpy(data_buffer.buf(), prop_ret, size);
                    XFree(prop_ret);
                    XDeleteProperty(dpy, target_window, target_property);

                    MimeData result;
                    result.set_data(type, std::move(data_buffer));
                    return result;
                }
            }
        }
        return {};
    }

    static std::unordered_map<std::string, std::string> s_uti_to_mime = {
        {"public.utf8-plain-text",               "text/plain"            },
        {"public.utf16-plain-text",              "text/plain"            },
        {"public.text",                          "text/plain"            },
        {"public.html",                          "text/html"             },
        {"public.url",                           "text/uri-list"         },
        {"public.file-url",                      "text/uri-list"         },
        {"public.tiff",                          "application/x-qt-image"},
        {"public.vcard",                         "text/plain"            },
        {"com.apple.traditional-mac-plain-text", "text/plain"            },
        {"com.apple.pict",                       "application/x-qt-image"},
    };

    static std::unordered_map<std::string, std::string> s_mime_to_uti = {
        {"text/plain",             "public.text"},
        {"text/html",              "public.html"},
        {"text/uri-list",          "public.url" },
        {"application/x-qt-image", "public.tiff"},
    };

    std::string MimeData::UTIForMime(std::string_view mimetype) {
        return s_mime_to_uti[std::string(mimetype)];
    }
    std::string MimeData::MimeForUTI(std::string_view uti) {
        return s_uti_to_mime[std::string(uti)];
    }
#endif

}  // namespace Toolbox
