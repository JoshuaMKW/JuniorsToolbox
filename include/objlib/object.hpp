#pragma once

#include "nameref.hpp"
#include "types.hpp"
#include <string>
#include <vector>

namespace Toolbox::Object {

    struct ISceneObject {
        virtual ~ISceneObject() = default;

        [[nodiscard]] virtual bool isGroupObject() const = 0;

        virtual std::string type() const   = 0;

        virtual NameRef getNameRef() const = 0;

        virtual std::vector<s8> getData() const       = 0;
        virtual std::vector<s8> getMemberData() const = 0;
        virtual std::size_t getMemberOffset() const   = 0;


        virtual void dump(std::ostream &out) const = 0;
    };

    struct ObjectGroupError {};

}  // namespace Toolbox::Object