#pragma once

#include <magic_enum.hpp>

#include "core/core.hpp"
#include "core/memory.hpp"

#include "serial.hpp"

#include "dolphin/interpreter/processor.hpp"

// Used in implementations of rlwimi, rlwinm, and rlwnm
inline u32 MakeRotationMask(u32 mb, u32 me) {
    // first make 001111111111111 part
    const u32 begin = 0xFFFFFFFF >> mb;
    // then make 000000000001111 part, which is used to flip the bits of the first one
    const u32 end = 0x7FFFFFFF >> me;
    // do the bitflip
    const u32 mask = begin ^ end;

    // and invert if backwards
    if (me < mb)
        return ~mask;

    return mask;
}

namespace Toolbox::Interpreter {

    // Memory
    void FixedPointProcessor::lbz(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lbz, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u8>(destination);
    }

    void FixedPointProcessor::lbzu(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lbzu, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u8>(destination);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::lbzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lbzx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lbzx,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lbzx,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u8>(destination);
    }

    void FixedPointProcessor::lbzux(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lbzux, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lbzux,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lbzux,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u8>(destination);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::lhz(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhz, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u16>(destination);
    }

    void FixedPointProcessor::lhzu(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhzu, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u16>(destination);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::lhzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhzx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhzx,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhzx,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u16>(destination);
    }

    void FixedPointProcessor::lhzux(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhzux, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhzux,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhzux,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u16>(destination);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::lha(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lha, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<s16>(destination);
    }

    void FixedPointProcessor::lhau(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhau, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<s16>(destination);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::lhax(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhax, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<s16>(destination);
    }

    void FixedPointProcessor::lhaux(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhaux, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<s16>(destination);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::lwz(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwz, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u32>(destination);
    }

    void FixedPointProcessor::lwzu(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lwzu, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u32>(destination);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::lwzx(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lwzx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwzx,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwzx,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u32>(destination);
    }

    void FixedPointProcessor::lwzux(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lwzux, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwzux,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwzux,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = storage.get<u32>(destination);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::stb(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stb, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u8>(destination, m_gpr[rs]);
    }

    void FixedPointProcessor::stbu(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stbu, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u8>(destination, m_gpr[rs]);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::stbx(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stbx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stbx,
                                          "Indexed store using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stbx,
                                          "Indexed store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u8>(destination, m_gpr[rs]);
    }

    void FixedPointProcessor::stbux(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stbux, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stbux,
                                          "Indexed store using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stbux,
                                          "Indexed store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u8>(destination, m_gpr[rs]);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::sth(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, sth, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u16>(destination, m_gpr[rs]);
    }

    void FixedPointProcessor::sthu(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, sthu, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u16>(destination, m_gpr[rs]);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::sthx(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, sthx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, sthx,
                                          "Indexed store using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, sthx,
                                          "Indexed store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u16>(destination, m_gpr[rs]);
    }

    void FixedPointProcessor::sthux(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, sthux, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, sthux,
                                          "Indexed store using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, sthux,
                                          "Indexed store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u16>(destination, m_gpr[rs]);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::stw(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stw, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, m_gpr[rs]);
    }

    void FixedPointProcessor::stwu(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stwu, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, m_gpr[rs]);
        m_gpr[ra] += d;
    }

    void FixedPointProcessor::stwx(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stwx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stwx,
                                          "Indexed store using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stwx,
                                          "Indexed store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, m_gpr[rs]);
    }

    void FixedPointProcessor::stwux(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stwux, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stwux,
                                          "Indexed store using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stwux,
                                          "Indexed store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, m_gpr[rs]);
        m_gpr[ra] += m_gpr[rb];
    }

    void FixedPointProcessor::lhbrx(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lhbrx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhbrx,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lhbrx,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = std::byteswap(storage.get<u16>(destination));
    }
    void FixedPointProcessor::lwbrx(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lwbrx, "Invalid registers detected!"));
            return;
        }
        if (ra == rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwbrx,
                                          "Indexed load using equivalent registers is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lwbrx,
                                          "Indexed load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        m_gpr[rt] = std::byteswap(storage.get<u32>(destination));
    }

    void FixedPointProcessor::sthbrx(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, sthbrx, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u16>(destination, std::byteswap<u16>(m_gpr[rs]));
    }
    void FixedPointProcessor::stwbrx(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stwbrx, "Invalid registers detected!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        if (!MemoryContainsPAddress(storage, destination)) {
            m_exception_cb(ExceptionCause::EXCEPTION_DSI);
            return;
        }
        storage.set<u32>(destination, std::byteswap<u32>(m_gpr[rs]));
    }

    void FixedPointProcessor::lmw(u8 rt, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lmw, "Invalid registers detected!"));
            return;
        }
        if (rt <= ra) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lmw,
                                          "Source register in range of multi load is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lmw,
                                          "Load using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if ((destination & 0b11) != 0) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
        }
        while (rt < 32) {
            if (!MemoryContainsPAddress(storage, destination)) {
                m_exception_cb(ExceptionCause::EXCEPTION_DSI);
                return;
            }
            m_gpr[rt++] = storage.get<u32>(destination);
            destination += 4;
        }
    }
    void FixedPointProcessor::stmw(u8 rs, s16 d, u8 ra, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stmw, "Invalid registers detected!"));
            return;
        }
        if (rs <= ra) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stmw,
                                          "Source register in range of multi store is invalid!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, stmw,
                                          "Store using source register 0 is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + d - 0x80000000;
        if ((destination & 0b11) != 0) {
            m_exception_cb(ExceptionCause::EXCEPTION_ALIGNMENT);
        }
        while (rs < 32) {
            if (!MemoryContainsPAddress(storage, destination)) {
                m_exception_cb(ExceptionCause::EXCEPTION_DSI);
                return;
            }
            storage.set<u32>(destination, m_gpr[rs++]);
            destination += 4;
        }
    }

    void FixedPointProcessor::lswi(u8 rt, u8 ra, u8 nb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lswi, "Invalid registers detected!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswi,
                                          "Store using source register 0 is invalid!"));
            return;
        }
        if (rt <= ra) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswi,
                                          "Source register in range of string load is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] - 0x80000000;
        while (nb > 0) {
            if (!MemoryContainsPAddress(storage, destination)) {
                m_exception_cb(ExceptionCause::EXCEPTION_DSI);
                return;
            }
            if (nb < 4) {
                u32 value = storage.get<u32>(destination);
                value &= ~((0x100 << ((nb - 1) * 8)) - 1);
                m_gpr[rt++] = value;
                destination += 4;
                nb = 0;
            } else {
                m_gpr[rt++] = storage.get<u32>(destination);
                destination += 4;
                nb -= 4;
            }
            rt %= 32;
        }
    }

    void FixedPointProcessor::lswx(u8 rt, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, lswx, "Invalid registers detected!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswx,
                                          "Store using source register 0 is invalid!"));
            return;
        }
        if (rt <= ra || rt <= rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswx,
                                          "Source register in range of string load is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] + m_gpr[rb] - 0x80000000;
        u8 nb           = XER_STR(m_xer);
        while (nb > 0) {
            if (!MemoryContainsPAddress(storage, destination)) {
                m_exception_cb(ExceptionCause::EXCEPTION_DSI);
                return;
            }
            if (nb < 4) {
                u32 value = storage.get<u32>(destination);
                value &= ~((0x100 << ((nb - 1) * 8)) - 1);
                m_gpr[rt++] = value;
                destination += 4;
                nb = 0;
            } else {
                m_gpr[rt++] = storage.get<u32>(destination);
                destination += 4;
                nb -= 4;
            }
            rt %= 32;
        }
    }

    void FixedPointProcessor::stswi(u8 rs, u8 ra, u8 nb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stwbrx, "Invalid registers detected!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswx,
                                          "Store using source register 0 is invalid!"));
            return;
        }
        if (rs <= ra) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswx,
                                          "Source register in range of string load is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] - 0x80000000;
        while (nb > 0) {
            if (!MemoryContainsPAddress(storage, destination)) {
                m_exception_cb(ExceptionCause::EXCEPTION_DSI);
                return;
            }
            if (nb < 4) {
                memcpy(storage.buf<u8>() + destination, &m_gpr[rs++], nb);
                destination += 4;
                nb = 0;
            } else {
                storage.set<u32>(destination, m_gpr[rs++]);
                destination += 4;
                nb -= 4;
            }
            rs %= 32;
        }
    }

    void FixedPointProcessor::stswx(u8 rs, u8 ra, u8 rb, Buffer &storage) {
        if (!IsRegValid(rs) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, stwbrx, "Invalid registers detected!"));
            return;
        }
        if (ra == 0) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswx,
                                          "Store using source register 0 is invalid!"));
            return;
        }
        if (rs <= ra || rs <= rb) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, lswx,
                                          "Source register in range of string load is invalid!"));
            return;
        }
        s32 destination = m_gpr[ra] - 0x80000000;
        u8 nb           = XER_STR(m_xer);
        while (nb > 0) {
            if (!MemoryContainsPAddress(storage, destination)) {
                m_exception_cb(ExceptionCause::EXCEPTION_DSI);
                return;
            }
            if (nb < 4) {
                memcpy(storage.buf<u8>() + destination, &m_gpr[rs++], nb);
                destination += 4;
                nb = 0;
            } else {
                storage.set<u32>(destination, m_gpr[rs++]);
                destination += 4;
                nb -= 4;
            }
            rs %= 32;
        }
    }

    // Math

    void FixedPointProcessor::addi(u8 rt, u8 ra, s16 si) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, addi, "Invalid registers detected!"));
            return;
        }
        u64 result = ra == 0 ? si : m_gpr[ra] + si;
        m_gpr[rt]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::addis(u8 rt, u8 ra, s16 si) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, addis, "Invalid registers detected!"));
            return;
        }
        u64 result = ra == 0 ? si : m_gpr[ra] + si;
        m_gpr[rt]  = (result << 16) & 0xFFFFFFFF;
    }

    void FixedPointProcessor::add(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, add, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[ra] + m_gpr[rb];
        m_gpr[rt]  = result & 0xFFFFFFFF;
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::addic(u8 rt, u8 ra, s16 si, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, addic, "Invalid registers detected!"));
            return;
        }
        u64 result = ra == 0 ? si : m_gpr[ra] + si;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
    }

    void FixedPointProcessor::subf(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, subf, "Invalid registers detected!"));
            return;
        }
        u64 result = ~m_gpr[ra] + m_gpr[rb] + 1;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfic(u8 rt, u8 ra, s16 si) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, subfic, "Invalid registers detected!"));
            return;
        }
        u64 result = ~m_gpr[ra] + (s32)si + 1;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
    }

    void FixedPointProcessor::addc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, addc, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[ra] + m_gpr[rb];
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfc(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, subfc, "Invalid registers detected!"));
            return;
        }
        u64 result = ~m_gpr[ra] + m_gpr[rb] + 1;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::adde(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, adde, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[ra] + m_gpr[rb] + (u64)XER_CA(m_xer);
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfe(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, subfe, "Invalid registers detected!"));
            return;
        }
        u64 result = ~m_gpr[ra] + m_gpr[rb] + (u64)XER_CA(m_xer);
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::addme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, addme, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[ra] + (u64)XER_CA(m_xer) + 0xFFFFFFFF;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfme(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, subfme, "Invalid registers detected!"));
            return;
        }
        u64 result = ~m_gpr[ra] + (u64)XER_CA(m_xer) - 1;
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::addze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, addze, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[ra] + (u64)XER_CA(m_xer);
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::subfze(u8 rt, u8 ra, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, subfze, "Invalid registers detected!"));
            return;
        }
        u64 result = ~m_gpr[ra] + (u64)XER_CA(m_xer);
        m_gpr[rt]  = result & 0xFFFFFFFF;
        XER_SET_CA(m_xer, result > 0xFFFFFFFF);
        if (XER_CA(m_xer) && oe) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::mulli(u8 rt, u8 ra, s16 si) {
        if (!IsRegValid(rt) || !IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mulli, "Invalid registers detected!"));
            return;
        }
        s64 result = m_gpr[ra] * si;
        m_gpr[rt]  = (result & 0xFFFFFFFF);
    }

    void FixedPointProcessor::mullw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mullw, "Invalid registers detected!"));
            return;
        }
        s64 result = m_gpr[ra] * m_gpr[rb];
        m_gpr[rt]  = (result & 0xFFFFFFFF);
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::mullhw(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mullhw, "Invalid registers detected!"));
            return;
        }
        s64 result = (s64)m_gpr[ra] * (s64)m_gpr[rb];
        m_gpr[rt]  = (s32)(result >> 32);
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::mullhwu(u8 rt, u8 ra, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mullhwu, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[ra] * m_gpr[rb];
        m_gpr[rt]  = (u32)(result >> 32);
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::divw(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, divw, "Invalid registers detected!"));
            return;
        }
        u64 result = (s32)m_gpr[ra] / (s32)m_gpr[rb];
        if (m_gpr[ra] == 0x80000000 && m_gpr[rb] == 0xFFFFFFFF) {
            result = 0x7FFFFFFF;
        } else if (m_gpr[rb] == 0) {
            result = 0x7FFFFFFF;
        }
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    void FixedPointProcessor::divwu(u8 rt, u8 ra, u8 rb, bool oe, bool rc, Register::CR &cr) {
        if (!IsRegValid(rt) || !IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, divwu, "Invalid registers detected!"));
            return;
        }
        u64 result = (u32)m_gpr[ra] / (u32)m_gpr[rb];
        if (m_gpr[ra] == 0x80000000 && m_gpr[rb] == 0xFFFFFFFF) {
            result = 0x7FFFFFFF;
        } else if (m_gpr[rb] == 0) {
            result = 0x7FFFFFFF;
        }
        if (oe && result > 0xFFFFFFFF) {
            XER_SET_OV(m_xer, true);
            XER_SET_SO(m_xer, true);
        }
        if (rc) {
            cr.cmp(0, (s32)m_gpr[rt], 0, m_xer);
        }
    }

    // Compare

    void FixedPointProcessor::cmpi(u8 bf, bool l, u8 ra, s16 si, Register::CR &cr) {
        if (!IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, cmpi, "Invalid registers detected!"));
            return;
        }
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmpi,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (s32)m_gpr[ra], si, m_xer);
    }

    void FixedPointProcessor::cmp(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmp, "Invalid registers detected!"));
            return;
        }
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmp,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (s32)m_gpr[ra], (s32)m_gpr[rb], m_xer);
    }

    void FixedPointProcessor::cmpli(u8 bf, bool l, u8 ra, u16 ui, Register::CR &cr) {
        if (!IsRegValid(ra)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, cmpli, "Invalid registers detected!"));
            return;
        }
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmpli,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (u32)m_gpr[ra], (u32)ui, m_xer);
    }

    void FixedPointProcessor::cmpl(u8 bf, bool l, u8 ra, u8 rb, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, cmpl, "Invalid registers detected!"));
            return;
        }
        // Double comparison unsupported (32-bit)
        if (l == 1) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, cmpl,
                                          "Double comparison (l == 1) is invalid!"));
            return;
        }
        cr.cmp(bf, (u32)m_gpr[ra], (u32)m_gpr[rb], m_xer);
    }

    // Trap

    void FixedPointProcessor::twi(u8 to, u8 ra, s16 si) {}
    void FixedPointProcessor::tw(u8 to, u8 ra, u8 rb) {}

    // Logic

    void FixedPointProcessor::andi(u8 ra, u8 rs, u16 ui, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, andi, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] & ui;
        m_gpr[ra]  = result & 0xFFFFFFFF;
        cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
    }

    void FixedPointProcessor::andis(u8 ra, u8 rs, u16 ui, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, andis, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] & ((u64)ui << 16);
        m_gpr[ra]  = result & 0xFFFFFFFF;
        cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
    }

    void FixedPointProcessor::ori(u8 ra, u8 rs, u16 ui) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, ori, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] | ui;
        m_gpr[ra]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::oris(u8 ra, u8 rs, u16 ui) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, oris, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] | ((u64)ui << 16);
        m_gpr[ra]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::xori(u8 ra, u8 rs, u16 ui) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, xori, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] ^ ui;
        m_gpr[ra]  = result & 0xFFFFFFFF;
    }

    void FixedPointProcessor::and_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, and_, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] & m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::or_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, or_, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] | m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::xor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, xor_, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] ^ m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::nand_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, nand_, "Invalid registers detected!"));
            return;
        }
        u64 result = ~(m_gpr[rs] & m_gpr[rb]);
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::nor_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, nor_, "Invalid registers detected!"));
            return;
        }
        u64 result = ~(m_gpr[rs] | m_gpr[rb]);
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::eqv_(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, eqv_, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] ^ m_gpr[rb];
        m_gpr[ra]  = ~(result)&0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::andc(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, andc, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] & ~m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }
    void FixedPointProcessor::orc(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, orc, "Invalid registers detected!"));
            return;
        }
        u64 result = m_gpr[rs] | ~m_gpr[rb];
        m_gpr[ra]  = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::extsb(u8 ra, u8 rs, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, extsb, "Invalid registers detected!"));
            return;
        }
        m_gpr[ra] = std::bit_cast<u64, s64>((s64)m_gpr[rs]) & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::extsh(u8 ra, u8 rs, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, extsh, "Invalid registers detected!"));
            return;
        }
        m_gpr[ra] = std::bit_cast<u64, s64>((s64)m_gpr[rs]) & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::cntlzw(u8 ra, u8 rs, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, cntlzw, "Invalid registers detected!"));
            return;
        }
        u64 result = 0;
        while (result < 32) {
            if ((m_gpr[rs] & SIG_BIT((u8)result, 32)) != 0) {
                break;
            }
            result += 1;
        }
        m_gpr[ra] = result & 0xFFFFFFFF;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    // Rotate | shift

    void FixedPointProcessor::rlwinm(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, rlwinm, "Invalid registers detected!"));
            return;
        }
        u32 mask  = MakeRotationMask(mb, me);
        m_gpr[ra] = std::rotl(m_gpr[rs], sh & 0x1F) & mask;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::rlwnm(u8 ra, u8 rs, u8 rb, u8 mb, u8 me, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, rlwnm, "Invalid registers detected!"));
            return;
        }
        u32 mask  = MakeRotationMask(mb, me);
        m_gpr[ra] = std::rotl(m_gpr[rs], m_gpr[rb] & 0x1F) & mask;
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::rlwimi(u8 ra, u8 rs, u8 sh, u8 mb, u8 me, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, rlwimi, "Invalid registers detected!"));
            return;
        }
        u32 mask  = MakeRotationMask(mb, me);
        m_gpr[ra] = (m_gpr[ra] & ~mask) | (std::rotl(m_gpr[rs], sh & 0x1F) & mask);
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::slw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, slw, "Invalid registers detected!"));
            return;
        }
        u32 amount = m_gpr[rb];
        m_gpr[ra]  = (amount & 0x20) ? 0 : m_gpr[rs] << (amount & 0x1F);
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::srw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, srw, "Invalid registers detected!"));
            return;
        }
        u32 amount = m_gpr[rb];
        m_gpr[ra]  = (amount & 0x20) ? 0 : m_gpr[rs] >> (amount & 0x1F);
        if (rc) {
            cr.cmp(0, (s32)m_gpr[ra], 0, m_xer);
        }
    }

    void FixedPointProcessor::srawi(u8 ra, u8 rs, u8 sh, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, srawi, "Invalid registers detected!"));
            return;
        }
        u32 amount    = sh;
        const s32 rrs = s32(m_gpr[rs]);

        m_gpr[ra] = u32(rrs >> amount);
        XER_SET_CA(m_xer, rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);
    }

    void FixedPointProcessor::sraw(u8 ra, u8 rs, u8 rb, bool rc, Register::CR &cr) {
        if (!IsRegValid(ra) || !IsRegValid(rs) || !IsRegValid(rb)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, sraw, "Invalid registers detected!"));
            return;
        }
        u32 amount = m_gpr[rb];

        if ((rb & 0x20) != 0) {
            if ((m_gpr[rs] & 0x80000000) != 0) {
                m_gpr[ra] = 0xFFFFFFFF;
                XER_SET_CA(m_xer, true);
            } else {
                m_gpr[ra] = 0x00000000;
                XER_SET_CA(m_xer, false);
            }
        } else {
            const u32 amount = rb & 0x1f;
            const s32 rrs    = s32(m_gpr[rs]);
            m_gpr[ra]        = u32(rrs >> amount);

            XER_SET_CA(m_xer, rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);
        }
    }

    // SPRs

    void FixedPointProcessor::mcrxr(Register::CR &cr, u8 crf) {
        if (crf > 7) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mtspr, "Invalid cr field detected!"));
            return;
        }

        u32 mask = (0b1111 << ((7 - crf) * 4));
        cr.m_crf = (cr.m_crf & ~mask) | ((m_xer & 0b1111) >> (crf * 4));
    }

    void FixedPointProcessor::mtspr(Register::SPRType spr, u8 rs, Register::LR &lr,
                                    Register::CTR &ctr) {
        if (!IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mtspr, "Invalid registers detected!"));
            return;
        }

        using namespace Register;
        switch (spr) {
        case SPRType::SPR_XER:
            m_xer = m_gpr[rs];
            break;
        case SPRType::SPR_LR:
            lr = m_gpr[rs];
            break;
        case SPRType::SPR_CTR:
            ctr = m_gpr[rs];
            break;
        default:
            TOOLBOX_ERROR_V("SPR Type {} unimplemented", magic_enum::enum_name(spr));
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, mtspr, "SPR Type unimplemented!"));
            return;
        }
    }
    void FixedPointProcessor::mfspr(Register::SPRType spr, u8 rt, const Register::LR &lr,
                                    const Register::CTR &ctr) {
        if (!IsRegValid(rt)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mfspr, "Invalid registers detected!"));
            return;
        }

        using namespace Register;
        switch (spr) {
        case SPRType::SPR_XER:
            m_gpr[rt] = m_xer;
            break;
        case SPRType::SPR_LR:
            m_gpr[rt] = lr;
            break;
        case SPRType::SPR_CTR:
            m_gpr[rt] = ctr;
            break;
        default:
            TOOLBOX_ERROR_V("SPR Type {} unimplemented", magic_enum::enum_name(spr));
            m_invalid_cb(PROC_INVALID_MSG(FixedPointProcessor, mfspr, "SPR Type unimplemented!"));
            return;
        }
    }

    void FixedPointProcessor::mftb(u8 rt, s16 tbr, const Register::TB &tb) {
        if (!IsRegValid(rt)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mftb, "Invalid registers detected!"));
            return;
        }
        if (tbr == 268) {
            m_gpr[rt] = tb & 0xFFFFFFFF;
        } else {
            m_gpr[rt] = (tb >> 32) & 0xFFFFFFFF;
        }
    }

#define CR_MSK(crm, i) ((crm & (0b1 << 7)) << 2)

    void FixedPointProcessor::mtcrf(u16 crm, u8 rs, Register::CR &cr) {
        if (!IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mtcrf, "Invalid registers detected!"));
            return;
        }

        u32 mask = [&]() {
            u32 expanded = 0;
            for (int i = 0; i < 8; ++i) {
                // Check if the i-th bit is set
                if (crm & (1 << i)) {
                    // Set 4 bits in the expanded bitmask
                    expanded |= (0xF << (i * 4));
                }
            }
            return expanded;
        }();

        cr.m_crf = (cr.m_crf & ~mask) | (m_gpr[rs] & mask);
    }

    void FixedPointProcessor::mfcr(u8 rt, const Register::CR &cr) {
        if (!IsRegValid(rt)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mfcr, "Invalid registers detected!"));
            return;
        }
        m_gpr[rt] = cr.m_crf;
    }

    void FixedPointProcessor::mtmsr(u8 rs, Register::MSR &msr) {
        if (!IsRegValid(rs)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mtmsr, "Invalid registers detected!"));
            return;
        }
        msr = m_gpr[rs];
    }

    void FixedPointProcessor::mfmsr(u8 rt, const Register::MSR &msr) {
        if (!IsRegValid(rt)) {
            m_invalid_cb(
                PROC_INVALID_MSG(FixedPointProcessor, mfmsr, "Invalid registers detected!"));
            return;
        }
        m_gpr[rt] = msr;
    }

    // External control

    void FixedPointProcessor::eciwx(u8 rt, u8 ra, u8 rb, Buffer &storage) { m_gpr[rt] = 0; }
    void FixedPointProcessor::ecowx(u8 rs, u8 ra, u8 rb, Buffer &storage) {}

}  // namespace Toolbox::Interpreter