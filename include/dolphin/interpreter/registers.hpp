#pragma once

#include <cmath>

#include "core/types.hpp"
#include "instructions/forms.hpp"

using namespace Toolbox;

namespace Toolbox::Interpreter::Register {

    enum class SPRType : u32 {
        XER    = 1,
        LR     = 8,
        CTR    = 9,
        DSISR  = 18,
        DAR    = 19,
        DEC    = 22,
        SDR1   = 25,
        SRR0   = 26,
        SRR1   = 27,
        SPRG0  = 272,
        SPRG1  = 273,
        SPRG2  = 274,
        SPRG3  = 275,
        EAR    = 282,
        TBL    = 284,
        TBU    = 285,
        PVR    = 287,
        IBAT0U = 528,
        IBAT0L = 529,
        IBAT1U = 530,
        IBAT1L = 531,
        IBAT2U = 532,
        IBAT2L = 533,
        IBAT3U = 534,
        IBAT3L = 535,
        DBAT0U = 536,
        DBAT0L = 537,
        DBAT1U = 538,
        DBAT1L = 539,
        DBAT1U = 540,
        DBAT1L = 541,
        DBAT1U = 542,
        DBAT1L = 543,
        GQR0   = 912,
        GQR1   = 913,
        GQR2   = 914,
        GQR3   = 915,
        GQR4   = 916,
        GQR5   = 917,
        GQR6   = 918,
        GQR7   = 919,
        HID2   = 920,
        WPAR   = 921,
        DMA_U  = 922,
        DMA_L  = 923,
        ECID_U = 924,
        ECID_M = 925,
        ECID_L = 926,
        UMMCR0 = 936,
        UPMC1  = 937,
        UPMC2  = 938,
        USIA   = 939,
        UMMCR1 = 940,
        UPMC3  = 941,
        UPMC4  = 942,
        USDA   = 943,
        MMCR0  = 952,
        PMC1   = 953,
        PMC2   = 954,
        SIA    = 955,
        MMCR1  = 956,
        PMC3   = 957,
        PMC4   = 958,
        SDA    = 959,
        HID0   = 1008,
        HID1   = 1009,
        IABR   = 1010,
        HID4   = 1011,
        DABR   = 1013,
        L2CR   = 1017,
        ICTC   = 1019,
        THRM1  = 1020,
        THRM2  = 1021,
        THRM3  = 1022,
    };

    struct PC {
        u64 m_address;
    };

    struct TB {
        u64 m_value;
    };

    struct MSR {
        bool m_sf          : 1;
        u8 m_reserved_1    : 2;
        bool m_hv          : 1;
        u64 m_reserved_4   : 43;
        bool m_reserved_47 : 1;
        bool m_ee          : 1;
        bool m_pr          : 1;
        bool m_fp          : 1;
        bool m_me          : 1;
        bool m_fe0         : 1;
        bool m_se          : 1;
        bool m_be          : 1;
        bool m_fe1         : 1;
        u8 m_reserved_56   : 2;
        bool m_ir          : 1;
        bool m_dr          : 1;
        bool m_reserved_60 : 1;
        bool m_pmm         : 1;
        bool m_ri          : 1;
        bool m_le          : 1;
    };

    struct DAR {
        u64 m_value;
    };

    struct DSISR {
        u32 m_value;
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

        bool is(u8 field, CRCmp cmp) {
            switch (field) {
            case 0:
                return (bool)(m_cr0 & cmp);
            case 1:
                return (bool)(m_cr1 & cmp);
            case 2:
                return (bool)(m_cr2 & cmp);
            case 3:
                return (bool)(m_cr3 & cmp);
            case 4:
                return (bool)(m_cr4 & cmp);
            case 5:
                return (bool)(m_cr5 & cmp);
            case 6:
                return (bool)(m_cr6 & cmp);
            case 7:
                return (bool)(m_cr7 & cmp);
            default:
                return false;
            }
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

    struct RegisterSnapshot {
        GPR m_gpr[32];
        FPR m_fpr[32];
        CR m_cr;
        LR m_lr;
        CTR m_ctr;
        XER m_xer;
        FPSCR m_fpscr;
        MSR m_msr;
        TB m_tb;
        DAR m_dar;
        DSISR m_dsisr;
    };

}  // namespace Toolbox::Interpreter::Register