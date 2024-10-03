#include "core/clipboard.hpp"

#ifdef WIN32
#include <Windows.h>
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <GLFW/glfw3.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <imgui/imgui.h>

#endif

namespace Toolbox {

    SystemClipboard::SystemClipboard() {}
    SystemClipboard::~SystemClipboard() {}

    Result<std::string, ClipboardError> SystemClipboard::getText() {
#ifdef WIN32
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<std::string>("Failed to open the clipboard!");
        }

        HANDLE clipboard_data = GetClipboardData(CF_TEXT);
        if (!clipboard_data) {
            return make_clipboard_error<std::string>("Failed to retrieve the data handle!");
        }

        const char *text_data = static_cast<const char *>(GlobalLock(clipboard_data));
        if (!text_data) {
            return make_clipboard_error<std::string>("Failed to retrieve the buffer from the handle!");
        }

        std::string result = text_data;

        if (!GlobalUnlock(clipboard_data)) {
            return make_clipboard_error<std::string>("Failed to unlock the data handle!");
        }

        if (!CloseClipboard()) {
            return make_clipboard_error<std::string>("Failed to close the clipboard!");
        }

        return result;
#else
#endif
    }
    Result<void, ClipboardError> SystemClipboard::setText(const std::string &text) {
#ifdef WIN32
        if (!OpenClipboard(nullptr)) {
            return make_clipboard_error<void>("Failed to open the clipboard!");
        }

        if (!EmptyClipboard()) {
            return make_clipboard_error<void>("Failed to clear the clipboard!");
        }

        HANDLE data_handle = GlobalAlloc(GMEM_DDESHARE, text.size()+1);
        if (!data_handle) {
            return make_clipboard_error<void>("Failed to alloc new data handle!");
        }

        char *data_buffer = static_cast<char *>(GlobalLock(data_handle));
        if (!data_buffer) {
            return make_clipboard_error<void>("Failed to retrieve the buffer from the handle!");
        }

        strncpy_s(data_buffer, text.size()+1, text.c_str(), text.size()+1);

        if (!GlobalUnlock(data_handle)) {
            return make_clipboard_error<void>("Failed to unlock the data handle!");
        }

        SetClipboardData(CF_TEXT, data_handle);

        if (!CloseClipboard()) {
            return make_clipboard_error<void>("Failed to close the clipboard!");
        }
#else
#endif
        return {};
    }

#ifdef TOOLBOX_PLATFORM_LINUX
    void (*glfwSelectionRequestHandler)(XEvent *);
    void sendRequestNotify(XSelecctionRequestEvent *request) {
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
    void handleSelectionRequest(XEvent *event) {
        MimeData clipboard_contents = SystemClipboard::instance().m_clipboard_contents;
        const XSelectionRequestEvent *request = &event->xselectionrequest;
        Display *dpy                          = getGLFWDisplay();
        Atom TARGETS                          = XInternAtom(dpy, "TARGETS", False);
        std::vector<Atom> targets             = {TARGETS};
        for (auto &string_format : clipboardContents.get_all_formats()) {
            Atom format_atom = XInternAtom(dpy, format.c_str(), False);
            targets.push_back(format_atom);
        }
        if (request->target == TARGETS) {
            XChangeProperty(dpy, request->requestor, request->property, XA_ATOM, 32,
                            PropModeReplace, reinterpret_cast<const unsigned char *>(targets.data()),
                            targets.size());
            sendRequestNotify(request);
        } else {
            char* requested_target = XGetAtomName(dpy, request->target);
            auto data = clipboard_contents.get_data(requested_target);
            if (data && request->property != None) {
                XChangeProperty(dpy, request->requestor, request->property, request->target,
                                8 /* is this ever wrong? */, PropModeReplace,
                                reinterpret_cast<const unsigned char*>data.buf(),
                                data.size);
            } else {
                glfwSelectionRequestHandler(event);
            }
            sendRequestNotify(request);
            XFree(requested_target);
        }
    }
    void hookClipboardIntoGLFW(void) {
        glfwSelectionRequestHandler = getSelectionRequestHandler();
        setSelectionRequestHandler(handleSelectionRequest);
    }
#else
    void hookClipboardIntoGLFW(void) {}
#endif

    Result<std::vector<std::string>, ClipboardError> SystemClipboard::possibleContentTypes() const {
#ifdef TOOLBOX_PLATFORM_LINUX
        Display *dpy         = getGLFWDisplay();
        Atom sel             = XInternAtom(dpy, "CLIPBOARD", False);
        Atom targets         = XInternAtom(dpy, "TARGETS", False);
        Atom target_property = XInternAtom(dpy, "CLIP_TYPES", False);
        Window target_window = getGLFWHelperWindow();
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
        XCloseDisplay(dpy);
#endif
        return {};
    }
    Result<MimeData, ClipboardError> SystemClipboard::getContent(const std::string &type) const {
#ifdef TOOLBOX_PLATFORM_LINUX
        Display *dpy = getGLFWDisplay();
        Atom requested_type    = XInternAtom(dpy, type.c_str(), False);
        Atom sel               = XInternAtom(dpy, "CLIPBOARD", False);
        Window clipboard_owner = XGetSelectionOwner(dpy, sel);
        if (clipboard_owner == None) {
            return make_clipboard_error<MimeData>("Clipboard isn't owned by anyone");
        }
        Window target_window = getGLFWHelperWindow();
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
#endif
        return {};
    }

    Result<void, ClipboardError> SystemClipboard::setContent(const MimeData &content) {
#ifdef TOOLBOX_PLATFORM_LINUX
        Display* dpy = getGLFWDisplay();
        Window target_window = getGLFWHelperWindow();
        Atom CLIPBOARD        = XInternAtom(dpy, "CLIPBOARD", False);
        m_clipboard_contents = content;
        XSetSelectionOwner(dpy, CLIPBOARD, target_window, CurrentTime);
#endif
        return {};
    }

}  // namespace Toolbox
