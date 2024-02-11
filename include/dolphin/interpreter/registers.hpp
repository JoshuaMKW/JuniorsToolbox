#pragma once

#include <cmath>

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

    enum class FPRState {
        ZERO,
        NORMALIZED,
        DENORMALIZED,
        INFINITE,
        NANS,
    };

    struct FPR {
        union {
            union {
                bool m_sign    : 1;
                u8 m_exp_bias  : 8;
                u32 m_fraction : 23;
            } ieee_32;
            union {
                bool m_sign    : 1;
                u8 m_exp_bias  : 11;
                u64 m_fraction : 52;
            } ieee_64;
            f32 float_32;
            f64 float_64;
        } m_value;

        float evalf(FPRState *state = nullptr) const {
            FPRState st = evalStatef();
            if (state)
                *state = st;
            switch (st) {
            case FPRState::ZERO:
                return 0.0f;
            case FPRState::INFINITE: {
                float sign = m_value.ieee_32.m_sign ? -1.0f : 1.0f;
                return sign * INFINITY;
            }
            case FPRState::NANS:
                return NAN;
            case FPRState::NORMALIZED:
            case FPRState::DENORMALIZED: {
                float sign        = m_value.ieee_32.m_sign ? -1.0f : 1.0f;
                int exponent      = static_cast<int>(m_value.ieee_32.m_exp_bias) - 127;
                float significand = 1.0f + (m_value.ieee_32.m_fraction / 8388608.0f);
                return sign * std::pow(2, exponent) * significand;
            }
            }
        }

        double evald(FPRState *state = nullptr) const {
            FPRState st = evalStated();
            if (state)
                *state = st;
            switch (st) {
            case FPRState::ZERO:
                return 0.0f;
            case FPRState::INFINITE: {
                double sign = m_value.ieee_64.m_sign ? -1.0 : 1.0;
                return sign * INFINITY;
            }
            case FPRState::NANS:
                return NAN;
            case FPRState::NORMALIZED:
            case FPRState::DENORMALIZED: {
                double sign        = m_value.ieee_64.m_sign ? -1.0 : 1.0;
                int exponent       = static_cast<int>(m_value.ieee_64.m_exp_bias) - 1023;
                double significand = 1.0 + (m_value.ieee_64.m_fraction / 4503599627370496.0);
                return sign * std::pow(2, exponent) * significand;
            }
            }
        }

    private:
        FPRState evalStatef() const {
            if (m_value.ieee_32.m_exp_bias == 0) {
                if (m_value.ieee_32.m_fraction == 0) {
                    return FPRState::ZERO;
                }
                return FPRState::DENORMALIZED;
            }

            if (m_value.ieee_32.m_exp_bias == 255) {
                if (m_value.ieee_32.m_fraction == 0) {
                    return FPRState::INFINITE;
                }
                return FPRState::NANS;
            }

            return FPRState::NORMALIZED;
        }

        FPRState evalStated() const {
            if (m_value.ieee_64.m_exp_bias == 0) {
                if (m_value.ieee_64.m_fraction == 0) {
                    return FPRState::ZERO;
                }
                return FPRState::DENORMALIZED;
            }

            if (m_value.ieee_64.m_exp_bias == 2047) {
                if (m_value.ieee_64.m_fraction == 0) {
                    return FPRState::INFINITE;
                }
                return FPRState::NANS;
            }

            return FPRState::NORMALIZED;
        }
    };

    enum class FPSCRCmp : u8 {
        NONE = 0,
        FE   = (1 << 1),
        FG   = (1 << 2),
        FL   = (1 << 3),
        FU   = (1 << 0),
    };
    TOOLBOX_BITWISE_ENUM(FPSCRCmp)

    enum class FPSCRRound : u8 {
        NEAREST,
        ZERO,
        PINFINITY,
        NINFINITY,
    };

    struct FPSCR {
        bool m_fx          : 1;
        bool m_fex         : 1;
        bool m_vx          : 1;
        bool m_ox          : 1;
        bool m_ux          : 1;
        bool m_zx          : 1;
        bool m_xx          : 1;
        bool m_vxsnan      : 1;
        bool m_vxisi       : 1;
        bool m_vxidi       : 1;
        bool m_vxzdz       : 1;
        bool m_vximz       : 1;
        bool m_vxvc        : 1;
        bool m_fr          : 1;
        bool m_fi          : 1;
        bool m_c           : 1;
        FPSCRCmp m_fpcc    : 4;
        bool m_reserved_20 : 1;
        bool m_vxsoft      : 1;
        bool m_vxsqrt      : 1;
        bool m_vxcvi       : 1;
        bool m_ve          : 1;
        bool m_oe          : 1;
        bool m_ue          : 1;
        bool m_ze          : 1;
        bool m_xe          : 1;
        bool m_ni          : 1;
        FPSCRRound m_rn    : 2;
    };

    struct XER {
        u32 m_reserved_0  : 32;
        bool m_so         : 1;
        bool m_ov         : 1;
        bool m_ca         : 1;
        u32 m_reserved_35 : 22;
        u32 m_str_size    : 7;
    };

    enum class CRCmp : u8 {
        NONE = 0,
        EQ   = (1 << 1),
        GT   = (1 << 2),
        LT   = (1 << 3),
        SO   = (1 << 0),
    };
    TOOLBOX_BITWISE_ENUM(CRCmp)

    struct CR {
        CRCmp m_cr0 : 4;
        CRCmp m_cr1 : 4;
        CRCmp m_cr2 : 4;
        CRCmp m_cr3 : 4;
        CRCmp m_cr4 : 4;
        CRCmp m_cr5 : 4;
        CRCmp m_cr6 : 4;
        CRCmp m_cr7 : 4;

        void cmp(s32 ra, s32 rb, const XER &xer) {
            if (ra < rb) {
                m_cr0 = CRCmp::LT | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
                return;
            }

            if (ra > rb) {
                m_cr0 = CRCmp::GT | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
                return;
            }

            m_cr0 = CRCmp::EQ | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
            return;
        }

        void cmp(f32 fa, f32 fb, const XER &xer) {
            if (fa < fb) {
                m_cr0 = CRCmp::LT | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
                return;
            }

            if (fa > fb) {
                m_cr0 = CRCmp::GT | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
                return;
            }

            m_cr0 = CRCmp::EQ | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
            return;
        }

        void cmp(f64 fa, f64 fb, const XER &xer) {
            if (fa < fb) {
                m_cr0 = CRCmp::LT | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
                return;
            }

            if (fa > fb) {
                m_cr0 = CRCmp::GT | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
                return;
            }

            m_cr0 = CRCmp::EQ | (xer.m_so ? CRCmp::SO : CRCmp::NONE);
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