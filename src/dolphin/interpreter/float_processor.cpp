#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <limits>

namespace DolphinLib {
    using namespace Toolbox::Interpreter::Register;

    template <typename T> constexpr T SNANConstant() {
        return std::numeric_limits<T>::signaling_NaN();
    }

    // The most significant bit of the fraction is an is-quiet bit on all architectures we care
    // about.
    static constexpr u64 DOUBLE_QBIT = 0x0008000000000000ULL;
    static constexpr u64 DOUBLE_SIGN = 0x8000000000000000ULL;
    static constexpr u64 DOUBLE_EXP  = 0x7FF0000000000000ULL;
    static constexpr u64 DOUBLE_FRAC = 0x000FFFFFFFFFFFFFULL;
    static constexpr u64 DOUBLE_ZERO = 0x0000000000000000ULL;

    static constexpr u32 FLOAT_SIGN = 0x80000000;
    static constexpr u32 FLOAT_EXP  = 0x7F800000;
    static constexpr u32 FLOAT_FRAC = 0x007FFFFF;
    static constexpr u32 FLOAT_ZERO = 0x00000000;

    inline bool IsQNAN(f64 d) {
        const u64 i = std::bit_cast<u64>(d);
        return ((i & DOUBLE_EXP) == DOUBLE_EXP) && ((i & DOUBLE_QBIT) == DOUBLE_QBIT);
    }

    inline bool IsSNAN(f64 d) {
        const u64 i = std::bit_cast<u64>(d);
        return ((i & DOUBLE_EXP) == DOUBLE_EXP) && ((i & DOUBLE_FRAC) != DOUBLE_ZERO) &&
               ((i & DOUBLE_QBIT) == DOUBLE_ZERO);
    }

    inline float FlushToZero(float f) {
        u32 i = std::bit_cast<u32>(f);
        if ((i & FLOAT_EXP) == 0) {
            // Turn into signed zero
            i &= FLOAT_SIGN;
        }
        return std::bit_cast<float>(i);
    }

    inline f64 FlushToZero(f64 d) {
        u64 i = std::bit_cast<u64>(d);
        if ((i & DOUBLE_EXP) == 0) {
            // Turn into signed zero
            i &= DOUBLE_SIGN;
        }
        return std::bit_cast<f64>(i);
    }

    // Uses PowerPC conventions for the return value, so it can be easily
    // used directly in CPU emulation.
    FPRState ClassifyDouble(f64 dvalue) {
        const u64 ivalue = std::bit_cast<u64>(dvalue);
        const u64 sign   = ivalue & DOUBLE_SIGN;
        const u64 exp    = ivalue & DOUBLE_EXP;

        if (exp > DOUBLE_ZERO && exp < DOUBLE_EXP) {
            // Nice normalized number.
            return sign ? FPRState::FPR_NNORMALIZED : FPRState::FPR_PNORMALIZED;
        }

        const u64 mantissa = ivalue & DOUBLE_FRAC;
        if (mantissa) {
            if (exp)
                return FPRState::FPR_NAN;

            // Denormalized number.
            return sign ? FPRState::FPR_NDENORMALIZED : FPRState::FPR_PDENORMALIZED;
        }

        if (exp) {
            // Infinite
            return sign ? FPRState::FPR_NINFINITE : FPRState::FPR_PINFINITE;
        }

        // Zero
        return sign ? FPRState::FPR_NZERO : FPRState::FPR_PZERO;
    }

    FPRState ClassifyFloat(float fvalue) {
        const u32 ivalue = std::bit_cast<u32>(fvalue);
        const u32 sign   = ivalue & FLOAT_SIGN;
        const u32 exp    = ivalue & FLOAT_EXP;

        if (exp > FLOAT_ZERO && exp < FLOAT_EXP) {
            // Nice normalized number.
            return sign ? FPRState::FPR_NNORMALIZED : FPRState::FPR_PNORMALIZED;
        }

        const u32 mantissa = ivalue & FLOAT_FRAC;
        if (mantissa) {
            if (exp)
                return FPRState::FPR_NAN;

            // Denormalized number.
            return sign ? FPRState::FPR_NDENORMALIZED : FPRState::FPR_PDENORMALIZED;
        }

        if (exp) {
            // Infinite
            return sign ? FPRState::FPR_NINFINITE : FPRState::FPR_PINFINITE;
        }

        // Zero
        return sign ? FPRState::FPR_NZERO : FPRState::FPR_PZERO;
    }

    struct BaseAndDec {
        int m_base;
        int m_dec;
    };

    const std::array<BaseAndDec, 32> frsqrte_expected = {
        {
         {0x3ffa000, 0x7a4}, {0x3c29000, 0x700}, {0x38aa000, 0x670}, {0x3572000, 0x5f2},
         {0x3279000, 0x584}, {0x2fb7000, 0x524}, {0x2d26000, 0x4cc}, {0x2ac0000, 0x47e},
         {0x2881000, 0x43a}, {0x2665000, 0x3fa}, {0x2468000, 0x3c2}, {0x2287000, 0x38e},
         {0x20c1000, 0x35e}, {0x1f12000, 0x332}, {0x1d79000, 0x30a}, {0x1bf4000, 0x2e6},
         {0x1a7e800, 0x568}, {0x17cb800, 0x4f3}, {0x1552800, 0x48d}, {0x130c000, 0x435},
         {0x10f2000, 0x3e7}, {0x0eff000, 0x3a2}, {0x0d2e000, 0x365}, {0x0b7c000, 0x32e},
         {0x09e5000, 0x2fc}, {0x0867000, 0x2d0}, {0x06ff000, 0x2a8}, {0x05ab800, 0x283},
         {0x046a000, 0x261}, {0x0339800, 0x243}, {0x0218800, 0x226}, {0x0105800, 0x20b},
         }
    };

    const std::array<BaseAndDec, 32> fres_expected = {
        {
         {0x7ff800, 0x3e1}, {0x783800, 0x3a7}, {0x70ea00, 0x371}, {0x6a0800, 0x340},
         {0x638800, 0x313}, {0x5d6200, 0x2ea}, {0x579000, 0x2c4}, {0x520800, 0x2a0},
         {0x4cc800, 0x27f}, {0x47ca00, 0x261}, {0x430800, 0x245}, {0x3e8000, 0x22a},
         {0x3a2c00, 0x212}, {0x360800, 0x1fb}, {0x321400, 0x1e5}, {0x2e4a00, 0x1d1},
         {0x2aa800, 0x1be}, {0x272c00, 0x1ac}, {0x23d600, 0x19b}, {0x209e00, 0x18b},
         {0x1d8800, 0x17c}, {0x1a9000, 0x16e}, {0x17ae00, 0x15b}, {0x14f800, 0x15b},
         {0x124400, 0x143}, {0x0fbe00, 0x143}, {0x0d3800, 0x12d}, {0x0ade00, 0x12d},
         {0x088400, 0x11a}, {0x065000, 0x11a}, {0x041c00, 0x108}, {0x020c00, 0x106},
         }
    };

    // PowerPC approximation algorithms
    f64 ApproximateReciprocalSquareRoot(f64 val) {
        s64 integral   = std::bit_cast<s64>(val);
        s64 mantissa   = integral & ((1LL << 52) - 1);
        const s64 sign = integral & (1ULL << 63);
        s64 exponent   = integral & (0x7FFLL << 52);

        // Special case 0
        if (mantissa == 0 && exponent == 0) {
            return sign ? -std::numeric_limits<f64>::infinity()
                        : std::numeric_limits<f64>::infinity();
        }

        // Special case NaN-ish numbers
        if (exponent == (0x7FFLL << 52)) {
            if (mantissa == 0) {
                if (sign)
                    return std::numeric_limits<f64>::quiet_NaN();

                return 0.0;
            }

            return 0.0 + val;
        }

        // Negative numbers return NaN
        if (sign)
            return std::numeric_limits<f64>::quiet_NaN();

        if (!exponent) {
            // "Normalize" denormal values
            do {
                exponent -= 1LL << 52;
                mantissa <<= 1;
            } while (!(mantissa & (1LL << 52)));
            mantissa &= (1LL << 52) - 1;
            exponent += 1LL << 52;
        }

        const bool odd_exponent = !(exponent & (1LL << 52));
        exponent = ((0x3FFLL << 52) - ((exponent - (0x3FELL << 52)) / 2)) & (0x7FFLL << 52);
        integral = sign | exponent;

        const int i       = static_cast<int>(mantissa >> 37);
        const int index   = i / 2048 + (odd_exponent ? 16 : 0);
        const auto &entry = frsqrte_expected[index];
        integral |= static_cast<s64>(entry.m_base - entry.m_dec * (i % 2048)) << 26;

        return std::bit_cast<f64>(integral);
    }

    f64 ApproximateReciprocal(f64 val) {
        s64 integral       = std::bit_cast<s64>(val);
        const s64 mantissa = integral & ((1LL << 52) - 1);
        const s64 sign     = integral & (1ULL << 63);
        s64 exponent       = integral & (0x7FFLL << 52);

        // Special case 0
        if (mantissa == 0 && exponent == 0)
            return std::copysign(std::numeric_limits<f64>::infinity(), val);

        // Special case NaN-ish numbers
        if (exponent == (0x7FFLL << 52)) {
            if (mantissa == 0)
                return std::copysign(0.0, val);
            return 0.0 + val;
        }

        // Special case small inputs
        if (exponent < (895LL << 52))
            return std::copysign(std::numeric_limits<float>::max(), val);

        // Special case large inputs
        if (exponent >= (1149LL << 52))
            return std::copysign(0.0, val);

        exponent = (0x7FDLL << 52) - exponent;

        const int i       = static_cast<int>(mantissa >> 37);
        const auto &entry = fres_expected[i / 1024];
        integral          = sign | exponent;
        integral |= static_cast<s64>(entry.m_base - (entry.m_dec * (i % 1024) + 1) / 2) << 29;

        return std::bit_cast<f64>(integral);
    }

}  // namespace DolphinLib

namespace Toolbox::Interpreter {

    // Pulled FP stuff from the Dolphin Emulator Project, thanks :)

    static inline void UpdateCR1(Register::CR &cr, Register::FPSCR &fpscr) {
        SET_CR_FIELD(cr, 1,
                     (FPSCR_FX(fpscr) << 3) | (FPSCR_FEX(fpscr) << 2) | (FPSCR_VX(fpscr) << 1) |
                         (int)FPSCR_OX(fpscr));
    }

    using namespace Register;

    constexpr f64 PPC_NAN = std::numeric_limits<f64>::quiet_NaN();

    inline void CheckFPExceptions(Register::FPSCR &fpscr, Register::MSR &msr,
                                  Register::SRR1 &srr1) {
        if (FPSCR_FEX(fpscr) && (MSR_FE0(msr) || MSR_FE1(msr)))
            srr1 = (u32)ExceptionCause::EXCEPTION_FPU_UNAVAILABLE;
    }

    inline void UpdateFPExceptionSummary(Register::FPSCR &fpscr, Register::MSR &msr,
                                         Register::SRR1 &srr1) {
        FPSCR_SET_VX(fpscr, (fpscr & FPSCRExceptionFlag::FPSCR_EX_VX_ANY) != 0);
        FPSCR_SET_FEX(fpscr, (fpscr & FPSCRExceptionFlag::FPSCR_EX_ANY_E) != 0);

        CheckFPExceptions(fpscr, msr, srr1);
    }

    inline void SetFPException(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1,
                               u32 mask) {
        if ((fpscr & mask) != mask) {
            FPSCR_SET_FX(fpscr, true);
        }

        fpscr |= mask;
        UpdateFPExceptionSummary(fpscr, msr, srr1);
    }

    inline float ForceSingle(const Register::FPSCR &fpscr, f64 value) {
        if (FPSCR_NI(fpscr)) {
            // Emulate a rounding quirk. If the conversion result before rounding is a subnormal
            // single, it's always flushed to zero, even if rounding would have caused it to become
            // normal.

            constexpr u64 smallest_normal_single = 0x3810000000000000;
            const u64 value_without_sign =
                std::bit_cast<u64>(value) & (DolphinLib::DOUBLE_EXP | DolphinLib::DOUBLE_FRAC);

            if (value_without_sign < smallest_normal_single) {
                const u64 flushed_f64    = std::bit_cast<u64>(value) & DolphinLib::DOUBLE_SIGN;
                const u32 flushed_single = static_cast<u32>(flushed_f64 >> 32);
                return std::bit_cast<float>(flushed_single);
            }
        }

        // Emulate standard conversion to single precision.

        float x = static_cast<float>(value);
        if (FPSCR_NI(fpscr)) {
            x = DolphinLib::FlushToZero(x);
        }
        return x;
    }

    inline f64 ForceDouble(const Register::FPSCR &fpscr, f64 d) {
        if (FPSCR_NI(fpscr)) {
            d = DolphinLib::FlushToZero(d);
        }
        return d;
    }

    inline f64 Force25Bit(f64 d) {
        u64 integral = std::bit_cast<u64>(d);

        integral = (integral & 0xFFFFFFFFF8000000ULL) + (integral & 0x8000000);

        return std::bit_cast<f64>(integral);
    }

    inline f64 MakeQuiet(f64 d) {
        const u64 integral = std::bit_cast<u64>(d) | DolphinLib::DOUBLE_QBIT;

        return std::bit_cast<f64>(integral);
    }

    // these functions allow globally modify operations behaviour
    // also, these may be used to set flags like FR, FI, OX, UX

    struct FPResult {
        bool HasNoInvalidExceptions() const {
            return (exception & FPSCRExceptionFlag::FPSCR_EX_VX_ANY) == 0;
        }

        void SetException(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1,
                          FPSCRExceptionFlag flag) {
            exception = flag;
            SetFPException(fpscr, msr, srr1, flag);
        }

        f64 value = 0.0;
        FPSCRExceptionFlag exception{};
    };

    inline FPResult NI_mul(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, f64 a,
                           f64 b) {
        FPResult result{a * b};

        if (std::isnan(result.value)) {
            if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b)) {
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
            }

            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);

            if (std::isnan(a)) {
                result.value = MakeQuiet(a);
                return result;
            }
            if (std::isnan(b)) {
                result.value = MakeQuiet(b);
                return result;
            }

            result.value = PPC_NAN;
            result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXIMZ);
            return result;
        }

        return result;
    }

    inline FPResult NI_div(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, f64 a,
                           f64 b) {
        FPResult result{a / b};

        if (std::isinf(result.value)) {
            if (b == 0.0) {
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_ZX);
                return result;
            }
        } else if (std::isnan(result.value)) {
            if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b))
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);

            if (std::isnan(a)) {
                result.value = MakeQuiet(a);
                return result;
            }
            if (std::isnan(b)) {
                result.value = MakeQuiet(b);
                return result;
            }

            if (b == 0.0)
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXZDZ);
            else if (std::isinf(a) && std::isinf(b))
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXIDI);

            result.value = PPC_NAN;
            return result;
        }

        return result;
    }

    inline FPResult NI_add(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, f64 a,
                           f64 b) {
        FPResult result{a + b};

        if (std::isnan(result.value)) {
            if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b))
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);

            if (std::isnan(a)) {
                result.value = MakeQuiet(a);
                return result;
            }
            if (std::isnan(b)) {
                result.value = MakeQuiet(b);
                return result;
            }

            result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXISI);
            result.value = PPC_NAN;
            return result;
        }

        if (std::isinf(a) || std::isinf(b))

            FPSCR_SET_FI(fpscr, false);
        FPSCR_SET_FR(fpscr, false);

        return result;
    }

    inline FPResult NI_sub(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, f64 a,
                           f64 b) {
        FPResult result{a - b};

        if (std::isnan(result.value)) {
            if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b))
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);

            if (std::isnan(a)) {
                result.value = MakeQuiet(a);
                return result;
            }
            if (std::isnan(b)) {
                result.value = MakeQuiet(b);
                return result;
            }

            result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXISI);
            result.value = PPC_NAN;
            return result;
        }

        if (std::isinf(a) || std::isinf(b)) {
            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);
        }

        return result;
    }

    // FMA instructions on PowerPC are weird:
    // They calculate (a * c) + b, but the order in which
    // inputs are checked for NaN is still a, b, c.
    inline FPResult NI_madd(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, f64 a,
                            f64 c, f64 b) {
        FPResult result{std::fma(a, c, b)};

        if (std::isnan(result.value)) {
            if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b) || DolphinLib::IsSNAN(c))
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);

            if (std::isnan(a)) {
                result.value = MakeQuiet(a);
                return result;
            }
            if (std::isnan(b)) {
                result.value = MakeQuiet(b);  // !
                return result;
            }
            if (std::isnan(c)) {
                result.value = MakeQuiet(c);
                return result;
            }

            result.SetException(fpscr, msr, srr1,
                                std::isnan(a * c) ? FPSCRExceptionFlag::FPSCR_EX_VXIMZ
                                                  : FPSCRExceptionFlag::FPSCR_EX_VXISI);
            result.value = PPC_NAN;
            return result;
        }

        if (std::isinf(a) || std::isinf(b) || std::isinf(c))
            FPSCR_SET_FI(fpscr, false);
        FPSCR_SET_FR(fpscr, false);

        return result;
    }

    inline FPResult NI_msub(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, f64 a,
                            f64 c, f64 b) {
        FPResult result{std::fma(a, c, -b)};

        if (std::isnan(result.value)) {
            if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b) || DolphinLib::IsSNAN(c))
                result.SetException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

            FPSCR_SET_FI(fpscr, false);
            FPSCR_SET_FR(fpscr, false);

            if (std::isnan(a)) {
                result.value = MakeQuiet(a);
                return result;
            }
            if (std::isnan(b)) {
                result.value = MakeQuiet(b);  // !
                return result;
            }
            if (std::isnan(c)) {
                result.value = MakeQuiet(c);
                return result;
            }

            result.SetException(fpscr, msr, srr1,
                                std::isnan(a * c) ? FPSCRExceptionFlag::FPSCR_EX_VXIMZ
                                                  : FPSCRExceptionFlag::FPSCR_EX_VXISI);
            result.value = PPC_NAN;
            return result;
        }

        if (std::isinf(a) || std::isinf(b) || std::isinf(c))
            FPSCR_SET_FI(fpscr, false);
        FPSCR_SET_FR(fpscr, false);

        return result;
    }

    // used by stfsXX instructions and ps_rsqrte
    inline u32 ConvertToSingle(u64 x) {
        const u32 exp = u32((x >> 52) & 0x7ff);

        if (exp > 896 || (x & ~DolphinLib::DOUBLE_SIGN) == 0) {
            return u32(((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff));
        } else if (exp >= 874) {
            u32 t = u32(0x80000000 | ((x & DolphinLib::DOUBLE_FRAC) >> 21));
            t     = t >> (905 - exp);
            t |= u32((x >> 32) & 0x80000000);
            return t;
        } else {
            // This is said to be undefined.
            // The code is based on hardware tests.
            return u32(((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff));
        }
    }

    // used by psq_stXX operations.
    inline u32 ConvertToSingleFTZ(u64 x) {
        const u32 exp = u32((x >> 52) & 0x7ff);

        if (exp > 896 || (x & ~DolphinLib::DOUBLE_SIGN) == 0) {
            return u32(((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff));
        } else {
            return u32((x >> 32) & 0x80000000);
        }
    }

    inline u64 ConvertToDouble(u32 value) {
        // This is a little-endian re-implementation of the algorithm described in
        // the PowerPC Programming Environments Manual for loading single
        // precision floating point numbers.
        // See page 566 of http://www.freescale.com/files/product/doc/MPCFPE32B.pdf

        u64 x    = value;
        u64 exp  = (x >> 23) & 0xff;
        u64 frac = x & 0x007fffff;

        if (exp > 0 && exp < 255)  // Normal number
        {
            u64 y = !(exp >> 7);
            u64 z = y << 61 | y << 60 | y << 59;
            return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
        } else if (exp == 0 && frac != 0)  // Subnormal number
        {
            exp = 1023 - 126;
            do {
                frac <<= 1;
                exp -= 1;
            } while ((frac & 0x00800000) == 0);

            return ((x & 0x80000000) << 32) | (exp << 52) | ((frac & 0x007fffff) << 29);
        } else  // QNaN, SNaN or Zero
        {
            u64 y = exp >> 7;
            u64 z = y << 61 | y << 60 | y << 59;
            return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
        }
    }

    namespace {
        // Apply current rounding mode
        enum class RoundingMode {
            Nearest                 = 0b00,
            TowardsZero             = 0b01,
            TowardsPositiveInfinity = 0b10,
            TowardsNegativeInfinity = 0b11
        };

        void SetFI(Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1, u32 fi) {
            if (fi != 0) {
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_XX);
            }
            FPSCR_SET_FI(fpscr, fi);
        }

        // Note that the convert to integer operation is defined
        // in Appendix C.4.2 in PowerPC Microprocessor Family:
        // The Programming Environments Manual for 32 and 64-bit Microprocessors
        void ConvertToInteger(u8 frt, u8 frb, bool rc, Register::FPR fpr[32],
                              Register::FPSCR &fpscr, Register::MSR &msr, Register::SRR1 &srr1,
                              Register::CR &cr, RoundingMode rounding_mode) {
            const double b = fpr[frb].ps0AsDouble();
            u32 value;
            bool exception_occurred = false;

            if (std::isnan(b)) {
                if (DolphinLib::IsSNAN(b))
                    SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

                value = 0x80000000;
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXCVI);
                exception_occurred = true;
            } else if (b > static_cast<double>(0x7fffffff)) {
                // Positive large operand or +inf
                value = 0x7fffffff;
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXCVI);
                exception_occurred = true;
            } else if (b < -static_cast<double>(0x80000000)) {
                // Negative large operand or -inf
                value = 0x80000000;
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXCVI);
                exception_occurred = true;
            } else {
                s32 i = 0;
                switch (rounding_mode) {
                case RoundingMode::Nearest: {
                    const double t = b + 0.5;
                    i              = static_cast<s32>(t);

                    // Ties to even
                    if (t - i < 0 || (t - i == 0 && (i & 1))) {
                        i--;
                    }
                    break;
                }
                case RoundingMode::TowardsZero:
                    i = static_cast<s32>(b);
                    break;
                case RoundingMode::TowardsPositiveInfinity:
                    i = static_cast<s32>(b);
                    if (b - i > 0) {
                        i++;
                    }
                    break;
                case RoundingMode::TowardsNegativeInfinity:
                    i = static_cast<s32>(b);
                    if (b - i < 0) {
                        i--;
                    }
                    break;
                }
                value           = static_cast<u32>(i);
                const double di = i;
                if (di == b) {
                    FPSCR_SET_FI(fpscr, false);
                    FPSCR_SET_FR(fpscr, false);
                } else {
                    // Also sets FPSCR[XX]
                    SetFI(fpscr, msr, srr1, 1);
                    FPSCR_SET_FR(fpscr, fabs(di) > fabs(b));
                }
            }

            if (exception_occurred) {
                FPSCR_SET_FI(fpscr, false);
                FPSCR_SET_FR(fpscr, false);
            }

            if (!exception_occurred || FPSCR_VE(fpscr) == 0) {
                // Based on HW tests
                // FPRF is not affected
                u64 result = 0xfff8000000000000ull | value;
                if (value == 0 && std::signbit(b))
                    result |= 0x100000000ull;

                fpr[frt].setPS0(result);
            }

            if (rc) {
                SET_CR_FIELD(cr, 1,
                             (FPSCR_FX(fpscr) << 3) | (FPSCR_FEX(fpscr) << 2) |
                                 (FPSCR_VX(fpscr) << 1) | (int)FPSCR_OX(fpscr));
            }
        }
    }  // Anonymous namespace

    void FloatingPointProcessor::lfs(u8 frt, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lfs, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].fill(ConvertToDouble(std::byteswap(storage.get<u32>(destination))));
    }

    void FloatingPointProcessor::lfsu(u8 frt, s16 d, u8 ra, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lfsu, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].fill(ConvertToDouble(std::byteswap(storage.get<u32>(destination))));
        gpr[ra] += d;
    }

    void FloatingPointProcessor::lfsx(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lfsx, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].fill(ConvertToDouble(std::byteswap(storage.get<u32>(destination))));
    }

    void FloatingPointProcessor::lfsux(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].fill(ConvertToDouble(std::byteswap(storage.get<u32>(destination))));
        gpr[ra] += gpr[rb];
    }

    void FloatingPointProcessor::lfd(u8 frt, s16 d, u8 ra, Register::GPR gpr[32], Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lfd, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].setPS0(std::byteswap(storage.get<u64>(destination)));
    }

    void FloatingPointProcessor::lfdu(u8 frt, s16 d, u8 ra, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lfdu, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].setPS0(std::byteswap(storage.get<u64>(destination)));
        gpr[ra] += d;
    }

    void FloatingPointProcessor::lfdx(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lfdx, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].setPS0(std::byteswap(storage.get<u64>(destination)));
    }

    void FloatingPointProcessor::lfdux(u8 frt, u8 ra, u8 rb, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lfdux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_fpr[frt].setPS0(std::byteswap(storage.get<u64>(destination)));
        gpr[ra] += gpr[rb];
    }

    void FloatingPointProcessor::stfs(u8 frs, s16 d, u8 ra, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfs, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, std::byteswap(ConvertToSingle(m_fpr[frs].ps0AsU64())));
    }

    void FloatingPointProcessor::stfsu(u8 frs, s16 d, u8 ra, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsu, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, std::byteswap(ConvertToSingle(m_fpr[frs].ps0AsU64())));
        gpr[ra] += d;
    }

    void FloatingPointProcessor::stfsx(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsx, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, std::byteswap(ConvertToSingle(m_fpr[frs].ps0AsU64())));
    }

    void FloatingPointProcessor::stfsux(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32],
                                        Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, std::byteswap(ConvertToSingle(m_fpr[frs].ps0AsU64())));
        gpr[ra] += gpr[rb];
    }

    void FloatingPointProcessor::stfd(u8 frs, s16 d, u8 ra, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u64>(destination, std::byteswap(m_fpr[frs].ps0AsU64()));
    }

    void FloatingPointProcessor::stfdu(u8 frs, s16 d, u8 ra, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + d - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u64>(destination, std::byteswap(m_fpr[frs].ps0AsU64()));
        gpr[ra] += d;
    }

    void FloatingPointProcessor::stfdx(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u64>(destination, std::byteswap(m_fpr[frs].ps0AsU64()));
    }

    void FloatingPointProcessor::stfdux(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32],
                                        Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u64>(destination, std::byteswap(m_fpr[frs].ps0AsU64()));
        gpr[ra] += gpr[rb];
    }

    void FloatingPointProcessor::stfiwx(u8 frs, u8 ra, u8 rb, Register::GPR gpr[32],
                                        Buffer &storage) {
        if (!IsRegValid(frs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stfsux, "Invalid registers detected!"));
            return;
        }
        s32 destination = static_cast<s32>(gpr[ra] + gpr[rb] - 0x80000000) & 0x7FFFFFFF;
        if ((destination & 0b11)) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
            return;
        }
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, std::byteswap(m_fpr[frs].ps0AsU32()));
    }

    // Move

    void FloatingPointProcessor::fmr(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                                     Register::SRR1 &srr1) {
        m_fpr[frt].setPS0(m_fpr[frb].ps0AsU64());

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fabs(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                                      Register::SRR1 &srr1) {
        m_fpr[frt].setPS0(std::fabs(m_fpr[frb].ps0AsDouble()));

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fneg(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                                      Register::SRR1 &srr1) {
        m_fpr[frt].setPS0(m_fpr[frb].ps0AsU64() ^ (UINT64_C(1) << 63));

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fnabs(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        m_fpr[frt].setPS0(m_fpr[frb].ps0AsU64() | (UINT64_C(1) << 63));

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    // Math

    void FloatingPointProcessor::fadd(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                      Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult sum =
            NI_add(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || sum.HasNoInvalidExceptions()) {
            const f64 result = ForceDouble(m_fpscr, sum.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fadds(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult sum =
            NI_add(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || sum.HasNoInvalidExceptions()) {
            const f32 result = ForceSingle(m_fpscr, sum.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fsub(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                      Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult sub =
            NI_sub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || sub.HasNoInvalidExceptions()) {
            const f64 result = ForceDouble(m_fpscr, sub.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fsubs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult sub =
            NI_sub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || sub.HasNoInvalidExceptions()) {
            const f32 result = ForceSingle(m_fpscr, sub.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fmul(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                      Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult mul =
            NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || mul.HasNoInvalidExceptions()) {
            const f64 result = ForceDouble(m_fpscr, mul.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fmuls(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult mul =
            NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || mul.HasNoInvalidExceptions()) {
            const f32 result = ForceSingle(m_fpscr, mul.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fdiv(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                      Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult div =
            NI_div(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || div.HasNoInvalidExceptions()) {
            const f64 result = ForceDouble(m_fpscr, div.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fdivs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult div =
            NI_div(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || div.HasNoInvalidExceptions()) {
            const f32 result = ForceSingle(m_fpscr, div.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult madd = NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                      m_fpr[frc].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || madd.HasNoInvalidExceptions()) {
            const f64 result = ForceDouble(m_fpscr, madd.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult madd =
            NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                    Force25Bit(m_fpr[frc].ps0AsDouble()), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || madd.HasNoInvalidExceptions()) {
            const f32 result = ForceSingle(m_fpscr, madd.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }
    void FloatingPointProcessor::fmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult msub = NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                      m_fpr[frc].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || msub.HasNoInvalidExceptions()) {
            const f64 result = ForceDouble(m_fpscr, msub.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult msub =
            NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                    Force25Bit(m_fpr[frc].ps0AsDouble()), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || msub.HasNoInvalidExceptions()) {
            const f32 result = ForceSingle(m_fpscr, msub.value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }
    void FloatingPointProcessor::fnmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult madd = NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                      m_fpr[frc].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || madd.HasNoInvalidExceptions()) {

            f64 result = ForceDouble(m_fpscr, madd.value);
            if (!std::isnan(result))
                result = -result;
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }
    void FloatingPointProcessor::fnmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult madd =
            NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                    Force25Bit(m_fpr[frc].ps0AsDouble()), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || madd.HasNoInvalidExceptions()) {

            f32 result = ForceSingle(m_fpscr, madd.value);
            if (!std::isnan(result))
                result = -result;
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }
    void FloatingPointProcessor::fnmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult msub = NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                      m_fpr[frc].ps0AsDouble(), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || msub.HasNoInvalidExceptions()) {

            f64 result = ForceDouble(m_fpscr, msub.value);
            if (!std::isnan(result))
                result = -result;
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }
    void FloatingPointProcessor::fnmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const FPResult msub =
            NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                    Force25Bit(m_fpr[frc].ps0AsDouble()), m_fpr[frb].ps0AsDouble());

        if (FPSCR_VE(m_fpscr) == 0 || msub.HasNoInvalidExceptions()) {

            f32 result = ForceSingle(m_fpscr, msub.value);
            if (!std::isnan(result))
                result = -result;
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(result));
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    // Rounding and conversion

    void FloatingPointProcessor::frsp(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                                      Register::SRR1 &srr1) {
        const double b      = m_fpr[frb].ps0AsDouble();
        const float rounded = ForceSingle(m_fpscr, b);

        if (std::isnan(b)) {
            const bool is_snan = DolphinLib::IsSNAN(b);

            if (is_snan)
                SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);

            if (!is_snan || FPSCR_VE(m_fpscr) == 0) {
                m_fpr[frt].fill(rounded);
                FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(rounded));
            }

            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);
        } else {
            SetFI(m_fpscr, msr, srr1, b != rounded);
            FPSCR_SET_FR(m_fpscr, std::fabs(rounded) > std::fabs(b));
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(rounded));
            m_fpr[frt].fill(rounded);
        }

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::fctiw(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        ConvertToInteger(frt, frb, rc, m_fpr, m_fpscr, msr, srr1, cr,
                         static_cast<RoundingMode>(FPSCR_RN(m_fpscr)));
    }

    void FloatingPointProcessor::fctiwz(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        ConvertToInteger(frt, frb, rc, m_fpr, m_fpscr, msr, srr1, cr, RoundingMode::TowardsZero);
    }

    // Compare

    inline void Helper_FloatCompareOrdered(double fa, double fb, u8 crfd, Register::FPSCR &fpscr,
                                           Register::MSR &msr, Register::SRR1 &srr1,
                                           Register::CR &cr) {
        FPSCRCmp compare_result;

        if (std::isnan(fa) || std::isnan(fb)) {
            compare_result = FPSCRCmp::FU;
            if (DolphinLib::IsSNAN(fa) || DolphinLib::IsSNAN(fb)) {
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
                if (FPSCR_VE(fpscr) == 0) {
                    SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXVC);
                }
            } else  // QNaN
            {
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXVC);
            }
        } else if (fa < fb) {
            compare_result = FPSCRCmp::FL;
        } else if (fa > fb) {
            compare_result = FPSCRCmp::FG;
        } else  // Equals
        {
            compare_result = FPSCRCmp::FE;
        }

        const u32 compare_value = static_cast<u32>(compare_result);

        // Clear and set the FPCC bits accordingly.
        FPSCR_SET_FPRT(fpscr, (static_cast<u8>(FPSCR_FPRT(fpscr)) & ~FPCC_MASK) | compare_value);

        SET_CRF_FIELD(cr.m_crf, crfd, compare_value);
    }

    inline void Helper_FloatCompareUnordered(double fa, double fb, u8 crfd, Register::FPSCR &fpscr,
                                             Register::MSR &msr, Register::SRR1 &srr1,
                                             Register::CR &cr) {
        FPSCRCmp compare_result;

        if (std::isnan(fa) || std::isnan(fb)) {
            compare_result = FPSCRCmp::FU;
            if (DolphinLib::IsSNAN(fa) || DolphinLib::IsSNAN(fb)) {
                SetFPException(fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
            }
        } else if (fa < fb) {
            compare_result = FPSCRCmp::FL;
        } else if (fa > fb) {
            compare_result = FPSCRCmp::FG;
        } else  // Equals
        {
            compare_result = FPSCRCmp::FE;
        }

        const u32 compare_value = static_cast<u32>(compare_result);

        // Clear and set the FPCC bits accordingly.
        FPSCR_SET_FPRT(fpscr, (static_cast<u8>(FPSCR_FPRT(fpscr)) & ~FPCC_MASK) | compare_value);

        SET_CRF_FIELD(cr.m_crf, crfd, compare_value);
    }

    void FloatingPointProcessor::fcmpu(u8 crfd, u8 fra, u8 frb, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const Register::FPR &a = m_fpr[fra];
        const Register::FPR &b = m_fpr[frb];
        Helper_FloatCompareUnordered(a.ps0AsDouble(), b.ps0AsDouble(), crfd, m_fpscr, msr, srr1,
                                     cr);
    }

    void FloatingPointProcessor::fcmpo(u8 crfd, u8 fra, u8 frb, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        const Register::FPR &a = m_fpr[fra];
        const Register::FPR &b = m_fpr[frb];
        Helper_FloatCompareOrdered(a.ps0AsDouble(), b.ps0AsDouble(), crfd, m_fpscr, msr, srr1, cr);
    }

    // FPSCR

    void FloatingPointProcessor::mffs(u8 frt, bool rc, Register::CR &cr, Register::MSR &msr,
                                      Register::SRR1 &srr1) {
        m_fpr[frt].setPS0(UINT64_C(0xFFF8000000000000) | m_fpscr);

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::mcrfs(u8 crfd, u8 crfa, Register::CR &cr, Register::MSR &msr,
                                       Register::SRR1 &srr1) {
        const u32 shift   = 4 * (7 - crfa);
        const u32 fpflags = (m_fpscr >> shift) & 0xF;

        // If any exception bits were read, clear them
        m_fpscr &= ~((0xF << shift) &
                     (FPSCRExceptionFlag::FPSCR_EX_FX | FPSCRExceptionFlag::FPSCR_EX_ANY_X));
        UpdateFPExceptionSummary(m_fpscr, msr, srr1);

        SET_CRF_FIELD(cr.m_crf, crfd, fpflags);
    }

    void FloatingPointProcessor::mtfsfi(u8 crfd, u8 imm, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const u32 pre_shifted_mask = 0xF0000000;
        const u32 mask             = (pre_shifted_mask >> (4 * crfd));

        m_fpscr = (m_fpscr & ~mask) | (imm >> (4 * crfd));

        UpdateFPExceptionSummary(m_fpscr, msr, srr1);

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::mtfsf(u8 fm, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                                       Register::SRR1 &srr1) {
        u32 m = 0;
        for (u32 i = 0; i < 8; i++) {
            if ((fm & (1U << i)) != 0)
                m |= (0xFU << (i * 4));
        }

        m_fpscr = (m_fpscr & ~m) | (static_cast<u32>(m_fpr[frb].ps0AsU64()) & m);

        UpdateFPExceptionSummary(m_fpscr, msr, srr1);

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::mtfsb0(u8 bt, bool rc, Register::CR &cr, Register::MSR &msr,
                                        Register::SRR1 &srr1) {
        u32 b = 0x80000000 >> bt;

        m_fpscr &= ~b;

        UpdateFPExceptionSummary(m_fpscr, msr, srr1);

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::mtfsb1(u8 bt, bool rc, Register::CR &cr, Register::MSR &msr,
                                        Register::SRR1 &srr1) {
        const u32 b = 0x80000000 >> bt;

        if ((b & FPSCRExceptionFlag::FPSCR_EX_ANY_X) != 0)
            SetFPException(m_fpscr, msr, srr1, b);
        else
            m_fpscr |= b;

        UpdateFPExceptionSummary(m_fpscr, msr, srr1);

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    // Extended

    void FloatingPointProcessor::fres(u8 frt, u8 frb, bool rc, Register::CR &cr, Register::MSR &msr,
                                      Register::SRR1 &srr1) {
        const double b = m_fpr[frb].ps0AsDouble();

        const auto compute_result = [this, frt](double value) {
            const double result = DolphinLib::ApproximateReciprocal(value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat((f32)result));
        };

        if (b == 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_ZX);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_ZE(m_fpscr) == 0)
                compute_result(b);
        } else if (DolphinLib::IsSNAN(b)) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_VE(m_fpscr) == 0)
                compute_result(b);
        } else {
            if (std::isnan(b) || std::isinf(b)) {
                FPSCR_SET_FI(m_fpscr, false);
                FPSCR_SET_FR(m_fpscr, false);
            }

            compute_result(b);
        }
    }
    void FloatingPointProcessor::frsqrte(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const double b = m_fpr[frb].ps0AsDouble();

        const auto compute_result = [this, frt](double value) {
            const double result = DolphinLib::ApproximateReciprocalSquareRoot(value);
            m_fpr[frt].setPS0(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyDouble(result));
        };

        if (b < 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSQRT);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_VE(m_fpscr) == 0)
                compute_result(b);
        } else if (b == 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_ZX);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_ZE(m_fpscr) == 0)
                compute_result(b);
        } else if (DolphinLib::IsSNAN(b)) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_VE(m_fpscr) == 0)
                compute_result(b);
        } else {
            if (std::isnan(b) || std::isinf(b)) {
                FPSCR_SET_FI(m_fpscr, false);
                FPSCR_SET_FR(m_fpscr, false);
            }

            compute_result(b);
        }
    }
    void FloatingPointProcessor::frsqrtes(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        const double b = m_fpr[frb].ps0AsDouble();

        const auto compute_result = [this, frt](double value) {
            double result = DolphinLib::ApproximateReciprocalSquareRoot(value);
            result        = ForceSingle(m_fpscr, result);
            m_fpr[frt].setPS0(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat((f32)result));
        };

        if (b < 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSQRT);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_VE(m_fpscr) == 0)
                compute_result(b);
        } else if (b == 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_ZX);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_ZE(m_fpscr) == 0)
                compute_result(b);
        } else if (DolphinLib::IsSNAN(b)) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);

            if (FPSCR_VE(m_fpscr) == 0)
                compute_result(b);
        } else {
            if (std::isnan(b) || std::isinf(b)) {
                FPSCR_SET_FI(m_fpscr, false);
                FPSCR_SET_FR(m_fpscr, false);
            }

            compute_result(b);
        }
    }
    void FloatingPointProcessor::fsel(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                      Register::MSR &msr, Register::SRR1 &srr1) {
        const auto &a = m_fpr[fra];
        const auto &b = m_fpr[frb];
        const auto &c = m_fpr[frc];

        m_fpr[frt].setPS0((a.ps0AsDouble() >= -0.0) ? c.ps0AsDouble() : b.ps0AsDouble());

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    // *** PAIRED SINGLE *** //

    // dequantize table
    const float m_dequantizeTable[] = {
        1.0 / (1ULL << 0),  1.0 / (1ULL << 1),  1.0 / (1ULL << 2),  1.0 / (1ULL << 3),
        1.0 / (1ULL << 4),  1.0 / (1ULL << 5),  1.0 / (1ULL << 6),  1.0 / (1ULL << 7),
        1.0 / (1ULL << 8),  1.0 / (1ULL << 9),  1.0 / (1ULL << 10), 1.0 / (1ULL << 11),
        1.0 / (1ULL << 12), 1.0 / (1ULL << 13), 1.0 / (1ULL << 14), 1.0 / (1ULL << 15),
        1.0 / (1ULL << 16), 1.0 / (1ULL << 17), 1.0 / (1ULL << 18), 1.0 / (1ULL << 19),
        1.0 / (1ULL << 20), 1.0 / (1ULL << 21), 1.0 / (1ULL << 22), 1.0 / (1ULL << 23),
        1.0 / (1ULL << 24), 1.0 / (1ULL << 25), 1.0 / (1ULL << 26), 1.0 / (1ULL << 27),
        1.0 / (1ULL << 28), 1.0 / (1ULL << 29), 1.0 / (1ULL << 30), 1.0 / (1ULL << 31),
        (1ULL << 32),       (1ULL << 31),       (1ULL << 30),       (1ULL << 29),
        (1ULL << 28),       (1ULL << 27),       (1ULL << 26),       (1ULL << 25),
        (1ULL << 24),       (1ULL << 23),       (1ULL << 22),       (1ULL << 21),
        (1ULL << 20),       (1ULL << 19),       (1ULL << 18),       (1ULL << 17),
        (1ULL << 16),       (1ULL << 15),       (1ULL << 14),       (1ULL << 13),
        (1ULL << 12),       (1ULL << 11),       (1ULL << 10),       (1ULL << 9),
        (1ULL << 8),        (1ULL << 7),        (1ULL << 6),        (1ULL << 5),
        (1ULL << 4),        (1ULL << 3),        (1ULL << 2),        (1ULL << 1),
    };

    // quantize table
    const float m_quantizeTable[] = {
        (1ULL << 0),        (1ULL << 1),        (1ULL << 2),        (1ULL << 3),
        (1ULL << 4),        (1ULL << 5),        (1ULL << 6),        (1ULL << 7),
        (1ULL << 8),        (1ULL << 9),        (1ULL << 10),       (1ULL << 11),
        (1ULL << 12),       (1ULL << 13),       (1ULL << 14),       (1ULL << 15),
        (1ULL << 16),       (1ULL << 17),       (1ULL << 18),       (1ULL << 19),
        (1ULL << 20),       (1ULL << 21),       (1ULL << 22),       (1ULL << 23),
        (1ULL << 24),       (1ULL << 25),       (1ULL << 26),       (1ULL << 27),
        (1ULL << 28),       (1ULL << 29),       (1ULL << 30),       (1ULL << 31),
        1.0 / (1ULL << 32), 1.0 / (1ULL << 31), 1.0 / (1ULL << 30), 1.0 / (1ULL << 29),
        1.0 / (1ULL << 28), 1.0 / (1ULL << 27), 1.0 / (1ULL << 26), 1.0 / (1ULL << 25),
        1.0 / (1ULL << 24), 1.0 / (1ULL << 23), 1.0 / (1ULL << 22), 1.0 / (1ULL << 21),
        1.0 / (1ULL << 20), 1.0 / (1ULL << 19), 1.0 / (1ULL << 18), 1.0 / (1ULL << 17),
        1.0 / (1ULL << 16), 1.0 / (1ULL << 15), 1.0 / (1ULL << 14), 1.0 / (1ULL << 13),
        1.0 / (1ULL << 12), 1.0 / (1ULL << 11), 1.0 / (1ULL << 10), 1.0 / (1ULL << 9),
        1.0 / (1ULL << 8),  1.0 / (1ULL << 7),  1.0 / (1ULL << 6),  1.0 / (1ULL << 5),
        1.0 / (1ULL << 4),  1.0 / (1ULL << 3),  1.0 / (1ULL << 2),  1.0 / (1ULL << 1),
    };

    template <typename SType> SType ScaleAndClamp(double ps, u32 st_scale) {
        const float conv_ps = float(ps) * m_quantizeTable[st_scale];
        constexpr float min = float(std::numeric_limits<SType>::min());
        constexpr float max = float(std::numeric_limits<SType>::max());

        return SType(std::clamp(conv_ps, min, max));
    }

    template <typename T> static T ReadUnpaired(Buffer &storage, u32 addr);

    template <> u8 ReadUnpaired<u8>(Buffer &storage, u32 addr) {
        return storage.get<u8>(addr - 0x80000000);
    }

    template <> u16 ReadUnpaired<u16>(Buffer &storage, u32 addr) {
        return storage.get<u16>(addr - 0x80000000);
    }

    template <> u32 ReadUnpaired<u32>(Buffer &storage, u32 addr) {
        return storage.get<u32>(addr - 0x80000000);
    }

    template <typename T> static std::pair<T, T> ReadPair(Buffer &storage, u32 addr);

    template <> std::pair<u8, u8> ReadPair<u8>(Buffer &storage, u32 addr) {
        const u16 val = std::byteswap(storage.get<u16>(addr - 0x80000000));
        return {u8(val >> 8), u8(val)};
    }

    template <> std::pair<u16, u16> ReadPair<u16>(Buffer &storage, u32 addr) {
        const u32 val = std::byteswap(storage.get<u32>(addr - 0x80000000));
        return {u16(val >> 16), u16(val)};
    }

    template <> std::pair<u32, u32> ReadPair<u32>(Buffer &storage, u32 addr) {
        const u64 val = std::byteswap(storage.get<u64>(addr - 0x80000000));
        return {u32(val >> 32), u32(val)};
    }

    template <typename T> static void WriteUnpaired(Buffer &storage, T val, u32 addr);

    template <> void WriteUnpaired<u8>(Buffer &storage, u8 val, u32 addr) {
        storage.set<u8>(addr - 0x80000000, val);
    }

    template <> void WriteUnpaired<u16>(Buffer &storage, u16 val, u32 addr) {
        storage.set<u16>(addr - 0x80000000, std::byteswap(val));
    }

    template <> void WriteUnpaired<u32>(Buffer &storage, u32 val, u32 addr) {
        storage.set<u32>(addr - 0x80000000, std::byteswap(val));
    }

    template <typename T> static void WritePair(Buffer &storage, T val1, T val2, u32 addr);

    template <> void WritePair<u8>(Buffer &storage, u8 val1, u8 val2, u32 addr) {
        storage.set<u16>(addr - 0x80000000, std::byteswap((u16{val1} << 8) | u16{val2}));
    }

    template <> void WritePair<u16>(Buffer &storage, u16 val1, u16 val2, u32 addr) {
        storage.set<u32>(addr - 0x80000000, std::byteswap((u32{val1} << 16) | u32{val2}));
    }

    template <> void WritePair<u32>(Buffer &storage, u32 val1, u32 val2, u32 addr) {
        storage.set<u64>(addr - 0x80000000, std::byteswap((u64{val1} << 32) | u64{val2}));
    }

    template <typename T>
    void QuantizeAndStore(Buffer &storage, double ps0, double ps1, u32 addr, u32 instW,
                          u32 st_scale) {
        using U = std::make_unsigned_t<T>;

        const U conv_ps0 = U(ScaleAndClamp<T>(ps0, st_scale));
        if (instW) {
            WriteUnpaired<U>(storage, conv_ps0, addr);
        } else {
            const U conv_ps1 = U(ScaleAndClamp<T>(ps1, st_scale));
            WritePair<U>(storage, conv_ps0, conv_ps1, addr);
        }
    }

    void FloatingPointProcessor::helperQuantize(Buffer &storage, u32 addr, u32 instI, u32 instRS,
                                                u32 instW) {
        if (!MemoryContainsVAddress(storage, addr)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }

        // TODO: Get actual type and scale from qprs
        const QuantizeType st_type = GQR_ST_TYPE(m_gqr[instI]);
        const u32 st_scale         = GQR_ST_SCALE(m_gqr[instI]);

        const double ps0 = m_fpr[instRS].ps0AsDouble();
        const double ps1 = m_fpr[instRS].ps1AsDouble();

        switch (st_type) {
        case QUANTIZE_FLOAT: {
            const u64 integral_ps0 = std::bit_cast<u64>(ps0);
            const u32 conv_ps0     = ConvertToSingleFTZ(integral_ps0);

            if (instW != 0) {
                WriteUnpaired<u32>(storage, conv_ps0, addr);
            } else {
                const u64 integral_ps1 = std::bit_cast<u64>(ps1);
                const u32 conv_ps1     = ConvertToSingleFTZ(integral_ps1);

                WritePair<u32>(storage, conv_ps0, conv_ps1, addr);
            }
            break;
        }

        case QUANTIZE_U8:
            QuantizeAndStore<u8>(storage, ps0, ps1, addr, instW, st_scale);
            break;

        case QUANTIZE_U16:
            QuantizeAndStore<u16>(storage, ps0, ps1, addr, instW, st_scale);
            break;

        case QUANTIZE_S8:
            QuantizeAndStore<s8>(storage, ps0, ps1, addr, instW, st_scale);
            break;

        case QUANTIZE_S16:
            QuantizeAndStore<s16>(storage, ps0, ps1, addr, instW, st_scale);
            break;

        case QUANTIZE_INVALID1:
        case QUANTIZE_INVALID2:
        case QUANTIZE_INVALID3:
            TOOLBOX_ERROR("(PS dequantize) unknown type to read");
            break;
        }
    }

    template <typename T>
    std::pair<double, double> LoadAndDequantize(Buffer &storage, u32 addr, u32 instW,
                                                u32 ld_scale) {
        using U = std::make_unsigned_t<T>;

        float ps0, ps1;
        if (instW != 0) {
            const U value = ReadUnpaired<U>(storage, addr);
            ps0           = float(T(value)) * m_dequantizeTable[ld_scale];
            ps1           = 1.0f;
        } else {
            const auto [first, second] = ReadPair<U>(storage, addr);
            ps0                        = float(T(first)) * m_dequantizeTable[ld_scale];
            ps1                        = float(T(second)) * m_dequantizeTable[ld_scale];
        }
        // ps0 and ps1 always contain finite and normal numbers. So we can just cast them to double
        return {static_cast<double>(ps0), static_cast<double>(ps1)};
    }

    void FloatingPointProcessor::helperDequantize(Buffer &storage, u32 addr, u32 instI, u32 instRD,
                                                  u32 instW) {
        if (!MemoryContainsVAddress(storage, addr)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }

        // TODO: Get actual type and scale from qprs
        const QuantizeType ld_type = GQR_LD_TYPE(m_gqr[instI]);
        const u32 ld_scale         = GQR_LD_SCALE(m_gqr[instI]);

        double ps0 = 0.0;
        double ps1 = 0.0;

        switch (ld_type) {
        case QUANTIZE_FLOAT:
            if (instW != 0) {
                const u32 value = ReadUnpaired<u32>(storage, addr);
                ps0             = std::bit_cast<double>(std::byteswap(ConvertToDouble(value)));
                ps1             = 1.0;
            } else {
                const auto [first, second] = ReadPair<u32>(storage, addr);
                ps0 = std::bit_cast<double>(std::byteswap(ConvertToDouble(first)));
                ps1 = std::bit_cast<double>(std::byteswap(ConvertToDouble(second)));
            }
            break;
        case QUANTIZE_U8:
            std::tie(ps0, ps1) = LoadAndDequantize<u8>(storage, addr, instW, ld_scale);
            break;

        case QUANTIZE_U16:
            std::tie(ps0, ps1) = LoadAndDequantize<u16>(storage, addr, instW, ld_scale);
            break;

        case QUANTIZE_S8:
            std::tie(ps0, ps1) = LoadAndDequantize<s8>(storage, addr, instW, ld_scale);
            break;

        case QUANTIZE_S16:
            std::tie(ps0, ps1) = LoadAndDequantize<s16>(storage, addr, instW, ld_scale);
            break;

        case QUANTIZE_INVALID1:
        case QUANTIZE_INVALID2:
        case QUANTIZE_INVALID3:
            TOOLBOX_ERROR("(PS dequantize) unknown type to read");
            ps0 = 0.0;
            ps1 = 0.0;
            break;
        }

        m_fpr[instRD].setBoth(ps0, ps1);
    }

    void FloatingPointProcessor::ps_l(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32],
                                      Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_l, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + d);
        helperDequantize(storage, destination, i, frt, w);
    }

    void FloatingPointProcessor::ps_lu(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_lu, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + d);
        helperDequantize(storage, destination, i, frt, w);
        gpr[ra] += d;
    }

    void FloatingPointProcessor::ps_lx(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_lx, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + gpr[rb]);
        helperQuantize(storage, destination, ix, frt, wx);
    }

    void FloatingPointProcessor::ps_lux(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32],
                                        Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_lux, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + gpr[rb]);
        helperQuantize(storage, destination, ix, frt, wx);
        gpr[ra] += gpr[rb];
    }

    void FloatingPointProcessor::ps_st(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32],
                                       Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_st, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + d);
        helperQuantize(storage, destination, i, frt, w);
    }

    void FloatingPointProcessor::ps_stu(u8 frt, s16 d, u8 i, u8 ra, u8 w, Register::GPR gpr[32],
                                        Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_stu, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + d);
        helperQuantize(storage, destination, i, frt, w);
        gpr[ra] += d;
    }

    void FloatingPointProcessor::ps_stx(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32],
                                        Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_stx, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + gpr[rb]);
        helperQuantize(storage, destination, ix, frt, wx);
    }

    void FloatingPointProcessor::ps_stux(u8 frt, u8 ix, u8 ra, u8 rb, u8 wx, Register::GPR gpr[32],
                                         Buffer &storage) {
        if (!IsRegValid(frt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, ps_stux, "Invalid registers detected!"));
            return;
        }
        u32 destination = static_cast<s32>(gpr[ra] + gpr[rb]);
        helperQuantize(storage, destination, ix, frt, wx);
        gpr[ra] += gpr[rb];
    }

    void FloatingPointProcessor::ps_cmpo0(u8 bf, u8 fra, u8 frb, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        Helper_FloatCompareOrdered(m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble(), bf, m_fpscr,
                                   msr, srr1, cr);
    }

    void FloatingPointProcessor::ps_cmpu0(u8 bf, u8 fra, u8 frb, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        Helper_FloatCompareUnordered(m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble(), bf,
                                     m_fpscr, msr, srr1, cr);
    }

    void FloatingPointProcessor::ps_cmpo1(u8 bf, u8 fra, u8 frb, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        Helper_FloatCompareOrdered(m_fpr[fra].ps1AsDouble(), m_fpr[frb].ps1AsDouble(), bf, m_fpscr,
                                   msr, srr1, cr);
    }

    void FloatingPointProcessor::ps_cmpu1(u8 bf, u8 fra, u8 frb, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        Helper_FloatCompareUnordered(m_fpr[fra].ps1AsDouble(), m_fpr[frb].ps1AsDouble(), bf,
                                     m_fpscr, msr, srr1, cr);
    }

    void FloatingPointProcessor::ps_mr(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                       Register::MSR &msr, Register::SRR1 &srr1) {
        m_fpr[frt] = m_fpr[frb];

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_abs(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        m_fpr[frt].setBoth(m_fpr[frb].ps0AsU64() & ~(UINT64_C(1) << 63),
                           m_fpr[frb].ps1AsU64() & ~(UINT64_C(1) << 63));

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_neg(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        m_fpr[frt].setBoth(m_fpr[frb].ps0AsU64() ^ (UINT64_C(1) << 63),
                           m_fpr[frb].ps1AsU64() ^ (UINT64_C(1) << 63));

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_nabs(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        m_fpr[frt].setBoth(m_fpr[frb].ps0AsU64() | (UINT64_C(1) << 63),
                           m_fpr[frb].ps1AsU64() | (UINT64_C(1) << 63));

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_add(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(
            m_fpscr,
            NI_add(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble()).value);
        const float ps1 = ForceSingle(
            m_fpscr,
            NI_add(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(), m_fpr[frb].ps1AsDouble()).value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_sub(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(
            m_fpscr,
            NI_sub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble()).value);
        const float ps1 = ForceSingle(
            m_fpscr,
            NI_sub(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(), m_fpr[frb].ps1AsDouble()).value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_mul(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                      Force25Bit(m_fpr[frb].ps0AsDouble()))
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                      Force25Bit(m_fpr[frb].ps1AsDouble()))
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_muls0(u8 frt, u8 fra, u8 frc, bool rc, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                      Force25Bit(m_fpr[frc].ps0AsDouble()))
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                      Force25Bit(m_fpr[frc].ps0AsDouble()))
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_muls1(u8 frt, u8 fra, u8 frc, bool rc, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                      Force25Bit(m_fpr[frc].ps1AsDouble()))
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_mul(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                      Force25Bit(m_fpr[frc].ps1AsDouble()))
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_div(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(
            m_fpscr,
            NI_div(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps0AsDouble()).value);
        const float ps1 = ForceSingle(
            m_fpscr,
            NI_div(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(), m_fpr[frb].ps1AsDouble()).value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_msub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps0AsDouble()),
                                                       m_fpr[frb].ps0AsDouble())
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps1AsDouble()),
                                                       m_fpr[frb].ps1AsDouble())
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_madds0(u8 frt, u8 fra, u8 frc, u8 frb, bool rc,
                                           Register::CR &cr, Register::MSR &msr,
                                           Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps0AsDouble()),
                                                       m_fpr[frb].ps0AsDouble())
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps0AsDouble()),
                                                       m_fpr[frb].ps1AsDouble())
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_madds1(u8 frt, u8 fra, u8 frc, u8 frb, bool rc,
                                           Register::CR &cr, Register::MSR &msr,
                                           Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps1AsDouble()),
                                                       m_fpr[frb].ps0AsDouble())
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps1AsDouble()),
                                                       m_fpr[frb].ps1AsDouble())
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_madd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps0AsDouble()),
                                                       m_fpr[frb].ps0AsDouble())
                                                   .value);
        const float ps1 = ForceSingle(m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                                                       Force25Bit(m_fpr[frc].ps1AsDouble()),
                                                       m_fpr[frb].ps1AsDouble())
                                                   .value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_nmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        const float tmp0 = ForceSingle(
            m_fpscr, NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                             Force25Bit(m_fpr[frc].ps0AsDouble()), m_fpr[frb].ps0AsDouble())
                         .value);
        const float tmp1 = ForceSingle(
            m_fpscr, NI_msub(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                             Force25Bit(m_fpr[frc].ps1AsDouble()), m_fpr[frb].ps1AsDouble())
                         .value);

        const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
        const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_nmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                          Register::MSR &msr, Register::SRR1 &srr1) {
        const float tmp0 = ForceSingle(
            m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(),
                             Force25Bit(m_fpr[frc].ps0AsDouble()), m_fpr[frb].ps0AsDouble())
                         .value);
        const float tmp1 = ForceSingle(
            m_fpscr, NI_madd(m_fpscr, msr, srr1, m_fpr[fra].ps1AsDouble(),
                             Force25Bit(m_fpr[frc].ps1AsDouble()), m_fpr[frb].ps1AsDouble())
                         .value);

        const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
        const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_sum0(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(
            m_fpscr,
            NI_add(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps1AsDouble()).value);
        const float ps1 = ForceSingle(m_fpscr, m_fpr[frc].ps1AsDouble());

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_sum1(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                         Register::MSR &msr, Register::SRR1 &srr1) {
        const float ps0 = ForceSingle(m_fpscr, m_fpr[frc].ps1AsDouble());
        const float ps1 = ForceSingle(
            m_fpscr,
            NI_add(m_fpscr, msr, srr1, m_fpr[fra].ps0AsDouble(), m_fpr[frb].ps1AsDouble()).value);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat(ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_res(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const double a = m_fpr[frb].ps0AsDouble();
        const double b = m_fpr[frb].ps1AsDouble();

        const auto compute_result = [this, frt](double value) {
            const double result = DolphinLib::ApproximateReciprocal(value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat((f32)result));
        };

        if (a == 0.0 || b == 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_ZX);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);
        }

        if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isnan(b)) {
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);
        }

        if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b)) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
        }

        const double ps0 = DolphinLib::ApproximateReciprocal(a);
        const double ps1 = DolphinLib::ApproximateReciprocal(b);

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat((float)ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_rsqrte(u8 frt, u8 frb, bool rc, Register::CR &cr,
                                           Register::MSR &msr, Register::SRR1 &srr1) {
        const double a = m_fpr[frb].ps0AsDouble();
        const double b = m_fpr[frb].ps1AsDouble();

        const auto compute_result = [this, frt](double value) {
            const double result = DolphinLib::ApproximateReciprocal(value);
            m_fpr[frt].fill(result);
            FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat((f32)result));
        };

        if (a == 0.0 || b == 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_ZX);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);
        }

        if (a < 0.0 || b < 0.0) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSQRT);
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);
        }

        if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isnan(b)) {
            FPSCR_SET_FI(m_fpscr, false);
            FPSCR_SET_FR(m_fpscr, false);
        }

        if (DolphinLib::IsSNAN(a) || DolphinLib::IsSNAN(b)) {
            SetFPException(m_fpscr, msr, srr1, FPSCRExceptionFlag::FPSCR_EX_VXSNAN);
        }

        const double ps0 = ForceSingle(m_fpscr, DolphinLib::ApproximateReciprocalSquareRoot(a));
        const double ps1 = ForceSingle(m_fpscr, DolphinLib::ApproximateReciprocalSquareRoot(b));

        m_fpr[frt].setBoth(ps0, ps1);
        FPSCR_SET_FPRT(m_fpscr, (u32)DolphinLib::ClassifyFloat((float)ps0));

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_sel(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr,
                                        Register::MSR &msr, Register::SRR1 &srr1) {
        const auto &a = m_fpr[fra];
        const auto &b = m_fpr[frb];
        const auto &c = m_fpr[frc];

        m_fpr[frt].setBoth((a.ps0AsDouble() >= -0.0) ? c.ps0AsDouble() : b.ps0AsDouble(),
                           (a.ps1AsDouble() >= -0.0) ? c.ps1AsDouble() : b.ps1AsDouble());

        // This is a binary instruction. Does not alter FPSCR
        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_merge00(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                            Register::MSR &msr, Register::SRR1 &srr1) {
        const auto &a = m_fpr[fra];
        const auto &b = m_fpr[frb];

        m_fpr[frt].setBoth(a.ps0AsDouble(), b.ps0AsDouble());

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_merge01(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                            Register::MSR &msr, Register::SRR1 &srr1) {
        const auto &a = m_fpr[fra];
        const auto &b = m_fpr[frb];

        m_fpr[frt].setBoth(a.ps0AsDouble(), b.ps1AsDouble());

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_merge10(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                            Register::MSR &msr, Register::SRR1 &srr1) {
        const auto &a = m_fpr[fra];
        const auto &b = m_fpr[frb];

        m_fpr[frt].setBoth(a.ps1AsDouble(), b.ps0AsDouble());

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

    void FloatingPointProcessor::ps_merge11(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr,
                                            Register::MSR &msr, Register::SRR1 &srr1) {
        const auto &a = m_fpr[fra];
        const auto &b = m_fpr[frb];

        m_fpr[frt].setBoth(a.ps1AsDouble(), b.ps1AsDouble());

        if (rc) {
            UpdateCR1(cr, m_fpscr);
        }
    }

}  // namespace Toolbox::Interpreter