#pragma once

#include "nameref.hpp"
#include <initializer_list>
#include <string>
#include <vector>

namespace Toolbox::Object {

    class QualifiedName {
    public:
        using iterator               = std::vector<std::string>::iterator;
        using const_iterator         = std::vector<std::string>::const_iterator;
        using reverse_iterator       = std::vector<std::string>::reverse_iterator;
        using const_reverse_iterator = std::vector<std::string>::const_reverse_iterator;

    protected:
        constexpr QualifiedName() = default;

    public:
        constexpr QualifiedName(const std::string &name) : m_scopes({name}) {}
        constexpr QualifiedName(std::string_view name) : m_scopes({std::string(name)}) {}
        QualifiedName(const std::string &name, const QualifiedName &parent)
            : m_scopes(parent.m_scopes) {
            m_scopes.push_back(name);
        }
        QualifiedName(std::string_view name, const QualifiedName &parent)
            : m_scopes(parent.m_scopes) {
            m_scopes.push_back(std::string(name));
        }
        constexpr QualifiedName(const std::vector<std::string> &scopes) : m_scopes(scopes) {}
        constexpr QualifiedName(const_iterator begin, const_iterator end) : m_scopes(begin, end) {}
        constexpr QualifiedName(const_reverse_iterator begin, const_reverse_iterator end)
            : m_scopes(begin, end) {}
        constexpr QualifiedName(std::initializer_list<std::string> scopes) : m_scopes(scopes) {}
        constexpr QualifiedName(const QualifiedName &other) = default;
        constexpr QualifiedName(QualifiedName &&other)      = default;
        constexpr ~QualifiedName()                          = default;

        constexpr QualifiedName &operator=(const QualifiedName &other) = default;
        constexpr QualifiedName &operator=(QualifiedName &&other)      = default;
        constexpr QualifiedName &operator=(const std::string &name) {
            return *this = QualifiedName(name);
        }
        constexpr QualifiedName &operator=(const std::vector<std::string> &scopes) {
            return *this = QualifiedName(scopes);
        }
        constexpr QualifiedName &operator=(std::initializer_list<std::string> scopes) {
            return *this = QualifiedName(scopes);
        }

        [[nodiscard]] constexpr bool operator==(const QualifiedName &other) const {
            return m_scopes == other.m_scopes;
        }
        [[nodiscard]] constexpr bool operator!=(const QualifiedName &other) const {
            return m_scopes != other.m_scopes;
        }

        [[nodiscard]] constexpr std::string_view operator[](size_t index) const {
            return m_scopes[index];
        }

        [[nodiscard]] constexpr std::string &operator[](size_t index) { return m_scopes[index]; }

        [[nodiscard]] constexpr bool empty() const { return m_scopes.empty(); }

        [[nodiscard]] constexpr std::string name() const { return m_scopes.back(); }
        [[nodiscard]] constexpr size_t depth() const { return m_scopes.size(); }

        [[nodiscard]] constexpr QualifiedName parent() const {
            if (m_scopes.size() == 1) {
                return {};
            }
            return QualifiedName(m_scopes.begin(), m_scopes.end() - 1);
        }

        [[nodiscard]] constexpr bool isParentOf(const QualifiedName &other) const {
            if (m_scopes.size() >= other.m_scopes.size()) {
                return false;
            }
            for (size_t i = 0; i < m_scopes.size(); ++i) {
                if (m_scopes[i] != other.m_scopes[i]) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr std::string toString() const {
            std::string result;
            for (const auto &scope : m_scopes) {
                result += scope + "::";
            }
            return result.substr(0, result.size() - 2);
        }

        [[nodiscard]] constexpr std::string toString(const std::string &separator) const {
            std::string result;
            for (const auto &scope : m_scopes) {
                result += scope + separator;
            }
            return result.substr(0, result.size() - separator.size());
        }

        [[nodiscard]] constexpr size_t getAbsIndexOf(size_t scopeidx, int index,
                                                     size_t sep_size) const {
            if (scopeidx >= m_scopes.size()) {
                return 0;
            }

            size_t result = 0;
            for (size_t i = 0; i < scopeidx; ++i) {
                result += m_scopes[i].size() + sep_size;
            }

            return result + index;
        }
        [[nodiscard]] constexpr size_t getAbsIndexOf(size_t scopeidx, int index) const {
            return getAbsIndexOf(scopeidx, index, 2);
        }

        [[nodiscard]] constexpr iterator begin() { return m_scopes.begin(); }
        [[nodiscard]] constexpr const_iterator begin() const { return m_scopes.begin(); }

        [[nodiscard]] constexpr iterator end() { return m_scopes.end(); }
        [[nodiscard]] constexpr const_iterator end() const { return m_scopes.end(); }

        [[nodiscard]] constexpr reverse_iterator rbegin() { return m_scopes.rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const { return m_scopes.rbegin(); }

        [[nodiscard]] constexpr reverse_iterator rend() { return m_scopes.rend(); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const { return m_scopes.rend(); }

    private:
        std::vector<std::string> m_scopes;
    };

}  // namespace Toolbox::Object