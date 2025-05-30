#include "core/clipboard.hpp"
#include "core/core.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS
#include <Windows.h>
#include <shlobj_core.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <GLFW/glfw3.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#define GLFW_EXPOSE_NATIVE_X11
#include "strutil.hpp"
#include <GLFW/glfw3native.h>
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

    Result<MimeData, ClipboardError>
    getContentType(std::unordered_map<std::string, UINT> &mime_to_format, const std::string &type) {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<MimeData>("Failed to open the clipboard!");
        }

        UINT format = CF_NULL;
        if (mime_to_format.find(type) == mime_to_format.end()) {
            format = SystemClipboard::FormatForMime(type);
            if (format == CF_NULL) {
                mime_to_format[type] = RegisterClipboardFormat(type.c_str());
                format               = mime_to_format[type];
            }
        } else {
            format = mime_to_format[type];
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
            std::vector<std::string> uri_list;
            uri_list.reserve(num_files);
            for (UINT i = 0; i < num_files; ++i) {
                UINT size = DragQueryFile(hdrop, i, nullptr, 0);
                std::string file(size, '\0');
                DragQueryFile(hdrop, i, file.data(), size + 1);
                uri_list.push_back(file);
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
    Result<std::string, ClipboardError> SystemClipboard::getText() const {
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
    // Assume the clipboard contains file paths and return them in a
    // vector. Returns a clipboardError if the clipboard doesn't have
    // files in it.
    Result<std::vector<fs_path>, ClipboardError> SystemClipboard::getFiles() const {
        // Get the file list target.
        auto data = getContentType(m_mime_to_format, "text/uri-list");
        // If the clipboard doesn't have a file list, propogate the
        // error.
        if (!data) {
            return std::unexpected<ClipboardError>(data.error());
        }
        auto lines = data.value().get_urls().value();
        // Allocate a vector of paths, and convert the vector of
        // strings to it. When doing so, strip off the "file://"
        // prefix.
        std::vector<fs_path> result;
        result.reserve((int)lines.size());
        for (auto &line : lines) {
            if (line.starts_with("file:/")) {
                if (line.starts_with("file:///")) {
                    result.push_back(line.substr(8));
                } else {
                    result.push_back(line.substr(7));
                }
            } else {
                return make_clipboard_error<std::vector<fs_path>>("Can't copy non file uri");
            }
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
    Result<MimeData, ClipboardError> SystemClipboard::getContent() const {
        // Figure out all possible formats the content can be in.
        auto formats = getAvailableContentFormats();
        // If that didn't work, propogate the error.
        if (!formats) {
            return std::unexpected<ClipboardError>(formats.error());
        }
        // For each possible data type...
        MimeData result;
        for (std::string &target : formats.value()) {
            // Get the target of each type.
            auto data = getContentType(m_mime_to_format, target);
            if (!data) {
                return std::unexpected<ClipboardError>(data.error());
            }
            result.set_data(target, std::move(data.value().get_data(target).value()));
        }
        return result;
    }

    Result<void, ClipboardError> SystemClipboard::setContent(const MimeData &mimedata) {
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<void>("Failed to open the clipboard!");
        }

        if (!EmptyClipboard()) {
            return make_clipboard_error<void>("Failed to clear the clipboard!");
        }

        std::vector<std::string> formats = mimedata.get_all_formats();
        if (formats.size() > 1) {
            return make_clipboard_error<void>(
                "Can't set clipboard to mulitple types at once on Windows!");
        }
        if (formats.size() == 0) {
            return make_clipboard_error<void>("The mimedata set on the clipboard has no formats!");
        }
        auto type = formats[0];

        std::optional<Buffer> result = mimedata.get_data(type);
        if (!result) {
            return make_clipboard_error<void>(
                std::format("Failed to find MIME data type \"{}\"", type));
        }

        Buffer data_buf = std::move(result.value());

        UINT format = CF_NULL;
        if (m_mime_to_format.find(type) == m_mime_to_format.end()) {
            format = SystemClipboard::FormatForMime(type);
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
            dest_buf_size += sizeof(DROPFILES) + sizeof(wchar_t);
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

            path_list_ptr[data_buf.size()]     = L'\0';
            path_list_ptr[data_buf.size() + 1] = L'\0';
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
    // On windows we don't need to hook into GLFW for clipboard
    // functionality, so do nothing.
    void hookClipboardIntoGLFW(void) {}

#elif defined(TOOLBOX_PLATFORM_LINUX)
    // Storage for a function pointer to the original handler, so that
    // we can call it if we don't want to handle the event ourselves.
    void (*glfwSelectionRequestHandler)(XEvent *);
    // Sends a notification that we're done responding to a given
    // request.
    void sendRequestNotify(Display *dpy, const XSelectionRequestEvent *request) {
        XSelectionEvent response;
        response.type      = SelectionNotify;
        response.requestor = request->requestor;
        response.selection = request->selection;
        response.target    = request->target;
        response.property  = request->property;
        response.time      = request->time;

        XSendEvent(dpy, request->requestor, True, NoEventMask,
                   reinterpret_cast<XEvent *>(&response));
    }
    // Our hook into the GLFW selection event handling code. When we
    // receive a selection event, either return a list of formats we
    // support or return the data in our clipboard.
    void handleSelectionRequest(XEvent *event) {
        // Pull the contents of the clipboard from the clipboard
        // instance. This function can't be a method on that object
        // because we're going to be passing it as a function pointer
        // into GLFW, so we have to do everything externally.
        MimeData clipboard_contents = SystemClipboard::instance().m_clipboard_contents;
        // Get the display object from GLFW so we can intern our
        // strings in its namespace.
        Display *dpy = getGLFWDisplay();
        // Pull out the specific event from the general one.
        const XSelectionRequestEvent *request = &event->xselectionrequest;
        // Start our targets array with just the "TARGET" target,
        // since we'll generate the rest in a loop.
        Atom TARGETS = XInternAtom(dpy, "TARGETS", False);
        if (request->target == TARGETS) {
            // If they requested the targets...
            std::vector<Atom> targets = {TARGETS};
            // For each format the selection data supports, intern it into
            // an atom and add it to the targets array.
            for (auto &string_format : clipboard_contents.get_all_formats()) {
                Atom format_atom = XInternAtom(dpy, string_format.c_str(), False);
                targets.push_back(format_atom);
            }
            // Put this data as the requested property on the
            // requested window. XAtoms are 32 bits wide, so specify
            // that.
            XChangeProperty(
                dpy, request->requestor, request->property, XA_ATOM, 32, PropModeReplace,
                reinterpret_cast<const unsigned char *>(targets.data()), targets.size());
            // Send a notification that we did the thing.
            sendRequestNotify(dpy, request);
        } else {
            // If they request anything but TARGETS, assume its a data
            // format from the clipboard data. Get the string name of
            // the format, and ask for it from the clipboard contents
            // mime type.
            char *requested_target = XGetAtomName(dpy, request->target);
            auto data              = clipboard_contents.get_data(requested_target);
            // If the mime data recognized and gave us a response, and
            // also we have a property to put the response on...
            if (data && request->property != None) {
                // Then set the property, and notify of a response.
                XChangeProperty(dpy, request->requestor, request->property, request->target,
                                8 /* is this ever wrong? */, PropModeReplace,
                                reinterpret_cast<const unsigned char *>(data.value().buf()),
                                data.value().size());
                sendRequestNotify(dpy, request);
            } else {
                // If we don't recognize the data, or it's a legacy
                // client without a property target, then let GLFW
                // complain about it.
                glfwSelectionRequestHandler(event);
            }
            // Make sure to free the string that represents the
            // format.
            XFree(requested_target);
        }
    }
    // Called on initialization to hook our code into the GLFW
    // selection code.
    void hookClipboardIntoGLFW(void) {
        // Get the old handler so that we can invoke it when our
        // handler doesn't know what to do.
        glfwSelectionRequestHandler = getSelectionRequestHandler();
        // Set our handler as the new handler.
        setSelectionRequestHandler(handleSelectionRequest);
    }
    // Requests that the selection owner send us the data in the
    // `target` format, by putting it on the `target_property`
    // property of the `target_window` window.
    void requestTarget(std::string target, Window target_window, Atom target_property) {
        Display *dpy          = getGLFWDisplay();
        Atom requested_target = XInternAtom(dpy, target.c_str(), False);
        Atom sel              = XInternAtom(dpy, "CLIPBOARD", False);
        XConvertSelection(dpy, sel, requested_target, target_property, target_window, CurrentTime);
    }

    // Get all possible content types for the system clipboard.
    Result<std::vector<std::string>, ClipboardError>
    SystemClipboard::getAvailableContentFormats() const {
        Display *dpy         = getGLFWDisplay();
        Window target_window = getGLFWHelperWindow();
        Atom target_property = XInternAtom(dpy, "CLIP", False);
        // Request that the "TARGETS" info be sent to the CLIP
        // property on the glfw helper window.
        requestTarget("TARGETS", target_window, target_property);

        // Keep waiting for X events until we get a SelectionNotify
        XEvent ev;
        while (ev.type != SelectionNotify) {
            XNextEvent(dpy, &ev);
        }
        XSelectionEvent *sev = &ev.xselection;
        // We can only copy from clients that give us a property to
        // put the data on.
        if (sev->property == None) {
            return make_clipboard_error<std::vector<std::string>>(
                "Conversion could not be performed.\n");
        }
        // Get the value on the property that the source told us.
        std::vector<std::string> types;
        unsigned long nitems;
        unsigned char *prop_ret = nullptr;
        // We won't use these.
        Atom _type;
        int _di;
        unsigned long _dul;
        XGetWindowProperty(dpy, target_window, target_property, 0, 1024 * sizeof(Atom), False,
                           XA_ATOM, &_type, &_di, &nitems, &_dul, &prop_ret);

        // Iterate through the returned atoms and convert them
        // into strings to return.
        Atom *targets = (Atom *)prop_ret;
        for (int i = 0; i < nitems; ++i) {
            char *an = XGetAtomName(dpy, targets[i]);
            types.push_back(std::string(an));
            if (an)
                XFree(an);
        }
        // Free up the X11 resources we used.
        XFree(prop_ret);
        XDeleteProperty(dpy, target_window, target_property);
        return types;
    }

    // Get the bytes corresponding to a particular target. This is a
    // higher-level function than requestTarget, which only sends the
    // request and doesn't return anything.
    Result<Buffer, ClipboardError> getTarget(std::string target) {
        // Request the target from the clipboard owner.
        Display *dpy         = getGLFWDisplay();
        Window target_window = getGLFWHelperWindow();
        Atom target_property = XInternAtom(dpy, "CLIP", False);
        requestTarget(target, target_window, target_property);

        // Keep waiting for X events until we get a SelectionNotify
        XEvent ev;
        while (ev.type != SelectionNotify) {
            XNextEvent(dpy, &ev);
        }
        XSelectionEvent *sev = &ev.xselection;
        // Throw an error on old clients who don't tell you where they
        // put stuff.
        if (sev->property == None) {
            return make_clipboard_error<Buffer>("Conversion could not be performed.");
        }
        Atom type_received;
        unsigned long size;
        unsigned char *prop_ret = nullptr;
        // These are unused
        int _di;
        unsigned long _dul;
        Atom _da;

        // Use an initial X11 request to figure out how big the data is
        // and whether it's requesting multiple batches.
        XGetWindowProperty(dpy, target_window, target_property, 0, 0, False, AnyPropertyType,
                           &type_received, &_di, &_dul, &size, &prop_ret);
        // Don't use the return data, it's zero sized anyway.
        XFree(prop_ret);

        // If the data needs multiple batches, throw an error for now.
        Atom incr = XInternAtom(dpy, "INCR", False);
        if (type_received == incr) {
            return make_clipboard_error<Buffer>("Data over 256kb, this isn't supported yet");
        }
        // Request that X11 copy the property data to an internal
        // buffer and put a pointer to that buffer in prop_ret, using
        // the size we got earlier.
        XGetWindowProperty(dpy, target_window, target_property, 0, size, False, AnyPropertyType,
                           &_da, &_di, &_dul, &_dul, &prop_ret);
        // Allocate our own Buffer object and copy the data into
        // it. Make sure to null-terminate the data in case it's a
        // c string (which it often is).
        Buffer data_buffer;
        if (!data_buffer.alloc(size + 1)) {
            return make_clipboard_error<Buffer>("Couldn't allocate buffer of big enough size");
        }
        std::memcpy(data_buffer.buf(), prop_ret, size);
        data_buffer.buf<char>()[size] = '\0';
        // Free the X11 data buffer
        XFree(prop_ret);
        // Free the window property
        XDeleteProperty(dpy, target_window, target_property);
        return data_buffer;
    }
    // Assume the clipboard has plain text and return it as a
    // string. Returns a ClipboardError if the clipboard doesn't have
    // plain text.
    Result<std::string, ClipboardError> SystemClipboard::getText() const {
        // Request the plain text target from the clipboard owner.
        auto data = getTarget("text/plain");
        // If the clipboard doesn't have plain text, propogate the
        // error.
        if (!data) {
            return std::unexpected<ClipboardError>(data.error());
        }
        // Reinterpret the received buffer as a c-string, then wrap it
        // in a c++ string to return.
        return std::string(reinterpret_cast<char *>(data.value().buf()));
    }

    // Set the contents of the cilpboard to the given string. This
    // can't fail at this step on Linux, but we return Result to be
    // consistent with the Windows code.
    Result<void, ClipboardError> SystemClipboard::setText(const std::string &text) {
        MimeData data;
        data.set_text(text);
        return setContent(data);
    }
    // Assume the clipboard contains file paths and return them in a
    // vector. Returns a clipboardError if the clipboard doesn't have
    // files in it.
    Result<std::vector<fs_path>, ClipboardError> SystemClipboard::getFiles() const {
        // Get the file list target.
        auto data = getTarget("text/uri-list");
        // If the clipboard doesn't have a file list, propogate the
        // error.
        if (!data) {
            return std::unexpected<ClipboardError>(data.error());
        }
        // Reinterpret the received buffer as a c-string, then wrap it
        // in a c++ string.
        std::string data_string(data.value().buf<char>());
        // Split the string into lines, one for each path.
        auto lines = String::splitLines(data_string);
        // Allocate a vector of paths, and convert the vector of
        // strings to it. When doing so, strip off the "file://"
        // prefix.
        std::vector<fs_path> result((int)lines.size());
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines[i].starts_with("file:///")) {
                TOOLBOX_ERROR_V("Can't copy non-local uri \"{}\"", lines[i]);
            }
            result[i] = lines[i].substr(7);
        }
        return result;
    }

    // Get the contents of the system clipboard.
    Result<MimeData, ClipboardError> SystemClipboard::getContent() const {
        // Figure out all possible formats the content can be in.
        auto formats = getAvailableContentFormats();
        // If that didn't work, propogate the error.
        if (!formats) {
            return std::unexpected<ClipboardError>(formats.error());
        }
        // For each possible data type...
        MimeData result;
        for (std::string &target : formats.value()) {
            // Skip targets that aren't MIME types. These will pretty
            // much always be redundant with a mime-type target
            // (except TARGETS, which we don't want to do here anyway.
            if (!MimeData::isMimeTarget(target))
                continue;
            // Get the target of each type.
            auto data = getTarget(target);
            if (!data) {
                return std::unexpected<ClipboardError>(data.error());
            }
            // Put that into the returned mime data indexed by the
            // type.
            result.set_data(target, std::move(data.value()));
        }
        return result;
    }

    // Set the contents of the system clipboard.
    Result<void, ClipboardError> SystemClipboard::setContent(const MimeData &content) {
        // Get the display and helper window from GLFW using our custom hooks.
        Display *dpy         = getGLFWDisplay();
        Window target_window = getGLFWHelperWindow();
        // Copy the content into the contents member variable.
        m_clipboard_contents = content;
        // Set ourselves as the owner of the clipboard. We'll do all
        // the actual data transfer once someone asks us for data, in
        // our event handling hook.
        Atom CLIPBOARD = XInternAtom(dpy, "CLIPBOARD", False);
        XSetSelectionOwner(dpy, CLIPBOARD, target_window, CurrentTime);
        return {};
    }
#endif

}  // namespace Toolbox
