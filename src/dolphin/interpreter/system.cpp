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

    void SystemDolphin::evaluateFixedSubOp(u32 inst) {}

    void SystemDolphin::evaluateFloatSingleSubOp(u32 inst) {}

    void SystemDolphin::evaluateFloatSubOp(u32 inst) {}

}  // namespace Toolbox::Interpreter