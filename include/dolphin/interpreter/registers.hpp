#pragma once

#include <cmath>

#include "core/types.hpp"
#include "instructions/forms.hpp"

using namespace Toolbox;

namespace Toolbox::Interpreter::Register {

    enum class SPRType : u32 {
        SPR_XER    = 1,
        SPR_LR     = 8,
        SPR_CTR    = 9,
        SPR_DSISR  = 18,
        SPR_DAR    = 19,
        SPR_DEC    = 22,
        SPR_SDR1   = 25,
        SPR_SRR0   = 26,
        SPR_SRR1   = 27,
        SPR_SPRG0  = 272,
        SPR_SPRG1  = 273,
        SPR_SPRG2  = 274,
        SPR_SPRG3  = 275,
        SPR_EAR    = 282,
        SPR_TBL    = 284,
        SPR_TBU    = 285,
        SPR_PVR    = 287,
        SPR_IBAT0U = 528,
        SPR_IBAT0L = 529,
        SPR_IBAT1U = 530,
        SPR_IBAT1L = 531,
        SPR_IBAT2U = 532,
        SPR_IBAT2L = 533,
        SPR_IBAT3U = 534,
        SPR_IBAT3L = 535,
        SPR_DBAT0U = 536,
        SPR_DBAT0L = 537,
        SPR_DBAT1U = 538,
        SPR_DBAT1L = 539,
        SPR_DBAT2U = 540,
        SPR_DBAT2L = 541,
        SPR_DBAT3U = 542,
        SPR_DBAT3L = 543,
        SPR_GQR0   = 912,
        SPR_GQR1   = 913,
        SPR_GQR2   = 914,
        SPR_GQR3   = 915,
        SPR_GQR4   = 916,
        SPR_GQR5   = 917,
        SPR_GQR6   = 918,
        SPR_GQR7   = 919,
        SPR_HID2   = 920,
        SPR_WPAR   = 921,
        SPR_DMA_U  = 922,
        SPR_DMA_L  = 923,
        SPR_ECID_U = 924,
        SPR_ECID_M = 925,
        SPR_ECID_L = 926,
        SPR_UMMCR0 = 936,
        SPR_UPMC1  = 937,
        SPR_UPMC2  = 938,
        SPR_USIA   = 939,
        SPR_UMMCR1 = 940,
        SPR_UPMC3  = 941,
        SPR_UPMC4  = 942,
        SPR_USDA   = 943,
        SPR_MMCR0  = 952,
        SPR_PMC1   = 953,
        SPR_PMC2   = 954,
        SPR_SIA    = 955,
        SPR_MMCR1  = 956,
        SPR_PMC3   = 957,
        SPR_PMC4   = 958,
        SPR_SDA    = 959,
        SPR_HID0   = 1008,
        SPR_HID1   = 1009,
        SPR_IABR   = 1010,
        SPR_HID4   = 1011,
        SPR_DABR   = 1013,
        SPR_L2CR   = 1017,
        SPR_ICTC   = 1019,
        SPR_THRM1  = 1020,
        SPR_THRM2  = 1021,
        SPR_THRM3  = 1022,
    };

    typedef u64 PC;

    typedef u64 TB;

    typedef u32 MSR;

    // clang-format off
#define MSR_EE(msr)  (((msr) >> 15) & 0b1)
#define MSR_PR(msr)  (((msr) >> 14) & 0b1)
#define MSR_FP(msr)  (((msr) >> 13) & 0b1)
#define MSR_ME(msr)  (((msr) >> 12) & 0b1)
#define MSR_FE0(msr) (((msr) >> 11) & 0b1)
#define MSR_SE(msr)  (((msr) >> 10) & 0b1)
#define MSR_BE(msr)  (((msr) >> 9) & 0b1)
#define MSR_FE1(msr) (((msr) >> 8) & 0b1)
#define MSR_IR(msr)  (((msr) >> 5) & 0b1)
#define MSR_DR(msr)  (((msr) >> 4) & 0b1)
#define MSR_PMM(msr) (((msr) >> 2) & 0b1)
#define MSR_RI(msr)  (((msr) >> 1) & 0b1)
#define MSR_LE(msr)  ((msr) & 0b1)

#define MSR_SET_EE(msr, value)  ((msr) = ((msr) & ~(0b1 << 15)) | (((value)&0b1) << 15))
#define MSR_SET_PR(msr, value)  ((msr) = ((msr) & ~(0b1 << 14)) | (((value)&0b1) << 14))
#define MSR_SET_FP(msr, value)  ((msr) = ((msr) & ~(0b1 << 13)) | (((value)&0b1) << 13))
#define MSR_SET_ME(msr, value)  ((msr) = ((msr) & ~(0b1 << 12)) | (((value)&0b1) << 12))
#define MSR_SET_FE0(msr, value) ((msr) = ((msr) & ~(0b1 << 11)) | (((value)&0b1) << 11))
#define MSR_SET_SE(msr, value)  ((msr) = ((msr) & ~(0b1 << 10)) | (((value)&0b1) << 10))
#define MSR_SET_BE(msr, value)  ((msr) = ((msr) & ~(0b1 << 9)) | (((value)&0b1) << 9))
#define MSR_SET_FE1(msr, value) ((msr) = ((msr) & ~(0b1 << 8)) | (((value)&0b1) << 8))
#define MSR_SET_IR(msr, value)  ((msr) = ((msr) & ~(0b1 << 5)) | (((value)&0b1) << 5))
#define MSR_SET_DR(msr, value)  ((msr) = ((msr) & ~(0b1 << 4)) | (((value)&0b1) << 4))
#define MSR_SET_PMM(msr, value) ((msr) = ((msr) & ~(0b1 << 2)) | (((value)&0b1) << 2))
#define MSR_SET_RI(msr, value)  ((msr) = ((msr) & ~(0b1 << 1)) | (((value)&0b1) << 1))
#define MSR_SET_LE(msr, value)  ((msr) = ((msr) & ~(0b1)) | ((value)&0b1))

    typedef u64 DAR;

    typedef u32 DSISR;

    typedef u64 GPR;

    enum class FPRState {
        FPR_NZERO = 0x12,
        FPR_PZERO = 0x2,
        FPR_NNORMALIZED = 0x8,
        FPR_PNORMALIZED = 0x4,
        FPR_NDENORMALIZED = 0x18,
        FPR_PDENORMALIZED = 0x14,
        FPR_NINFINITE = 0x9,
        FPR_PINFINITE = 0x5,
        FPR_NAN = 0x11,
    };

    struct PairedSingle {
        u64 ps0AsU64() const { return m_ps0; }
        u64 ps1AsU64() const { return m_ps1; }

        u32 ps0AsU32() const { return static_cast<u32>(m_ps0); }
        u32 ps1AsU32() const { return static_cast<u32>(m_ps1); }

        f64 ps0AsDouble() const { return std::bit_cast<f64>(m_ps0); }
        f64 ps1AsDouble() const { return std::bit_cast<f64>(m_ps1); }

        void setPS0(u64 value) { m_ps0 = value; }
        void setPS0(f64 value) { m_ps0 = std::bit_cast<u64>(m_ps0); }

        void setPS1(u64 value) { m_ps1 = value; }
        void setPS1(f64 value) { m_ps1 = std::bit_cast<u64>(m_ps1); }

        void setBoth(u64 lhs, u64 rhs) {
            setPS0(lhs);
            setPS1(rhs);
        }
        void setBoth(f64 lhs, f64 rhs) {
            setPS0(lhs);
            setPS1(rhs);
        }

        void fill(u64 value) { setBoth(value, value); }
        void fill(f64 value) { setBoth(value, value); }

        u64 m_ps0 = 0;
        u64 m_ps1 = 0;
    };

#if 0
    struct FPR {
        union {
            union {
                bool m_sign    : 1;
                u8 m_exp_bias  : 8;
                u32 m_fraction : 23;
            } ieee_32;
            union {
                bool m_sign    : 1;
                u16 m_exp_bias : 11;
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
            case FPRState::FPR_ZERO:
                return 0.0f;
            case FPRState::FPR_INFINITE: {
                float sign = m_value.ieee_32.m_sign ? -1.0f : 1.0f;
                return sign * INFINITY;
            }
            case FPRState::FPR_NANS:
                return NAN;
            case FPRState::FPR_DENORMALIZED:
            case FPRState::FPR_NORMALIZED: {
                float sign        = m_value.ieee_32.m_sign ? -1.0f : 1.0f;
                int exponent      = static_cast<int>(m_value.ieee_32.m_exp_bias) - 127;
                float significand = 1.0f + (m_value.ieee_32.m_fraction / 8388608.0f);
                return static_cast<float>(sign * std::pow(2, exponent) * significand);
            }
            }
        }

        double evald(FPRState *state = nullptr) const {
            FPRState st = evalStated();
            if (state)
                *state = st;
            switch (st) {
            case FPRState::FPR_ZERO:
                return 0.0f;
            case FPRState::FPR_INFINITE: {
                double sign = m_value.ieee_64.m_sign ? -1.0 : 1.0;
                return sign * INFINITY;
            }
            case FPRState::FPR_NANS:
                return NAN;
            case FPRState::FPR_DENORMALIZED:
            case FPRState::FPR_NORMALIZED: {
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
                    return FPRState::FPR_ZERO;
                }
                return FPRState::FPR_DENORMALIZED;
            }

            if (m_value.ieee_32.m_exp_bias == 255) {
                if (m_value.ieee_32.m_fraction == 0) {
                    return FPRState::FPR_INFINITE;
                }
                return FPRState::FPR_NANS;
            }

            return FPRState::FPR_NORMALIZED;
        }

        FPRState evalStated() const {
            if (m_value.ieee_64.m_exp_bias == 0) {
                if (m_value.ieee_64.m_fraction == 0) {
                    return FPRState::FPR_ZERO;
                }
                return FPRState::FPR_DENORMALIZED;
            }

            if (m_value.ieee_64.m_exp_bias == 2047) {
                if (m_value.ieee_64.m_fraction == 0) {
                    return FPRState::FPR_INFINITE;
                }
                return FPRState::FPR_NANS;
            }

            return FPRState::FPR_NORMALIZED;
        }
    };
#else
    typedef PairedSingle FPR;
#endif

    enum class FPSCRCmp : u8 {
        NONE = 0,
        FE   = (1 << 1),
        FG   = (1 << 2),
        FL   = (1 << 3),
        FU   = (1 << 0),
    };
    TOOLBOX_BITWISE_ENUM(FPSCRCmp)

    enum class FPSCRRound : u8 {
        FPSCR_NEAREST,
        FPSCR_ZERO,
        FPSCR_PINFINITY,
        FPSCR_NINFINITY,
    };

#define FPRF_SHIFT 12
#define FPRF_WIDTH 5
#define FPRF_MASK  (0x1F << FPRF_SHIFT)
#define FPCC_MASK  (0xF << FPRF_SHIFT)

    // FPSCR exception flags
    enum FPSCRExceptionFlag : u32 {
        FPSCR_EX_FX     = 1U << (31 - 0),
        FPSCR_EX_FEX    = 1U << (31 - 1),
        FPSCR_EX_VX     = 1U << (31 - 2),
        FPSCR_EX_OX     = 1U << (31 - 3),
        FPSCR_EX_UX     = 1U << (31 - 4),
        FPSCR_EX_ZX     = 1U << (31 - 5),
        FPSCR_EX_XX     = 1U << (31 - 6),
        FPSCR_EX_VXSNAN = 1U << (31 - 7),
        FPSCR_EX_VXISI  = 1U << (31 - 8),
        FPSCR_EX_VXIDI  = 1U << (31 - 9),
        FPSCR_EX_VXZDZ  = 1U << (31 - 10),
        FPSCR_EX_VXIMZ  = 1U << (31 - 11),
        FPSCR_EX_VXVC   = 1U << (31 - 12),
        FPSCR_EX_VXSOFT = 1U << (31 - 21),
        FPSCR_EX_VXSQRT = 1U << (31 - 22),
        FPSCR_EX_VXCVI  = 1U << (31 - 23),
        FPSCR_EX_VE     = 1U << (31 - 24),
        FPSCR_EX_OE     = 1U << (31 - 25),
        FPSCR_EX_UE     = 1U << (31 - 26),
        FPSCR_EX_ZE     = 1U << (31 - 27),
        FPSCR_EX_XE     = 1U << (31 - 28),

        FPSCR_EX_VX_ANY = FPSCR_EX_VXSNAN | FPSCR_EX_VXISI | FPSCR_EX_VXIDI | FPSCR_EX_VXZDZ | FPSCR_EX_VXIMZ |
                       FPSCR_EX_VXVC | FPSCR_EX_VXSOFT | FPSCR_EX_VXSQRT | FPSCR_EX_VXCVI,

        FPSCR_EX_ANY_X = FPSCR_EX_OX | FPSCR_EX_UX | FPSCR_EX_ZX | FPSCR_EX_XX | FPSCR_EX_VX_ANY,

        FPSCR_EX_ANY_E = FPSCR_EX_VE | FPSCR_EX_OE | FPSCR_EX_UE | FPSCR_EX_ZE | FPSCR_EX_XE,
    };
    TOOLBOX_BITWISE_ENUM(FPSCRExceptionFlag)

    typedef u32 FPSCR;

    // clang-format off
#define FPSCR_FX(fpscr)     (bool)(((fpscr) >> 31) & 0b1)
#define FPSCR_FEX(fpscr)    (bool)(((fpscr) >> 30) & 0b1)
#define FPSCR_VX(fpscr)     (bool)(((fpscr) >> 29) & 0b1)
#define FPSCR_OX(fpscr)     (bool)(((fpscr) >> 28) & 0b1)
#define FPSCR_UX(fpscr)     (bool)(((fpscr) >> 27) & 0b1)
#define FPSCR_ZX(fpscr)     (bool)(((fpscr) >> 26) & 0b1)
#define FPSCR_XX(fpscr)     (bool)(((fpscr) >> 25) & 0b1)
#define FPSCR_VXSNAN(fpscr) (bool)(((fpscr) >> 24) & 0b1)
#define FPSCR_VXISI(fpscr)  (bool)(((fpscr) >> 23) & 0b1)
#define FPSCR_VXIDI(fpscr)  (bool)(((fpscr) >> 22) & 0b1)
#define FPSCR_VXZDZ(fpscr)  (bool)(((fpscr) >> 21) & 0b1)
#define FPSCR_VXIMZ(fpscr)  (bool)(((fpscr) >> 20) & 0b1)
#define FPSCR_VXVC(fpscr)   (bool)(((fpscr) >> 19) & 0b1)
#define FPSCR_FR(fpscr)     (bool)(((fpscr) >> 18) & 0b1)
#define FPSCR_FI(fpscr)     (bool)(((fpscr) >> 17) & 0b1)
#define FPSCR_C(fpscr)      (bool)(((fpscr) >> 16) & 0b1)
#define FPSCR_FPRT(fpscr)   (FPSCRCmp)(((fpscr) >> 15) & 0b11111)
#define FPSCR_VXSOFT(fpscr) (bool)(((fpscr) >> 10) & 0b1)
#define FPSCR_VXSQRT(fpscr) (bool)(((fpscr) >> 9) & 0b1)
#define FPSCR_VXCVI(fpscr)  (bool)(((fpscr) >> 8) & 0b1)
#define FPSCR_VE(fpscr)     (bool)(((fpscr) >> 7) & 0b1)
#define FPSCR_OE(fpscr)     (bool)(((fpscr) >> 6) & 0b1)
#define FPSCR_UE(fpscr)     (bool)(((fpscr) >> 5) & 0b1)
#define FPSCR_ZE(fpscr)     (bool)(((fpscr) >> 4) & 0b1)
#define FPSCR_XE(fpscr)     (bool)(((fpscr) >> 3) & 0b1)
#define FPSCR_NI(fpscr)     (bool)(((fpscr) >> 2) & 0b1)
#define FPSCR_RN(fpscr)     (FPSCRRound)((fpscr)&0b11)

#define FPSCR_SET_FX(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 31)) | (((value)&0b1) << 31))
#define FPSCR_SET_FEX(fpscr, value)    ((fpscr) = ((fpscr) & ~(0b1 << 30)) | (((value)&0b1) << 30))
#define FPSCR_SET_VX(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 29)) | (((value)&0b1) << 29))
#define FPSCR_SET_OX(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 28)) | (((value)&0b1) << 28))
#define FPSCR_SET_UX(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 27)) | (((value)&0b1) << 27))
#define FPSCR_SET_ZX(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 26)) | (((value)&0b1) << 26))
#define FPSCR_SET_XX(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 25)) | (((value)&0b1) << 25))
#define FPSCR_SET_VXSNAN(fpscr, value) ((fpscr) = ((fpscr) & ~(0b1 << 24)) | (((value)&0b1) << 24))
#define FPSCR_SET_VXISI(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 23)) | (((value)&0b1) << 23))
#define FPSCR_SET_VXIDI(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 22)) | (((value)&0b1) << 22))
#define FPSCR_SET_VXZDZ(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 21)) | (((value)&0b1) << 21))
#define FPSCR_SET_VXIMZ(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 20)) | (((value)&0b1) << 20))
#define FPSCR_SET_VXVC(fpscr, value)   ((fpscr) = ((fpscr) & ~(0b1 << 19)) | (((value)&0b1) << 19))
#define FPSCR_SET_FR(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 18)) | (((value)&0b1) << 18))
#define FPSCR_SET_FI(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 17)) | (((value)&0b1) << 17))
#define FPSCR_SET_FPRT(fpscr, value)   ((fpscr) = ((fpscr) & ~(0b11111 << 12)) | (((value)&0b11111) << 12))
#define FPSCR_SET_VXSOFT(fpscr, value) ((fpscr) = ((fpscr) & ~(0b1 << 10)) | (((value)&0b1) << 10))
#define FPSCR_SET_VXSQRT(fpscr, value) ((fpscr) = ((fpscr) & ~(0b1 << 9)) | (((value)&0b1) << 9))
#define FPSCR_SET_VXCVI(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 8)) | (((value)&0b1) << 8))
#define FPSCR_SET_VE(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 7)) | (((value)&0b1) << 7))
#define FPSCR_SET_OE(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 6)) | (((value)&0b1) << 6))
#define FPSCR_SET_UE(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 5)) | (((value)&0b1) << 5))
#define FPSCR_SET_ZE(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 4)) | (((value)&0b1) << 4))
#define FPSCR_SET_XE(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 3)) | (((value)&0b1) << 3))
#define FPSCR_SET_NI(fpscr, value)     ((fpscr) = ((fpscr) & ~(0b1 << 2)) | (((value)&0b1) << 2))
#define FPSCR_SET_RN(fpscr, value)     ((fpscr) = ((fpscr) & ~0b11) | ((value)&0b11))
    // clang-format on

    typedef u32 XER;

#define XER_SO(fpscr)  (bool)(((fpscr) >> 31) & 0b1)
#define XER_OV(fpscr)  (bool)(((fpscr) >> 30) & 0b1)
#define XER_CA(fpscr)  (bool)(((fpscr) >> 29) & 0b1)
#define XER_STR(fpscr) (bool)((fpscr)&0b1111111)

#define XER_SET_SO(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 31)) | ((value) << 31))
#define XER_SET_OV(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 30)) | ((value) << 30))
#define XER_SET_CA(fpscr, value)  ((fpscr) = ((fpscr) & ~(0b1 << 29)) | ((value) << 29))
#define XER_SET_STR(fpscr, value) ((fpscr) = ((fpscr) & ~0b1111111) | ((value)&0b1111111))

    enum class CRCmp : u8 {
        NONE = 0,
        EQ   = (1 << 1),
        GT   = (1 << 2),
        LT   = (1 << 3),
        SO   = (1 << 0),
    };
    TOOLBOX_BITWISE_ENUM(CRCmp)

    struct CR {
        u32 m_crf;

        void cmp(u8 crf, s32 ra, s32 rb, const XER &xer) {
            if (ra < rb) {
                m_crf = (u8)(CRCmp::LT | (XER_SO(xer) ? CRCmp::SO : CRCmp::NONE))
                        << ((7 - crf) * 4);
                return;
            }

            if (ra > rb) {
                m_crf = (u8)(CRCmp::GT | (XER_SO(xer) ? CRCmp::SO : CRCmp::NONE))
                        << ((7 - crf) * 4);
                return;
            }

            m_crf = (u8)(CRCmp::EQ | (XER_SO(xer) ? CRCmp::SO : CRCmp::NONE)) << ((7 - crf) * 4);
            return;
        }

        void cmp(u8 crf, u32 ra, u32 rb, const XER &xer) {
            if (ra < rb) {
                m_crf = (u8)(CRCmp::LT | (XER_SO(xer) ? CRCmp::SO : CRCmp::NONE))
                        << ((7 - crf) * 4);
                return;
            }

            if (ra > rb) {
                m_crf = (u8)(CRCmp::GT | (XER_SO(xer) ? CRCmp::SO : CRCmp::NONE))
                        << ((7 - crf) * 4);
                return;
            }

            m_crf = (u8)(CRCmp::EQ | (XER_SO(xer) ? CRCmp::SO : CRCmp::NONE)) << ((7 - crf) * 4);
            return;
        }

        void cmp(u8 crf, f32 fa, f32 fb) {
            if (fa < fb) {
                m_crf = (u8)(CRCmp::LT);
                return;
            }

            if (fa > fb) {
                m_crf = (u8)(CRCmp::GT);
                return;
            }

            m_crf = (u8)(CRCmp::EQ);
            return;
        }

        void cmp(u8 crf, f64 fa, f64 fb) {
            if (fa < fb) {
                m_crf = (u8)(CRCmp::LT);
                return;
            }

            if (fa > fb) {
                m_crf = (u8)(CRCmp::GT);
                return;
            }

            m_crf = (u8)(CRCmp::EQ);
            return;
        }

        bool is(u8 crf, CRCmp cmp) const { return (bool)((m_crf >> ((7 - crf) * 4)) & (u8)cmp); }
    };

    #define SET_CR_FIELD(cr, field, value) ((cr).m_crf = ((cr).m_crf & ~(0b1111 << ((7 - (field)) * 4))) | (((value) & 0b1111) << ((7 - (field)) * 4)))

    typedef u64 LR;
    typedef u64 CTR;
    typedef u64 SRR0;
    typedef u64 SRR1;

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
        SRR0 m_srr0;
        SRR1 m_srr1;
    };

}  // namespace Toolbox::Interpreter::Register