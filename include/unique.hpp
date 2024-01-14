#pragma once

#include "types.hpp"

class IUnique {
public:
    virtual [[nodiscard]] u64 getID() const = 0;
    virtual [[nodiscard]] u64 getSiblingID() const = 0;

    virtual void setID(u64 id) = 0;
    virtual void setSiblingID(u64 id) = 0;
};