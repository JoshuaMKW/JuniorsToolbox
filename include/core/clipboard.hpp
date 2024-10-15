#pragma once

#include <expected>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/mimedata/mimedata.hpp"
#include "fsystem.hpp"
#include "tristate.hpp"

namespace Toolbox {

    struct ClipboardError : public BaseError {};

    template <typename _Ret>
    inline Result<_Ret, ClipboardError> make_clipboard_error(std::string_view reason) {
        if (reason.empty()) {
            reason = "Unknown error occured";
        }
        ClipboardError err = {std::vector<std::string>({std::format("ClipboardError: {}", reason)}),
                              std::stacktrace::current()};
        return std::unexpected<ClipboardError>(err);
    }

    class SystemClipboard {
    protected:
        SystemClipboard();
        ~SystemClipboard();

        SystemClipboard(const SystemClipboard &) = delete;
        SystemClipboard(SystemClipboard &&)      = delete;

    public:
        static SystemClipboard &instance() {
            static SystemClipboard s_clipboard;
            return s_clipboard;
        }

        Result<std::vector<std::string>, ClipboardError> getAvailableContentFormats() const;

        Result<std::string, ClipboardError> getText() const;
        Result<void, ClipboardError> setText(const std::string &text);
        Result<MimeData, ClipboardError> getContent() const;
        Result<std::vector<fs_path>, ClipboardError> getFiles() const;
        Result<void, ClipboardError> setContent(const MimeData &mimedata);

#ifdef TOOLBOX_PLATFORM_WINDOWS
        friend Result<MimeData, ClipboardError>
        getContentType(std::unordered_map<std::string, UINT> &mime_to_format,
                       const std::string &type);

    protected:
        static UINT FormatForMime(std::string_view mimetype);
        static std::string MimeForFormat(UINT format);

    private:
        mutable std::unordered_map<std::string, UINT> m_mime_to_format;
#elif defined(TOOLBOX_PLATFORM_LINUX)
        // The reason this isn't private/protected is actually kind of
        // dumb. It needs to be accessed by handleSelectionRequest,
        // but we can't put the signature for handleSelectionRequest
        // in this header file so it can't be a method or friend
        // function. The reason we can't put it in this header file is
        // because it's argument type is XEvent*, which is defined in
        // X11/Xlib.h. But we can't include X11/Xlib.h in this header,
        // because that would make it recursively included in every
        // file that includes this header. And because it's a C
        // library, it doesn't have namespaces, and it defines a bunch
        // of symbols with very general names which clash with a bunch
        // of names we like to use in other places.
        MimeData m_clipboard_contents;
#endif
    };
    Result<MimeData, ClipboardError>
    getContentType(std::unordered_map<std::string, UINT> &mime_to_format, const std::string &type);
    void hookClipboardIntoGLFW(void);

    class DataClipboard {
    public:
        DataClipboard()  = default;
        ~DataClipboard() = default;

        static DataClipboard &instance() {
            static DataClipboard _instance;
            return _instance;
        }

        void clear() {
            delete m_data;
            m_data = nullptr;
        }

        bool hasData() const { return m_data != nullptr; }
        [[nodiscard]] void *getData() const { return m_data; }

        [[nodiscard]] bool acceptTarget(std::string_view target) {
            if (m_target_state.first != target) {
                return false;
            }
            m_target_state.second = TriState::TS_TRUE;
        }

        [[nodiscard]] bool rejectTarget(std::string_view target) {
            if (m_target_state.first != target) {
                return false;
            }
            m_target_state.second = TriState::TS_FALSE;
        }

        void setTarget(std::string_view target) {
            m_target_state = {std::string(target), TriState::TS_INDETERMINATE};
        }

        void setData(void *data) {
            if (m_data) {
                delete m_data;
            }
            m_data = data;
        }

    private:
        std::pair<std::string, TriState> m_target_state = {};
        void *m_data                                    = nullptr;
    };

    template <typename _DataT> class TypedDataClipboard {
    public:
        TypedDataClipboard()  = default;
        ~TypedDataClipboard() = default;

        void clear() { m_data.clear(); }

        bool hasData() const { return !m_data.empty(); }
        [[nodiscard]] std::vector<_DataT> getData() const { return m_data; }

        [[nodiscard]] bool acceptTarget(std::string_view target) {
            if (m_target_state.first != target) {
                return false;
            }
            m_target_state.second = TriState::TS_TRUE;
        }

        [[nodiscard]] bool rejectTarget(std::string_view target) {
            if (m_target_state.first != target) {
                return false;
            }
            m_target_state.second = TriState::TS_FALSE;
        }

        void setTarget(std::string_view target) {
            m_target_state = {std::string(target), TriState::TS_INDETERMINATE};
        }

        void setData(const _DataT &data) { m_data = {data}; }
        void setData(_DataT &&data) { m_data = {data}; }
        void setData(const std::vector<_DataT> &data) { m_data = data; }
        void setData(std::vector<_DataT> &&data) { m_data = data; }

        void appendData(const _DataT &data) { m_data.push_back(data); }
        void appendData(_DataT &&data) { m_data.push_back(data); }
        void appendData(const std::vector<_DataT> &data) {
            m_data.insert(m_data.end(), data.begin(), data.end());
        }
        void appendData(std::vector<_DataT> &&data) {
            m_data.insert(m_data.end(), data.begin(), data.end());
        }

    private:
        std::pair<std::string, TriState> m_target_state = {};
        std::vector<_DataT> m_data                      = {};
    };

}  // namespace Toolbox
