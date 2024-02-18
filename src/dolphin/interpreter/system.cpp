#include "dolphin/interpreter/system.hpp"
#include "dolphin/interpreter/instructions/forms.hpp"
#include "dolphin/process.hpp"

#include "core/application.hpp"

using namespace Toolbox::Dolphin;

namespace Toolbox::Interpreter {

    void SystemDolphin::evaluateFunction() {
        while (m_evaluating.load()) {
            evaluateInstruction();
        }
    }

    void SystemDolphin::evaluateInstruction() {
        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            m_evaluating.store(false);
            return;
        }

        u32 inst      = communicator.read<u32>(static_cast<u32>(m_system_proc.m_pc)).value();
        Opcode opcode = FORM_OPCD(inst);
        switch (opcode) {
        case Opcode::OP_TWI:
            m_fixed_proc.twi(FORM_TO(inst), FORM_RA(inst), FORM_SI(inst));
            break;
        case Opcode::OP_PAIRED_SINGLE:
            evaluatePairedSingleSubOp(inst);
            break;
        case Opcode::OP_MULLI:
            m_fixed_proc.mulli(FORM_TO(inst), FORM_RA(inst), FORM_SI(inst));
            break;
        case Opcode::OP_SUBFIC:
            m_fixed_proc.subfic(FORM_TO(inst), FORM_RA(inst), FORM_SI(inst));
            break;
        case Opcode::OP_CMPLI:
            m_fixed_proc.cmpli(FORM_CRFD(inst), FORM_L(inst), FORM_RA(inst), FORM_UI(inst),
                               m_branch_proc.m_cr);
            break;
        case Opcode::OP_CMPL:
            m_fixed_proc.cmpli(FORM_CRFD(inst), FORM_L(inst), FORM_RA(inst), FORM_RB(inst),
                               m_branch_proc.m_cr);
            break;
        case Opcode::OP_ADDIC:
        case Opcode::OP_ADDIC_RC:
            m_fixed_proc.addic(FORM_RS(inst), FORM_RA(inst), FORM_SI(inst), FORM_RC(inst),
                               m_branch_proc.m_cr);
            break;
        case Opcode::OP_ADDI:
            m_fixed_proc.addi(FORM_RS(inst), FORM_RA(inst), FORM_SI(inst));
            break;
        case Opcode::OP_ADDIS:
            m_fixed_proc.addis(FORM_RS(inst), FORM_RA(inst), FORM_SI(inst));
            break;
        case Opcode::OP_BC:
            m_branch_proc.bc(FORM_BD(inst), FORM_BO(inst), FORM_BI(inst), FORM_AA(inst),
                             FORM_LK(inst), m_system_proc.m_pc);
            break;
        case Opcode::OP_SC:
            m_system_proc.sc(FORM_LEV(inst));
            break;
        case Opcode::OP_B:
            m_branch_proc.b(FORM_LI(inst), FORM_AA(inst), FORM_LK(inst), m_system_proc.m_pc);
            break;
        case Opcode::OP_CONTROL_FLOW:
            evaluateControlFlowSubOp(inst);
            break;
        case Opcode::OP_RLWIMI:
            m_fixed_proc.rlwimi(FORM_RA(inst), FORM_RS(inst), FORM_SH(inst), FORM_MB(inst),
                                FORM_ME(inst), FORM_RC(inst), m_branch_proc.m_cr);
            break;
        case Opcode::OP_RLWINM:
            m_fixed_proc.rlwinm(FORM_RA(inst), FORM_RS(inst), FORM_SH(inst), FORM_MB(inst),
                                FORM_ME(inst), FORM_RC(inst), m_branch_proc.m_cr);
            break;
        case Opcode::OP_RLWNM:
            m_fixed_proc.rlwnm(FORM_RA(inst), FORM_RS(inst), FORM_RB(inst), FORM_MB(inst),
                               FORM_ME(inst), FORM_RC(inst), m_branch_proc.m_cr);
            break;
        case Opcode::OP_ORI:
            m_fixed_proc.ori(FORM_RA(inst), FORM_RS(inst), FORM_UI(inst));
            break;
        case Opcode::OP_ORIS:
            m_fixed_proc.oris(FORM_RA(inst), FORM_RS(inst), FORM_UI(inst));
            break;
        case Opcode::OP_XORI:
            m_fixed_proc.xori(FORM_RA(inst), FORM_RS(inst), FORM_UI(inst));
            break;
        case Opcode::OP_XORIS:
            m_fixed_proc.xoris(FORM_RA(inst), FORM_RS(inst), FORM_UI(inst));
            break;
        case Opcode::OP_ANDI:
            m_fixed_proc.andi(FORM_RA(inst), FORM_RS(inst), FORM_UI(inst), m_branch_proc.m_cr);
            break;
        case Opcode::OP_ANDIS:
            m_fixed_proc.andis(FORM_RA(inst), FORM_RS(inst), FORM_UI(inst), m_branch_proc.m_cr);
            break;
        case Opcode::OP_FIXED:
            evaluateFixedSubOp(inst);
            break;
        case Opcode::OP_LWZ:
            m_fixed_proc.lwz(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LWZU:
            m_fixed_proc.lwzu(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LBZ:
            m_fixed_proc.lbz(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LBZU:
            m_fixed_proc.lbzu(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STW:
            m_fixed_proc.stw(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STWU:
            m_fixed_proc.stwu(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STB:
            m_fixed_proc.stb(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STBU:
            m_fixed_proc.stbu(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LHZ:
            m_fixed_proc.lhz(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LHZU:
            m_fixed_proc.lhzu(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LHA:
            m_fixed_proc.lha(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LHAU:
            m_fixed_proc.lhau(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STH:
            m_fixed_proc.sth(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STHU:
            m_fixed_proc.sthu(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LMW:
            m_fixed_proc.lmw(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STMW:
            m_fixed_proc.stmw(FORM_RS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LFS:
            m_float_proc.lfs(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LFSU:
            m_float_proc.lfsu(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LFD:
            m_float_proc.lfd(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_LFDU:
            m_float_proc.lfdu(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STFS:
            m_float_proc.stfs(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STFSU:
            m_float_proc.stfsu(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STFD:
            m_float_proc.stfd(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_STFDU:
            m_float_proc.stfdu(FORM_FS(inst), FORM_D(inst), FORM_RA(inst), m_storage);
            break;
        case Opcode::OP_FS_MATH:
            evaluateFloatSingleSubOp(inst);
            break;
        case Opcode::OP_FLOAT:
            evaluateFloatSubOp(inst);
            break;
        default:
            internalInvalidCB(PROC_INVALID_MSG(SystemDolphin, unknown,
                                               "Attempted to evaluate unknown instruction!"));
            break;
        }
    }

    void SystemDolphin::evaluatePairedSingleSubOp(u32 inst) {
        TableSubOpcode4 sub_op = (TableSubOpcode4)FORM_XO_5(inst);
        switch (sub_op) {
        default:
            internalInvalidCB(PROC_INVALID_MSG(SystemDolphin, unknown,
                                               "Attempted to evaluate unknown instruction!"));
            break;
        }
    }

    void SystemDolphin::evaluateControlFlowSubOp(u32 inst) {
        TableSubOpcode19 sub_op = (TableSubOpcode19)FORM_XO_9(inst);
        switch (sub_op) {
        case TableSubOpcode19::MCRF:
            m_branch_proc.mcrf(FORM_CRBD(inst), FORM_CRBA(inst));
            break;
        case TableSubOpcode19::BCLR:
            m_branch_proc.bclr(FORM_BO(inst), FORM_BI(inst), FORM_LK(inst), m_system_proc.m_pc);
            break;
        case TableSubOpcode19::CRNOR:
            m_branch_proc.crnor(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::RFI:
            m_system_proc.rfi();
            break;
        case TableSubOpcode19::CRANDC:
            m_branch_proc.crandc(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::ISYNC:
            m_system_proc.sync((SyncType)0);
            break;
        case TableSubOpcode19::CRXOR:
            m_branch_proc.crxor(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::CRNAND:
            m_branch_proc.crnand(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::CRAND:
            m_branch_proc.crand(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::CREQV:
            m_branch_proc.creqv(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::CRORC:
            m_branch_proc.crorc(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::CROR:
            m_branch_proc.cror(FORM_CRBD(inst), FORM_CRBA(inst), FORM_CRBB(inst));
            break;
        case TableSubOpcode19::BCCTR:
            m_branch_proc.bcctr(FORM_BO(inst), FORM_BI(inst), FORM_LK(inst), m_system_proc.m_pc);
        default:
            internalInvalidCB(PROC_INVALID_MSG(SystemDolphin, unknown,
                                               "Attempted to evaluate unknown instruction!"));
            break;
        }
    }

    void SystemDolphin::evaluateFixedSubOp(u32 inst) {
        TableSubOpcode31 sub_op = (TableSubOpcode31)FORM_XO_9(inst);
        switch (sub_op) {
        case TableSubOpcode31::ADD:
            break;
        case TableSubOpcode31::ADDO:
            break;
        case TableSubOpcode31::ADDC:
            break;
        case TableSubOpcode31::ADDCO:
            break;
        case TableSubOpcode31::ADDE:
            break;
        case TableSubOpcode31::ADDEO:
            break;
        case TableSubOpcode31::ADDME:
            break;
        case TableSubOpcode31::ADDMEO:
            break;
        case TableSubOpcode31::ADDZE:
            break;
        case TableSubOpcode31::ADDZEO:
            break;
        case TableSubOpcode31::DIVW:
            break;
        case TableSubOpcode31::DIVWO:
            break;
        case TableSubOpcode31::DIVWU:
            break;
        case TableSubOpcode31::DIVWUO:
            break;
        case TableSubOpcode31::MULHW:
            break;
        case TableSubOpcode31::MULHWU:
            break;
        case TableSubOpcode31::MULLW:
            break;
        case TableSubOpcode31::MULLWO:
            break;
        case TableSubOpcode31::NEG:
            break;
        case TableSubOpcode31::NEGO:
            break;
        case TableSubOpcode31::SUBF:
            break;
        case TableSubOpcode31::SUBFOs:
            break;
        case TableSubOpcode31::SUBFC:
            break;
        case TableSubOpcode31::SUBFCO:
            break;
        case TableSubOpcode31::SUBFE:
            break;
        case TableSubOpcode31::SUBFEO:
            break;
        case TableSubOpcode31::SUBFME:
            break;
        case TableSubOpcode31::SUBFMEO:
            break;
        case TableSubOpcode31::SUBFZE:
            break;
        case TableSubOpcode31::SUBFZEO:
            break;
        case TableSubOpcode31::AND:
            break;
        case TableSubOpcode31::ANDC:
            break;
        case TableSubOpcode31::OR:
            break;
        case TableSubOpcode31::NOR:
            break;
        case TableSubOpcode31::XOR:
            break;
        case TableSubOpcode31::ORC:
            break;
        case TableSubOpcode31::NAND:
            break;
        case TableSubOpcode31::EQV:
            break;
        case TableSubOpcode31::CMP:
            break;
        case TableSubOpcode31::CMPL:
            break;
        case TableSubOpcode31::CNTLZW:
            break;
        case TableSubOpcode31::EXTSH:
            break;
        case TableSubOpcode31::EXTSB:
            break;
        case TableSubOpcode31::SRW:
            break;
        case TableSubOpcode31::SRAW:
            break;
        case TableSubOpcode31::SRAWI:
            break;
        case TableSubOpcode31::SLW:
            break;
        case TableSubOpcode31::DCBST:
            break;
        case TableSubOpcode31::DCBF:
            break;
        case TableSubOpcode31::DCBTST:
            break;
        case TableSubOpcode31::DCBT:
            break;
        case TableSubOpcode31::DCBI:
            break;
        case TableSubOpcode31::DCBA:
            break;
        case TableSubOpcode31::DCBZ:
            break;
        case TableSubOpcode31::LWZ:
            break;
        case TableSubOpcode31::LWZU:
            break;
        case TableSubOpcode31::LHZ:
            break;
        case TableSubOpcode31::LHZU:
            break;
        case TableSubOpcode31::LHA:
            break;
        case TableSubOpcode31::LHAU:
            break;
        case TableSubOpcode31::LBZ:
            break;
        case TableSubOpcode31::LBZU:
            break;
        case TableSubOpcode31::LWBR:
            break;
        case TableSubOpcode31::LHBR:
            break;
        case TableSubOpcode31::STWCXD:
            break;
        case TableSubOpcode31::LWAR:
            break;
        case TableSubOpcode31::LSW:
            break;
        case TableSubOpcode31::LSWI:
            break;
        case TableSubOpcode31::STW:
            break;
        case TableSubOpcode31::STWU:
            break;
        case TableSubOpcode31::STH:
            break;
        case TableSubOpcode31::STHU:
            break;
        case TableSubOpcode31::STB:
            break;
        case TableSubOpcode31::STBU:
            break;
        case TableSubOpcode31::STWBR:
            break;
        case TableSubOpcode31::STHBR:
            break;
        case TableSubOpcode31::STSW:
            break;
        case TableSubOpcode31::STSWI:
            break;
        case TableSubOpcode31::LFS:
            break;
        case TableSubOpcode31::LFSU:
            break;
        case TableSubOpcode31::LFD:
            break;
        case TableSubOpcode31::LFDU:
            break;
        case TableSubOpcode31::STFS:
            break;
        case TableSubOpcode31::STFSU:
            break;
        case TableSubOpcode31::STFD:
            break;
        case TableSubOpcode31::STFDU:
            break;
        case TableSubOpcode31::STFIW:
            break;
        case TableSubOpcode31::MFCR:
            break;
        case TableSubOpcode31::MFMSR:
            break;
        case TableSubOpcode31::MTCRF:
            break;
        case TableSubOpcode31::MTMSR:
            break;
        case TableSubOpcode31::MTSR:
            break;
        case TableSubOpcode31::MTSRIN:
            break;
        case TableSubOpcode31::MFSPR:
            break;
        case TableSubOpcode31::MTSPR:
            break;
        case TableSubOpcode31::MFTB:
            break;
        case TableSubOpcode31::MCRXR:
            break;
        case TableSubOpcode31::MFSR:
            break;
        case TableSubOpcode31::MFSRIN:
            break;
        case TableSubOpcode31::TW:
            break;
        case TableSubOpcode31::SYNC:
            break;
        case TableSubOpcode31::ICBI:
            break;
        case TableSubOpcode31::ECIW:
            break;
        case TableSubOpcode31::ECOW:
            break;
        case TableSubOpcode31::EIEIO:
            break;
        case TableSubOpcode31::TLBIE:
            break;
        case TableSubOpcode31::TLBSYNC:
            break;
        }
    }

    void SystemDolphin::evaluateFloatSingleSubOp(u32 inst) {
        TableSubOpcode59 sub_op = (TableSubOpcode59)FORM_XO_5(inst);
        switch (sub_op) {
        case TableSubOpcode59::FDIVS:
            m_float_proc.fdivs(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                               m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FSUBS:
            m_float_proc.fsubs(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                               m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FADDS:
            m_float_proc.fadds(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                               m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FRES:
            m_float_proc.fres(FORM_FS(inst), FORM_FA(inst), FORM_RC(inst), m_branch_proc.m_cr,
                              m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FMULS:
            m_float_proc.fmuls(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                               m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FMSUBS:
            m_float_proc.fmsubs(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                                FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                                m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FMADDS:
            m_float_proc.fmadds(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                                FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                                m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FNMSUBS:
            m_float_proc.fnmsubs(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                                 FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                                 m_system_proc.m_srr1);
            break;
        case TableSubOpcode59::FNMADDS:
            m_float_proc.fnmadds(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                                 FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                                 m_system_proc.m_srr1);
            break;
        default:
            internalInvalidCB(PROC_INVALID_MSG(SystemDolphin, unknown,
                                               "Attempted to evaluate unknown instruction!"));
            break;
        }
    }

    void SystemDolphin::evaluateFloatSubOp(u32 inst) {
        TableSubOpcode63 sub_op = (TableSubOpcode63)FORM_XO_5(inst);
        switch (sub_op) {
        case TableSubOpcode63::FCMPU:
            m_float_proc.fcmpu(FORM_CRFD(inst), FORM_FA(inst), FORM_FB(inst), m_branch_proc.m_cr,
                               m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FRSP:
            m_float_proc.frsp(FORM_FS(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                              m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FCTIW:
            m_float_proc.fctiw(FORM_FS(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                               m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FCTIWZ:
            m_float_proc.fctiwz(FORM_FS(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                                m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FDIV:
            m_float_proc.fdiv(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                              m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FSUB:
            m_float_proc.fsub(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                              m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FADD:
            m_float_proc.fadd(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                              m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FRES:
            m_float_proc.fres(FORM_FS(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                              m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FMUL:
            m_float_proc.fmul(FORM_FS(inst), FORM_FA(inst), FORM_FB(inst), FORM_RC(inst),
                              m_branch_proc.m_cr, m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FMSUB:
            m_float_proc.fmsub(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                               FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                               m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FMADD:
            m_float_proc.fmadd(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                               FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                               m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FNMSUB:
            m_float_proc.fnmsub(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                                FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                                m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FNMADD:
            m_float_proc.fnmadd(FORM_FS(inst), FORM_FA(inst), FORM_FC(inst), FORM_FB(inst),
                                FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                                m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FCMPO:
            m_float_proc.fcmpo(FORM_CRFD(inst), FORM_FA(inst), FORM_FB(inst), m_branch_proc.m_cr,
                               m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::MTFSB1:
            m_float_proc.mtfsb1(FORM_CRBD(inst), FORM_RC(inst), m_branch_proc.m_cr,
                                m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FNEG:
            m_float_proc.fneg(FORM_FS(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                              m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::MCRFS:
            m_float_proc.mcrfs(FORM_CRBD(inst), FORM_CRBA(inst), m_branch_proc.m_cr,
                               m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::MTFSB0:
            m_float_proc.mtfsb0(FORM_CRBD(inst), FORM_RC(inst), m_branch_proc.m_cr,
                                m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FMR:
            m_float_proc.fmr(FORM_CRFD(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                             m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::MTFSFI:
            break;
        case TableSubOpcode63::FNABS:
            m_float_proc.fnabs(FORM_CRFD(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                               m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::FABS:
            m_float_proc.fabs(FORM_CRFD(inst), FORM_FB(inst), FORM_RC(inst), m_branch_proc.m_cr,
                              m_system_proc.m_msr, m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::MFFS:
            m_float_proc.mffs(FORM_FS(inst), FORM_RC(inst), m_branch_proc.m_cr, m_system_proc.m_msr,
                              m_system_proc.m_srr1);
            break;
        case TableSubOpcode63::MTFS:
        default:
            internalInvalidCB(PROC_INVALID_MSG(SystemDolphin, unknown,
                                               "Attempted to evaluate unknown instruction!"));
            break;
        }
    }

}  // namespace Toolbox::Interpreter