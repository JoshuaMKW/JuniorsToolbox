#pragma once

#include "core/types.hpp"
#include "instructions/forms.hpp"

using namespace Toolbox;

namespace Toolbox::Interpreter::Register {

    struct PC {
        u64 m_address;
    };

    struct GPR {
        u64 m_value;
    };

    struct XER {
        u32 m_reserved_0  : 32;
        bool m_so         : 1;
        bool m_ov         : 1;
        bool m_ca         : 1;
        u32 m_reserved_35 : 22;
        u32 m_str_size    : 7;
    };

    enum class Cmp {
        NONE = 0,
        EQ   = (1 << 0),
        GT   = (1 << 1),
        LT   = (1 << 2),
        SO   = (1 << 3),
    };
    TOOLBOX_BITWISE_ENUM(Cmp)

    struct CR {
        Cmp m_cr0 : 4;
        Cmp m_cr1 : 4;
        Cmp m_cr2 : 4;
        Cmp m_cr3 : 4;
        Cmp m_cr4 : 4;
        Cmp m_cr5 : 4;
        Cmp m_cr6 : 4;
        Cmp m_cr7 : 4;

        void cmp(s32 ra, s32 rb, const XER &xer) {
            if (ra < rb) {
                m_cr0 = Cmp::LT | (xer.m_so ? Cmp::SO : Cmp::NONE);
                return;
            }

            if (ra > rb) {
                m_cr0 = Cmp::GT | (xer.m_so ? Cmp::SO : Cmp::NONE);
                return;
            }

            m_cr0 = Cmp::EQ | (xer.m_so ? Cmp::SO : Cmp::NONE);
            return;
        }

        void cmp(f32 fa, f32 fb, const XER &xer) {
            if (fa < fb) {
                m_cr0 = Cmp::LT | (xer.m_so ? Cmp::SO : Cmp::NONE);
                return;
            }

            if (fa > fb) {
                m_cr0 = Cmp::GT | (xer.m_so ? Cmp::SO : Cmp::NONE);
                return;
            }

            m_cr0 = Cmp::EQ | (xer.m_so ? Cmp::SO : Cmp::NONE);
            return;
        }

        void cmp(f64 fa, f64 fb, const XER &xer) {
            if (fa < fb) {
                m_cr0 = Cmp::LT | (xer.m_so ? Cmp::SO : Cmp::NONE);
                return;
            }

            if (fa > fb) {
                m_cr0 = Cmp::GT | (xer.m_so ? Cmp::SO : Cmp::NONE);
                return;
            }

            m_cr0 = Cmp::EQ | (xer.m_so ? Cmp::SO : Cmp::NONE);
            return;
        }
    };

    struct LR {
        u64 m_target_address;
    };

    struct CTR {
        union {
            s64 m_count;
            u64 m_target_address;
        } m_0;
    };

}  // namespace Toolbox::Interpreter::Register