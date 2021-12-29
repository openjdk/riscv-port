/*
 * Copyright (c) 1997, 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef CPU_RISCV_ASSEMBLER_RISCV_B_HPP
#define CPU_RISCV_ASSEMBLER_RISCV_B_HPP

#define INSN(NAME, op, funct3, funct7)                  \
  void NAME(Register Rd, Register Rs1, Register Rs2) {  \
    unsigned insn = 0;                                  \
    patch((address)&insn, 6,  0, op);                   \
    patch((address)&insn, 14, 12, funct3);              \
    patch((address)&insn, 31, 25, funct7);              \
    patch_reg((address)&insn, 7, Rd);                   \
    patch_reg((address)&insn, 15, Rs1);                 \
    patch_reg((address)&insn, 20, Rs2);                 \
    emit(insn);                                         \
  }

  INSN(add_uw, 0b0111011, 0b000, 0b0000100);
  INSN(rol,    0b0110011, 0b001, 0b0110000);
  INSN(rolw,   0b0111011, 0b001, 0b0110000);
  INSN(ror,    0b0110011, 0b101, 0b0110000);
  INSN(rorw,   0b0111011, 0b101, 0b0110000);

#undef INSN

#define INSN(NAME, op, funct3, funct12)                 \
  void NAME(Register Rd, Register Rs1) {                \
    unsigned insn = 0;                                  \
    patch((address)&insn, 6, 0, op);                    \
    patch((address)&insn, 14, 12, funct3);              \
    patch((address)&insn, 31, 20, funct12);             \
    patch_reg((address)&insn, 7, Rd);                   \
    patch_reg((address)&insn, 15, Rs1);                 \
    emit(insn);                                         \
  }

  INSN(sext_b, 0b0010011, 0b001, 0b011000000100);
  INSN(sext_h, 0b0010011, 0b001, 0b011000000101);
  INSN(zext_h, 0b0111011, 0b100, 0b000010000000);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                  \
  void NAME(Register Rd, Register Rs1, unsigned shamt) {\
    guarantee(shamt <= 0x3f, "Shamt is invalid");       \
    unsigned insn = 0;                                  \
    patch((address)&insn, 6, 0, op);                    \
    patch((address)&insn, 14, 12, funct3);              \
    patch((address)&insn, 25, 20, shamt);               \
    patch((address)&insn, 31, 26, funct6);              \
    patch_reg((address)&insn, 7, Rd);                   \
    patch_reg((address)&insn, 15, Rs1);                 \
    emit(insn);                                         \
  }

  INSN(rori, 0b0010011, 0b101, 0b011000);

#undef INSN

#define INSN(NAME, op, funct3, funct7)                  \
  void NAME(Register Rd, Register Rs1, unsigned shamt){ \
    guarantee(shamt <= 0x1f, "Shamt is invalid");       \
    unsigned insn = 0;                                  \
    patch((address)&insn, 6, 0, op);                    \
    patch((address)&insn, 14, 12, funct3);              \
    patch((address)&insn, 24, 20, shamt);               \
    patch((address)&insn, 31, 25, funct7);              \
    patch_reg((address)&insn, 7, Rd);                   \
    patch_reg((address)&insn, 15, Rs1);                 \
    emit(insn);                                         \
  }

  INSN(roriw, 0b0011011, 0b101, 0b0110000);

#undef INSN

// RVB pseudo instructions
// zero extend word
void zext_w(Register Rd, Register Rs) {
  add_uw(Rd, Rs, zr);
}


#endif // CPU_RISCV_ASSEMBLER_RISCV_B_HPP
