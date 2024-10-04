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
    // Storage for a function pointer to the original handler, so that
    // we can call it if we don't want to handle the event ourselves.
    void (*glfwSelectionRequestHandler)(XEvent *);
    // Sends a notification that we're done responding to a given
    // request.
    void sendRequestNotify(Display* dpy, const XSelectionRequestEvent *request) {
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
        MimeData clipboard_contents           = SystemClipboard::instance().m_clipboard_contents;
        // Get the display object from GLFW so we can intern our
        // strings in its namespace.
        Display *dpy                          = getGLFWDisplay();
        // Pull out the specific event from the general one.
        const XSelectionRequestEvent *request = &event->xselectionrequest;
        // Start our targets array with just the "TARGET" target,
        // since we'll generate the rest in a loop.
        Atom TARGETS                          = XInternAtom(dpy, "TARGETS", False);
        // If they
        if (request->target == TARGETS) {
            std::vector<Atom> targets             = {TARGETS};
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
                                reinterpret_cast<const unsigned char *>(data.value().buf()), data.value().size());
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
#else
    // On windows we don't need to hook into GLFW for clipboard
    // functionality, so do nothing.
    void hookClipboardIntoGLFW(void) {}
#endif

    // Get all possible content types for the system clipboard.
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
        // This unconditional while loop looks pretty dangerous but
        // this is how glfw does it, and how the examples online do
        // it, so :shrug:
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
#endif
        return {};
    }
    // Get the contents of the system clipboard.
    Result<MimeData, ClipboardError> SystemClipboard::getContent(const std::string &type) const {
#ifdef TOOLBOX_PLATFORM_LINUX
        Display *dpy           = getGLFWDisplay();
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
        // This unconditional while loop looks pretty dangerous but
        // this is how glfw does it, and how the examples online do
        // it, so :shrug:
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

    // Set the contents of the system clipboard.
    Result<void, ClipboardError> SystemClipboard::setContent(const MimeData &content) {
#ifdef TOOLBOX_PLATFORM_LINUX
        // Get the display and helper window from GLFW using our custom hooks.
        Display *dpy         = getGLFWDisplay();
        Window target_window = getGLFWHelperWindow();
        // Copy the content into the contents member variable.
        m_clipboard_contents = content;
        // Set ourselves as the owner of the clipboard. We'll do all
        // the actual data transfer once someone asks us for data, in
        // our event handling hook.
        Atom CLIPBOARD       = XInternAtom(dpy, "CLIPBOARD", False);
        XSetSelectionOwner(dpy, CLIPBOARD, target_window, CurrentTime);
#endif
        return {};
    }

}  // namespace Toolbox
