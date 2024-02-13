/*
 * For further details, read here (page 17+)
 * https://arcb.csc.ncsu.edu/~mueller/cluster/ps3/SDK3.0/docs/arch/PPC_Vers202_Book1_public.pdf
 *
 * AA (30)
 * Absolute Address bit.
 * 0 The immediate field represents an
 * address relative to the current instruction
 * address. For I-form branches the effec-
 * tive address of the branch target is the
 * sum of the LI field sign-extended to 64 bits
 * and the address of the branch instruc-
 * tion. For B-form branches the effective
 * address of the branch target is the sum of
 * the BD field sign-extended to 64 bits and
 * the address of the branch instruction.
 * 1 The immediate field represents an abso-
 * lute address. For I-form branches the
 * effective address of the branch target is
 * the LI field sign-extended to 64 bits. For
 * B-form branches the effective address of
 * the branch target is the BD field
 * sign-extended to 64 bits.
 *
 * BA (11:15)
 * Field used to specify a bit in the CR to be used as
 * a source.
 *
 * BB (16:20)
 * Field used to specify a bit in the CR to be used as
 * a source.
 *
 * BD (16:29)
 * Immediate field used to specify a 14-bit signed
 * two’s complement branch displacement which is
 * concatenated on the right with 0b00 and
 * sign-extended to 64 bits.
 *
 * BF (6:8)
 * Field used to specify one of the CR fields or one of
 * the FPSCR fields to be used as a target.
 *
 * BFA (11:13)
 * Field used to specify one of the CR fields or one of
 * the FPSCR fields to be used as a source.
 *
 * BH (19:20)
 * Field used to specify a hint in the Branch Condi-
 * tional to Link Register and Branch Conditional to
 * Count Register instructions. The encoding is
 * described in Section 2.4.1, “Branch Instructions”
 * on page 20.
 *
 * BI (11:15)
 * Field used to specify a bit in the CR to be tested by
 * a Branch Conditional instruction.
 *
 * BO (6:10)
 * Field used to specify options for the Branch Condi-
 * tional instructions. The encoding is described in
 * Section 2.4.1, “Branch Instructions” on page 20.
 *
 * BT (6:10)
 * Field used to specify a bit in the CR or in the
 * FPSCR to be used as a target.
 *
 * D (16:31)
 * Immediate field used to specify a 16-bit signed
 * two’s complement integer which is sign-extended
 * to 64 bits.
 *
 * DS (16:29)
 * Immediate field used to specify a 14-bit signed
 * two’s complement integer which is concatenated
 * on the right with 0b00 and sign-extended to 64
 * bits.
 *
 * FLM (7:14)
 * Field mask used to identify the FPSCR fields that
 * are to be updated by the mtfsf instruction.
 *
 * FRA (11:15)
 * Field used to specify an FPR to be used as a
 * source.
 *
 * FRB (16:20)
 * Field used to specify an FPR to be used as a
 * source.
 *
 * FRC (21:25)
 * Field used to specify an FPR to be used as a
 * source.
 *
 * FRS (6:10)
 * Field used to specify an FPR to be used as a
 * source.
 *
 * FRT (6:10)
 * Field used to specify an FPR to be used as a tar-
 * get.
 *
 * FXM (12:19)
 * Field mask used to identify the CR fields that are to
 * be written by the mtcrf and mtocrf instructions, or
 * read by the mfocrf instruction.
 *
 * L (10 or 15)
 * Field used to specify whether a fixed-point Com-
 * pare instruction is to compare 64-bit numbers or
 * 32-bit numbers.
 * Field used by the optional version of the Data
 * Cache Block Flush instruction (see Book II, Pow-
 * erPC Virtual Environment Architecture).
 * Field used by the Move To Machine State Register
 * and TLB Invalidate Entry instructions (see Book III,
 * PowerPC Operating Environment Architecture).
 *
 * L (9:10)
 * Field used by the Synchronize instruction (see
 * Book II, PowerPC Virtual Environment Architec-
 * ture).
 *
 * LEV (20:26)
 * Field used by the System Call instruction.
 *
 * LI (6:29)
 * Immediate field used to specify a 24-bit signed
 * two’s complement integer which is concatenated
 * on the right with 0b00 and sign-extended to 64
 * bits.
 *
 * LK (31)
 * LINK bit.
 * 0 Do not set the Link Register.
 * 1 Set the Link Register. The address of the
 * instruction following the Branch instruction
 * is placed into the Link Register.
 *
 * MB (21:25) and ME (26:30)
 * Fields used in M-form instructions to specify a
 * 64-bit mask consisting of 1-bits from bit MB+32
 * through bit ME+32 inclusive and 0-bits elsewhere,
 * as described in Section 3.3.12, “Fixed-Point
 * Rotate and Shift Instructions” on page 71.
 *
 * MB (21:26)
 * Field used in MD-form and MDS-form instructions
 * to specify the first 1-bit of a 64-bit mask, as
 * described in Section 3.3.12, “Fixed-Point Rotate
 * and Shift Instructions” on page 71.
 *
 * ME (21:26)
 * Field used in MD-form and MDS-form instructions
 * to specify the last 1-bit of a 64-bit mask, as
 * described in Section 3.3.12, “Fixed-Point Rotate
 * and Shift Instructions” on page 71.
 *
 * NB (16:20)
 * Field used to specify the number of bytes to move
 * in an immediate Move Assist instruction.
 *
 * OPCD (0:5)
 * Primary opcode field.
 *
 * OE (21)
 * Field used by XO-form instructions to enable set-
 * ting OV and SO in the XER.
 *
 * RA (11:15)
 * Field used to specify a GPR to be used as a
 * source or as a target.
 *
 * RB (16:20)
 * Field used to specify a GPR to be used as a
 * source.
 *
 * Rc (31)
 * RECORD bit.
 * 0 Do not alter the Condition Register.
 * 1 Set Condition Register Field 0 or Field 1
 * as described in Section 2.3.1, “Condition
 * Register” on page 18.
 *
 * RS (6:10)
 * Field used to specify a GPR to be used as a
 * source.
 *
 * RT (6:10)
 * 12 PowerPC User Instruction Set Architecture
 * Field used to specify a GPR to be used as a target.
 *
 * SH (16:20, or 16:20 and 30)
 * Field used to specify a shift amount.
 *
 * SI (16:31)
 * Immediate field used to specify a 16-bit signed
 * integer.
 *
 * SPR (11:20)
 * Field used to specify a Special Purpose Register
 * for the mtspr and mfspr instructions.
 *
 * SR (12:15)
 * Field used by the Segment Register Manipulation
 * instructions (see Book III, PowerPC Operating
 * Environment Architecture).
 *
 * TBR (11:20)
 * Field used by the Move From Time Base instruc-
 * tion (see Book II, PowerPC Virtual Environment
 * Architecture).
 *
 * TH (7:10)
 * Field used by the optional data stream variant of
 * the dcbt instruction (see Book II, PowerPC Virtual
 * Environment Architecture).
 *
 * TO (6:10)
 * Field used to specify the conditions on which to
 * trap. The encoding is described in Section 3.3.10,
 * “Fixed-Point Trap Instructions” on page 62.
 *
 * U (16:19)
 * Immediate field used as the data to be placed into
 * a field in the FPSCR.
 *
 * UI (16:31)
 * Immediate field used to specify a 16-bit unsigned
 * integer.
 *
 * XO (21:29, 21:30, 22:30, 26:30, 27:29, 27:30, or
 * 30:31)
 * Extended opcode field.
 */

#pragma once

#include "opcodes.hpp"

namespace Toolbox::Interpreter::PPC {

    struct IForm {
        Opcode m_opcode : 6;
        u32 m_li        : 24;
        bool m_aa       : 1;
        bool m_lk       : 1;
    };

    struct BForm {
        Opcode m_opcode : 6;
        u8 m_bo         : 5;
        u8 m_bi         : 5;
        u16 m_bd        : 14;
        bool m_aa       : 1;
        bool m_lk       : 1;
    };

    struct SCForm {
        Opcode m_opcode      : 6;
        u8 m_reserved_6      : 5;
        u8 m_reserved_11     : 5;
        u8 m_reserved_16     : 4;
        u8 m_lev             : 7;
        u8 m_reserved_27     : 3;
        bool m_reserved_true : 1;
        bool m_reserved_31   : 1;
    };

#pragma pack(push, 1)
    struct DForm {
        Opcode m_opcode : 6;
        union {
            u8 rt   : 5;
            u8 rs   : 5;
            u8 bf_l : 5;
            u8 to   : 5;
            u8 frt  : 5;
            u8 frs  : 5;
        } m_6;
        u8 m_ra : 5;
        union {
            u16 d  : 16;
            u16 si : 16;
            u16 ui : 16;
        } m_16;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    struct DSForm {
        Opcode m_opcode : 6;
        union {
            u8 rt : 5;
            u8 rs : 5;
        } m_6;
        u8 m_ra   : 5;
        u16 m_ds  : 15;
        bool m_lk : 1;
    };
#pragma pack(pop)

    struct XForm {
        Opcode m_opcode : 6;
        union {
            u8 rt   : 5;
            u8 rs   : 5;
            u8 bf_l : 5;
            u8 bf   : 5;
            u8 th   : 5;
            u8 l    : 5;
            u8 to   : 5;
            u8 frt  : 5;
            u8 frs  : 5;
            u8 bt   : 5;
        } m_6;
        union {
            u8 ra  : 5;
            u8 sr  : 5;
            u8 l   : 5;
            u8 fra : 5;
            u8 bfa : 5;
        } m_11;
        union {
            u8 rb  : 5;
            u8 nb  : 5;
            u8 sh  : 5;
            u8 frb : 5;
            u8 u   : 5;
        } m_16;
        u16 m_xo  : 10;
        bool m_rc : 1;
    };

    struct XLForm {
        Opcode m_opcode : 6;
        union {
            u8 bt : 5;
            u8 bo : 5;
            u8 bf : 5;
        } m_6;
        union {
            u8 ba  : 5;
            u8 bi  : 5;
            u8 bfa : 5;
        } m_11;
        union {
            u8 bb : 5;
            u8 bh : 5;
        } m_16;
        u16 m_xo  : 10;
        bool m_lk : 1;
    };

    struct XFXForm {
        Opcode m_opcode : 6;
        union {
            u8 rt : 5;
            u8 rs : 5;
        } m_6;
        union {
            u8 spr : 10;
            u8 tbr : 10;
            u8 fxm : 10;
        } m_11;
        u16 m_xo           : 10;
        bool m_reserved_31 : 1;
    };

    struct XFLForm {
        Opcode m_opcode  : 6;
        u8 m_reserved_6  : 1;
        u16 m_flm        : 8;
        u8 m_reserved_15 : 1;
        u8 m_frb         : 5;
        u16 m_xo         : 10;
        bool m_rc        : 1;
    };

    struct XSForm {
        Opcode m_opcode : 6;
        u8 m_rs         : 5;
        u8 m_ra         : 5;
        u8 m_sh         : 5;
        u16 m_xo        : 9;
        bool m_sh_b     : 1;
        bool m_rc       : 1;
    };

    struct XOForm {
        Opcode m_opcode : 6;
        u8 m_rt         : 5;
        u8 m_ra         : 5;
        u8 m_rb         : 5;
        bool m_oe       : 1;
        u16 m_xo        : 9;
        bool m_rc       : 1;
    };

    struct AForm {
        Opcode m_opcode : 6;
        u8 m_frt        : 5;
        u8 m_fra        : 5;
        u8 m_frb        : 5;
        u8 m_frc        : 5;
        u16 m_xo        : 5;
        bool m_rc       : 1;
    };

    struct MForm {
        Opcode m_opcode : 6;
        u8 m_rs         : 5;
        u8 m_ra         : 5;
        union {
            u8 rb : 5;
            u8 sh : 5;
        } m_16;
        u8 m_mb   : 5;
        u8 m_me   : 5;
        bool m_rc : 1;
    };

    struct MDForm {
        Opcode m_opcode : 6;
        u8 m_rs         : 5;
        u8 m_ra         : 5;
        u8 m_sh         : 5;
        union {
            u8 mb : 6;
            u8 me : 6;
        } m_21;
        u8 m_xo   : 3;
        bool m_sh : 1;
        bool m_rc : 1;
    };

    struct MDSForm {
        Opcode m_opcode : 6;
        u8 m_rs         : 5;
        u8 m_ra         : 5;
        u8 m_rb         : 5;
        union {
            u8 mb : 6;
            u8 me : 6;
        } m_21;
        u8 m_xo   : 4;
        bool m_rc : 1;
    };

#define FORM_RA(inst)    (u8)(((inst) >> 16) & 0x1f)
#define FORM_RB(inst)    (u8)(((inst) >> 11) & 0x1f)
#define FORM_RC(inst)    (u8)(((inst) >> 6) & 0x1f)
#define FORM_RD(inst)    (u8)(((inst) >> 21) & 0x1f)
#define FORM_RS(inst)    (u8)(((inst) >> 21) & 0x1f)
#define FORM_FA(inst)    (u8)(((inst) >> 16) & 0x1f)
#define FORM_FB(inst)    (u8)(((inst) >> 11) & 0x1f)
#define FORM_FC(inst)    (u8)(((inst) >> 6) & 0x1f)
#define FORM_FD(inst)    (u8)(((inst) >> 21) & 0x1f)
#define FORM_FS(inst)    (u8)(((inst) >> 21) & 0x1f)
#define FORM_IMM(inst)   (s16)((inst)&0xffff)
#define FORM_UIMM(inst)  (u16)((inst)&0xffff)
#define FORM_OFS(inst)   (s16)((inst)&0xffff)
#define FORM_OPCD(inst)  (Opcode)(((inst) >> 26) & 0x3f)
#define FORM_XO_10(inst) (s16)(((inst) >> 1) & 0x3ff)
#define FORM_XO_9(inst)  (s16)(((inst) >> 1) & 0x1ff)
#define FORM_XO_5(inst)  (u8)(((inst) >> 1) & 0x1f)
#define FORM_Rc(inst)    (bool)((inst)&1)
#define FORM_SH(inst)    (u16)(((inst) >> 11) & 0x1f)
#define FORM_MB(inst)    (s16)(((inst) >> 6) & 0x1f)
#define FORM_ME(inst)    (s16)(((inst) >> 1) & 0x1f)
#define FORM_OE(inst)    (bool)(((inst) >> 10) & 1)
#define FORM_TO(inst)    (u8)(((inst) >> 21) & 0x1f)
#define FORM_CRFD(inst)  (u8)(((inst) >> 23) & 0x7)
#define FORM_CRFS(inst)  (u8)(((inst) >> 18) & 0x7)
#define FORM_CRBD(inst)  (u8)(((inst) >> 21) & 0x1f)
#define FORM_CRBA(inst)  (u8)(((inst) >> 16) & 0x1f)
#define FORM_CRBB(inst)  (u8)(((inst) >> 11) & 0x1f)
#define FORM_L(inst)     (bool)(((inst) >> 21) & 1)
#define FORM_NB(inst)    (u8)(((inst) >> 11) & 0x1f)
#define FORM_AA(inst)    (bool)(((inst) >> 1) & 1)
#define FORM_LK(inst)    (bool)((inst)&1)
#define FORM_LI(inst)    (s32)(((inst) >> 2) & 0xffffff)
#define FORM_BO(inst)    (u8)(((inst) >> 21) & 0x1f)
#define FORM_BI(inst)    (u8)(((inst) >> 16) & 0x1f)
#define FORM_BD(inst)    (s16)(((inst) >> 2) & 0x3fff)
#define FORM_D(inst)    (s16)(((inst) >> 16) & 0xffff)
#define FORM_SI(inst)    (s16)(((inst) >> 16) & 0xffff)
#define FORM_UI(inst)    (u16)(((inst) >> 16) & 0xffff)

#define FORM_MTFSFI_IMM(inst) (u8)(((inst) >> 12) & 0xf)
#define FORM_FM(inst)         (u8)(((inst) >> 17) & 0xff)
#define FORM_SR(inst)         (u8)(((inst) >> 16) & 0xf)
#define FORM_SPR(inst)        (s16)(((inst) >> 11) & 0x3ff)
#define FORM_TBR(inst)        (s16)(((inst) >> 11) & 0x3ff)
#define FORM_CRM(inst)        (u8)(((inst) >> 12) & 0xff)

#define FORM_I(inst)  (u8)(((inst) >> 12) & 0x7)
#define FORM_W(inst)  (bool)(((inst) >> 15) & 0x1)
#define FORM_IX(inst) (u8)(((inst) >> 7) & 0x7)
#define FORM_WX(inst) (bool)(((inst) >> 10) & 0x1)

}  // namespace Toolbox::Interpreter::PPC