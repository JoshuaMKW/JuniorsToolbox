#pragma once

#include <expected>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/mimedata/mimedata.hpp"
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
        SystemClipboard(SystemClipboard &&) = delete;

    public:
        static SystemClipboard &instance() {
            static SystemClipboard s_clipboard;
            return s_clipboard;
        }

        Result<std::string, ClipboardError> getText();
        Result<void, ClipboardError> setText(const std::string &text);
        Result<std::vector<std::string>, ClipboardError> possibleContentTypes();
        Result<MimeData, ClipboardError> getContent(const std::string &type);
        Result<void, ClipboardError> setContent(const MimeData &content);
    };

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

}  // namespace Toolbox::UI