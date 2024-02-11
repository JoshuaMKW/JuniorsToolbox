#pragma once

#include "core/memory.hpp"
#include "registers.hpp"
#include "serial.hpp"

namespace Toolbox::Interpreter {

    class BranchProcessor {
    public:
        BranchProcessor() = default;

        void process(const PPC::BForm &instr, Register::PC &pc);
        void process(const PPC::IForm &instr, Register::PC &pc);
        void process(const PPC::SCForm &instr, Register::PC &pc);
        void process(const PPC::XLForm &instr, Register::PC &pc);

    private:
        void b(s32 target_addr, bool aa, bool lk, Register::PC &pc);
        void bc(s32 target_addr, u8 bi, u8 bo, bool aa, bool lk, Register::PC &pc);
        void bclr(u8 bo, u8 bi, u8 bh, bool lk, Register::PC &pc);
        void bcctr(u8 bo, u8 bi, u8 bh, bool lk, Register::PC &pc);

        void sc(u8 lev, Register::PC &pc);

        void crand(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crandc(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void creqv(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void cror(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crorc(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crnand(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crnor(u8 bt, u8 ba, u8 bb, Register::PC &pc);
        void crxor(u8 bt, u8 ba, u8 bb, Register::PC &pc);

        Register::CR m_cr;
        Register::LR m_lr;
        Register::CTR m_ctr;
    };

    class FixedPointProcessor {
    public:
        FixedPointProcessor() = default;
        void process(const PPC::DForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::DSForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::MForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::MDForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::MDSForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::XForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::XFXForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::XOForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);
        void process(const PPC::XSForm &instr, Register::PC &pc, Buffer &memory, Register::CR &cr,
                     Register::XER &xer);

    private:
        // Memory
        void lbz(u8 rt, u8 ra, s16 d, Buffer &memory);
        void lbzu(u8 rt, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void lbzx(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void lbzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void lhz(u8 rt, u8 ra, s16 d, Buffer &memory);
        void lhzu(u8 rt, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void lhzx(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void lhzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);
        void lha(u8 rt, u8 ra, s16 d, Buffer &memory);
        void lhau(u8 rt, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void lhax(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void lhaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void lwz(u8 rt, u8 ra, s16 d, Buffer &memory);
        void lwzu(u8 rt, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void lwzx(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void lwzux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);
        void lwa(u8 rt, u8 ra, s16 d, Buffer &memory);
        void lwax(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void lwaux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void ld(u8 rt, u8 ra, s16 d, Buffer &memory);
        void ldu(u8 rt, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void ldx(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void ldux(u8 rt, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void stb(u8 rs, u8 ra, s16 d, Buffer &memory);
        void stbu(u8 rs, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void stbx(u8 rs, u8 ra, u8 rb, Buffer &memory);
        void stbux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void sth(u8 rs, u8 ra, s16 d, Buffer &memory);
        void sthu(u8 rs, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void sthx(u8 rs, u8 ra, u8 rb, Buffer &memory);
        void sthux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void stw(u8 rs, u8 ra, s16 d, Buffer &memory);
        void stwu(u8 rs, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void stwx(u8 rs, u8 ra, u8 rb, Buffer &memory);
        void stwux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void std(u8 rs, u8 ra, s16 d, Buffer &memory);
        void stdu(u8 rs, u8 ra, s16 d, Register::PC &pc, Buffer &memory);
        void stdx(u8 rs, u8 ra, u8 rb, Buffer &memory);
        void stdux(u8 rs, u8 ra, u8 rb, Register::PC &pc, Buffer &memory);

        void lhbrx(u8 rt, u8 ra, u8 rb, Buffer &memory);
        void lwbrx(u8 rt, u8 ra, u8 rb, Buffer &memory);

        void sthbrx(u8 rs, u8 ra, u8 rb, Buffer &memory);
        void stwbrx(u8 rs, u8 ra, u8 rb, Buffer &memory);

        void lmw(u8 rt, u8 ra, s16 d, Buffer &memory);
        void stmw(u8 rs, u8 ra, s16 d, Buffer &memory);

        void lswi(u8 rt, u8 ra, u8 nb, Buffer &memory);
        void lswx(u8 rt, u8 ra, u8 rb, Buffer &memory);

        void stswi(u8 rt, u8 ra, u8 nb, Buffer &memory);
        void stswx(u8 rt, u8 ra, u8 rb, Buffer &memory);

        // Math

        void addi(u8 rt, u8 ra, s16 si);
        void addis(u8 rt, u8 ra, s16 si);
        void add(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void addic(u8 rt, u8 ra, s16 si, bool rc, Register::CR &cr, Register::XER &xer);
        void subf(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void subfic(u8 rt, u8 ra, s16 si, Register::XER &xer);
        void addc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void subfc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void adde(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void subfe(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void addme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void subfme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void addze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void subfze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr, Register::XER &xer);

        void mulli(u8 rt, u8 ra, s16 si);
        void mulld(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void mullw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void mullhd(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhdu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhw(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);
        void mullhwu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr);

        void divd(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void divw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void divdu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);
        void divwu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr, Register::XER &xer);

        // Compare

        void cmpi(u8 bf, bool l, u8 ra, s16 si, Register::CR &cr);
        void cmp(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr);
        void cmpli(u8 bf, bool l, u8 ra, u16 ui, Register::CR &cr);
        void cmpl(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr);

        // Trap

        void tdi(u8 to, u8 ra, s16 si);
        void twi(u8 to, u8 ra, s16 si);
        void td(u8 to, u8 ra, u8 rb);
        void tw(u8 to, u8 ra, u8 rb);

        // Logic

        void andi(u8 ra, u8 rs, u16 ui, Register::CR &cr);
        void andis(u8 ra, u8 rs, u16 ui, Register::CR &cr);
        void ori(u8 ra, u8 rs, u16 ui);
        void oris(u8 ra, u8 rs, u16 ui);
        void xori(u8 ra, u8 rs, u16 ui);
        void and_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void or_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void xor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void nand_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void nor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void eqv_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void andc_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void orc_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);

        void extsb(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void extsh(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void extsw(u8 ra, u8 rs, bool rc, Register::CR &cr);

        void cntlzd(u8 ra, u8 rs, bool rc, Register::CR &cr);
        void cntlzw(u8 ra, u8 rs, bool rc, Register::CR &cr);

        void popcntb(u8 ra, u8 rs);

        // Rotate | shift

        void rldicl(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr);
        void rldicr(u8 ra, u8 rs, u8 sh, u8 me, bool rc, Register::CR &cr);
        void rldic(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr);
        void rlwinm(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr);
        void rldcl(u8 ra, u8 rs, u8 rb, u8 mb, bool rc, Register::CR &cr);
        void rldcr(u8 ra, u8 rs, u8 rb, u8 me, bool rc, Register::CR &cr);
        void rlwnm(u8 ra, u8 rs, u8 rb, u8 mb, u8 me, bool rc, Register::CR &cr);
        void rldimi(u8 ra, u8 rs, u8 sh, u8 mb, bool rc, Register::CR &cr);
        void rlwimi(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr);

        void sld(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void slw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void srd(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void srw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr);
        void sradi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr, Register::XER &xer);
        void srawi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr, Register::XER &xer);
        void srad(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr, Register::XER &xer);
        void sraw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr, Register::XER &xer);

        // SPRs

        void mtspr(u16 spr, u8 rs, Register::XER &xer, Register::LR &lr, Register::CTR &ctr);
        void mfspr(u16 spr, u8 rt, const Register::XER &xer, const Register::LR &lr,
                   const Register::CTR &ctr);
        void mtcrf(u16 fxm, u8 rs, Register::CR &cr);
        void mfcr(u8 rt, const Register::CR &cr);

        Register::GPR m_gpr[32];
    };

}  // namespace Toolbox::Interpreter