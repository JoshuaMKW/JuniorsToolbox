#pragma once

#include "tristate.hpp"
#include <string>
#include <vector>

namespace Toolbox::UI {

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
            m_target_state.second = TriState::TRUE;
        }

        [[nodiscard]] bool rejectTarget(std::string_view target) {
            if (m_target_state.first != target) {
                return false;
            }
            m_target_state.second = TriState::FALSE;
        }

        void setTarget(std::string_view target) {
            m_target_state = {std::string(target), TriState::INDETERMINATE};
        }

        void setData(void *data) {
            if (m_data) {
                delete m_data;
            }
            m_data = data;
        }

    private:
        std::pair<std::string, TriState> m_target_state;
        void *m_data;
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
            m_target_state.second = TriState::TRUE;
        }

        [[nodiscard]] bool rejectTarget(std::string_view target) {
            if (m_target_state.first != target) {
                return false;
            }
            m_target_state.second = TriState::FALSE;
        }

        void setTarget(std::string_view target) {
            m_target_state = {std::string(target), TriState::INDETERMINATE};
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
        std::pair<std::string, TriState> m_target_state;
        std::vector<_DataT> m_data;
    };

}  // namespace Toolbox::UI