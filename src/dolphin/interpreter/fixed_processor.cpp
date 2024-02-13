#pragma once

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

namespace Toolbox::Interpreter {

    // Memory
    void FixedPointProcessor::lbz(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lbzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lbzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lbzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::lhz(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lhzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lhzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lhzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lha(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lhau(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lhax(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lhaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::lwz(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lwzu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lwzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lwzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::lwa(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::lwax(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lwaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::ld(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::ldu(u8 rt, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::ldx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::ldux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::stb(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stbu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::stbx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stbux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::sth(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::sthu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::sthx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::sthux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::stw(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stwu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::stwx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stwux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::std(u8 rs, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stdu(u8 rs, s16 d, u8 ra, Register::PC &pc, Buffer &storage) {}
    void FixedPointProcessor::stdx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stdux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &storage) {}

    void FixedPointProcessor::lhbrx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::lwbrx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}

    void FixedPointProcessor::sthbrx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}
    void FixedPointProcessor::stwbrx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}

    void FixedPointProcessor::lmw(u8 rt, s16 d, u8 ra, Buffer &storage) {}
    void FixedPointProcessor::stmw(u8 rs, s16 d, u8 ra, Buffer &storage) {}

    void FixedPointProcessor::lswi(u8 rt, u8 ra, u8 nb, Buffer &storage) {}
    void FixedPointProcessor::lswx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}

    void FixedPointProcessor::stswi(u8 rt, u8 ra, u8 nb, Buffer &storage) {}
    void FixedPointProcessor::stswx(u8 rt, u8 ra, u8 rb, Buffer &storage) {}

    // Math

    void FixedPointProcessor::addi(u8 rt, u8 ra, s16 si) {}
    void FixedPointProcessor::addis(u8 rt, u8 ra, s16 si) {}
    void FixedPointProcessor::add(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::addic(u8 rt, u8 ra, s16 si, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subf(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfic(u8 rt, u8 ra, s16 si) {}
    void FixedPointProcessor::addc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::adde(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfe(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::addme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::addze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::subfze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::mulli(u8 rt, u8 ra, s16 si) {}
    void FixedPointProcessor::mulld(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhd(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhdu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhw(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::mullhwu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::divd(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::divw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::divdu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::divwu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {}

    // Compare

    void FixedPointProcessor::cmpi(u8 bf, bool l, u8 ra, s16 si, Register::CR &cr) {}
    void FixedPointProcessor::cmp(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr) {}
    void FixedPointProcessor::cmpli(u8 bf, bool l, u8 ra, u16 ui, Register::CR &cr) {}
    void FixedPointProcessor::cmpl(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr) {}

    // Trap

    void FixedPointProcessor::tdi(u8 to, u8 ra, s16 si) {}
    void FixedPointProcessor::twi(u8 to, u8 ra, s16 si) {}
    void FixedPointProcessor::td(u8 to, u8 ra, u8 rb) {}
    void FixedPointProcessor::tw(u8 to, u8 ra, u8 rb) {}

    // Logic

    void FixedPointProcessor::andi(u8 ra, u8 rs, u16 ui, Register::CR &cr) {}
    void FixedPointProcessor::andis(u8 ra, u8 rs, u16 ui, Register::CR &cr) {}
    void FixedPointProcessor::ori(u8 ra, u8 rs, u16 ui) {}
    void FixedPointProcessor::oris(u8 ra, u8 rs, u16 ui) {}
    void FixedPointProcessor::xori(u8 ra, u8 rs, u16 ui) {}
    void FixedPointProcessor::and_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::or_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::xor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::nand_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::nor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::eqv_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::andc_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::orc_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::extsb(u8 ra, u8 rs, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::extsh(u8 ra, u8 rs, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::extsw(u8 ra, u8 rs, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::cntlzd(u8 ra, u8 rs, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::cntlzw(u8 ra, u8 rs, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::popcntb(u8 ra, u8 rs) {}

    // Rotate | shift

    void FixedPointProcessor::rldicl(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldicr(u8 ra, u8 rs, u8 sh, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldic(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rlwinm(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldcl(u8 ra, u8 rs, u8 rb, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldcr(u8 ra, u8 rs, u8 rb, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rlwnm(u8 ra, u8 rs, u8 rb, u8 mb, u8 me, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rldimi(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::rlwimi(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr) {}

    void FixedPointProcessor::sld(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::slw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::srd(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::srw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::sradi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::srawi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::srad(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}
    void FixedPointProcessor::sraw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {}

    // SPRs

    void FixedPointProcessor::mtspr(SPRType spr, u8 rs, Register::LR &lr, Register::CTR &ctr) {}
    void FixedPointProcessor::mfspr(SPRType spr, u8 rt, const Register::LR &lr,
                                    const Register::CTR &ctr) {}
    void FixedPointProcessor::mtcrf(u16 fxm, u8 rs, Register::CR &cr) {}
    void FixedPointProcessor::mfcr(u8 rt, const Register::CR &cr) {}
    void FixedPointProcessor::mtmsrd(u8 rs, bool l, Register::MSR &msr) {}
    void FixedPointProcessor::mfmsr(u8 rs, const Register::MSR &msr) {}

    // Extended

    void FixedPointProcessor::mtocrf(u8 fxm, u8 rs, Register::CR &cr) {}
    void FixedPointProcessor::mfocrf(u8 fxm, u8 rt, const Register::CR &cr) {}

}  // namespace Toolbox::Interpreter