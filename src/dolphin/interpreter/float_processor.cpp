#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

namespace Toolbox::Interpreter {

    void FloatingPointProcessor::lfs(u8 frt, s16 d, u8 ra) {}
    void FloatingPointProcessor::lfsu(u8 frt, s16 d, u8 ra) {}
    void FloatingPointProcessor::lfsx(u8 frt, u8 ra, u8 rb) {}
    void FloatingPointProcessor::lfsux(u8 frt, u8 ra, u8 rb) {}

    void FloatingPointProcessor::lfd(u8 frt, s16 d, u8 ra) {}
    void FloatingPointProcessor::lfdu(u8 frt, s16 d, u8 ra) {}
    void FloatingPointProcessor::lfdx(u8 frt, u8 ra, u8 rb) {}
    void FloatingPointProcessor::lfdux(u8 frt, u8 ra, u8 rb) {}

    void FloatingPointProcessor::stfs(u8 frs, s16 d, u8 ra) {}
    void FloatingPointProcessor::stfsu(u8 frs, s16 d, u8 ra) {}
    void FloatingPointProcessor::stfsx(u8 frs, u8 ra, u8 rb) {}
    void FloatingPointProcessor::stfsux(u8 frs, u8 ra, u8 rb) {}

    void FloatingPointProcessor::stfd(u8 frs, s16 d, u8 ra) {}
    void FloatingPointProcessor::stfdu(u8 frs, s16 d, u8 ra) {}
    void FloatingPointProcessor::stfdx(u8 frs, u8 ra, u8 rb) {}
    void FloatingPointProcessor::stfdux(u8 frs, u8 ra, u8 rb) {}

    void FloatingPointProcessor::stfiwx(u8 frs, u8 ra, u8 rb) {}

    // Move

    void FloatingPointProcessor::fmr(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fabs(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fneg(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fnabs(u8 frt, u8 frb, bool rc, Register::CR &cr) {}

    // Math

    void FloatingPointProcessor::fadd(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fadds(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fsub(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fsubs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fmul(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fmuls(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fdiv(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fdivs(u8 frt, u8 fra, u8 frb, bool rc, Register::CR &cr) {}

    void FloatingPointProcessor::fmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {
    }
    void FloatingPointProcessor::fmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {
    }
    void FloatingPointProcessor::fnmadd(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {
    }
    void FloatingPointProcessor::fnmadds(u8 frt, u8 fra, u8 frc, u8 frb, bool rc,
                                         Register::CR &cr) {}
    void FloatingPointProcessor::fnmsub(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {
    }
    void FloatingPointProcessor::fnmsubs(u8 frt, u8 fra, u8 frc, u8 frb, bool rc,
                                         Register::CR &cr) {}

    // Rounding and conversion

    void FloatingPointProcessor::fsrp(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fctid(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fdtidz(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fcfid(u8 frt, u8 frb, bool rc, Register::CR &cr) {}

    // Compare

    void FloatingPointProcessor::fcmpu(u8 bf, u8 fra, u8 frb, Register::CR &cr) {}
    void FloatingPointProcessor::fcmpo(u8 bf, u8 fra, u8 frb, Register::CR &cr) {}

    // FPSCR

    void FloatingPointProcessor::mffs(u8 frt, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::mcrfs(u8 bf, u8 bfa, Register::CR &cr) {}
    void FloatingPointProcessor::mtfsfi(u8 bf, u8 u, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::mtfsf(u8 flm, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::mtfsb0(u8 bt, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::mtfsb1(u8 bt, bool rc, Register::CR &cr) {}

    // Extended

    void FloatingPointProcessor::fsqrt(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fsqrts(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fre(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fres(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::frsqrte(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::frsqrtes(u8 frt, u8 frb, bool rc, Register::CR &cr) {}
    void FloatingPointProcessor::fsel(u8 frt, u8 fra, u8 frc, u8 frb, bool rc, Register::CR &cr) {}

}  // namespace Toolbox::Interpreter