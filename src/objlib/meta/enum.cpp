#include "objlib/meta/enum.hpp"
#include "objlib/meta/value.hpp"
#include <iostream>
#include <string>

namespace Toolbox::Object {

    void MetaEnum::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        indention_width          = std::min(indention_width, size_t(8));
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        out << self_indent << "enum " << m_name << "<" << meta_type_name(m_type) << "> {\n";
        for (const auto &v : m_values) {
            out << value_indent << v.first << " = " << v.second.toString() << ",\n";
        }
        out << self_indent << "}\n";
    }

}  // namespace Toolbox::Object