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

        QualifiedName() = delete;
        QualifiedName(const std::string &name) : m_scopes({name}) {}
        QualifiedName(const std::string &name, const QualifiedName &parent)
            : m_scopes(parent.m_scopes) {
            m_scopes.push_back(name);
        }
        QualifiedName(const std::vector<std::string> &scopes) : m_scopes(scopes) {}
        QualifiedName(std::initializer_list<std::string> scopes) : m_scopes(scopes) {}
        QualifiedName(const QualifiedName &other) = default;
        QualifiedName(QualifiedName &&other)      = default;
        ~QualifiedName()                          = default;

        QualifiedName &operator=(const QualifiedName &other) = default;
        QualifiedName &operator=(QualifiedName &&other)      = default;
        QualifiedName &operator=(const std::string &name) { return *this = QualifiedName(name); }
        QualifiedName &operator=(const std::vector<std::string> &scopes) {
            return *this = QualifiedName(scopes);
        }
        QualifiedName &operator=(std::initializer_list<std::string> scopes) {
            return *this = QualifiedName(scopes);
        }

        bool operator==(const QualifiedName &other) const { return m_scopes == other.m_scopes; }
        bool operator!=(const QualifiedName &other) const { return m_scopes != other.m_scopes; }

        std::string toString() const {
            std::string result;
            for (const auto &scope : m_scopes) {
                result += scope + "::";
            }
            return result.substr(0, result.size() - 2);
        }

        std::string toString(const std::string &separator) const {
            std::string result;
            for (const auto &scope : m_scopes) {
                result += scope + separator;
            }
            return result.substr(0, result.size() - separator.size());
        }

        std::string name() const { return m_scopes.back(); }
        size_t depth() const { return m_scopes.size(); }

        QualifiedName parent() const {
            if (m_scopes.size() == 1) {
                return {};
            }
            return QualifiedName(m_scopes.begin(), m_scopes.end() - 1);
        }

        bool isParentOf(const QualifiedName &other) const {
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

        iterator begin() { return m_scopes.begin(); }
        const_iterator begin() const { return m_scopes.begin(); }

        iterator end() { return m_scopes.end(); }
        const_iterator end() const { return m_scopes.end(); }

        reverse_iterator rbegin() { return m_scopes.rbegin(); }
        const_reverse_iterator rbegin() const { return m_scopes.rbegin(); }

        reverse_iterator rend() { return m_scopes.rend(); }
        const_reverse_iterator rend() const { return m_scopes.rend(); }

    private:
        std::vector<std::string> m_scopes;
    }
};

}  // namespace Toolbox::Object