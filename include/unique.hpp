#pragma once

#include "types.hpp"

class IUnique {
public:
    virtual [[nodiscard]] u32 getID() const = 0;
    virtual [[nodiscard]] u32 getSiblingID() const = 0;

    virtual void setID(u32 id) = 0;
    virtual void setSiblingID(u32 id) = 0;
};

inline u32 uuid() {
    static u32 _uuid = 0;
    return _uuid++;
}