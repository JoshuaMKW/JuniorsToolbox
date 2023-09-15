#pragma once

#include <memory>

namespace Toolbox {

    class Iclone {
    public:
        virtual ~Iclone() = default;

        virtual std::unique_ptr<Iclone> clone(bool deep) const    = 0;

        bool operator==(const Iclone &other) const { return true; }
    };

}  // namespace Toolbox