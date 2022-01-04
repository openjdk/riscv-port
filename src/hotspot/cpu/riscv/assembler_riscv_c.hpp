/*
 * Copyright (c) 1997, 2021, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2022, 2022, Alibaba Group Holding Limited. All rights reserved.
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

#ifndef CPU_RISCV_ASSEMBLER_RISCV_C_HPP
#define CPU_RISCV_ASSEMBLER_RISCV_C_HPP

private:
  bool _in_compressible_region;
public:
  bool in_compressible_region() const { return _in_compressible_region; }
  void set_in_compressible_region(bool b) { _in_compressible_region = b; }
public:

  // RVC: If an instruction is compressible, then
  //   we will implicitly emit a 16-bit compressed instruction instead of the 32-bit
  //   instruction in Assembler. All below logic follows Chapter -
  //   "C" Standard Extension for Compressed Instructions, Version 2.0.
  //   We can get code size reduction and performance improvement with this extension,
  //   considering the reduction of instruction size and the code density increment.

  // Note:
  //   1. When UseRVC is enabled, 32-bit instructions under 'CompressibleRegion's will be
  //      transformed to 16-bit instructions if compressible.
  //   2. RVC instructions in Assembler always begin with 'c_' prefix, as 'c_li',
  //      but most of time we have no need to explicitly use these instructions.
  //   3. We introduce 'CompressibleRegion' to hint instructions in this Region's RTTI range
  //      are qualified to change to their 2-byte versions.
  //      An example:
  //
  //        CompressibleRegion cr(_masm);
  //        __ andr(...);      // this instruction could change to c.and if able to
  //
  //   4. Using -XX:PrintAssemblyOptions=no-aliases could print RVC instructions instead of
  //      normal ones.
  //

  // RVC: extract a 16-bit instruction.
  static inline uint16_t c_extract(uint16_t val, unsigned msb, unsigned lsb) {
    assert_cond(msb >= lsb && msb <= 15);
    unsigned nbits = msb - lsb + 1;
    uint16_t mask = (1U << nbits) - 1;
    uint16_t result = val >> lsb;
    result &= mask;
    return result;
  }

  static inline int16_t c_sextract(uint16_t val, unsigned msb, unsigned lsb) {
    assert_cond(msb >= lsb && msb <= 15);
    int16_t result = val << (15 - msb);
    result >>= (15 - msb + lsb);
    return result;
  }

  // RVC: patch a 16-bit instruction.
  static void c_patch(address a, unsigned msb, unsigned lsb, uint16_t val) {
    assert_cond(a != NULL);
    assert_cond(msb >= lsb && msb <= 15);
    unsigned nbits = msb - lsb + 1;
    guarantee(val < (1U << nbits), "Field too big for insn");
    uint16_t mask = (1U << nbits) - 1;
    val <<= lsb;
    mask <<= lsb;
    uint16_t target = *(uint16_t *)a;
    target &= ~mask;
    target |= val;
    *(uint16_t *)a = target;
  }

  static void c_patch(address a, unsigned bit, uint16_t val) {
    c_patch(a, bit, bit, val);
  }

  // RVC: patch a 16-bit instruction with a general purpose register ranging [0, 31] (5 bits)
  static void c_patch_reg(address a, unsigned lsb, Register reg) {
    c_patch(a, lsb + 4, lsb, reg->encoding_nocheck());
  }

  // RVC: patch a 16-bit instruction with a general purpose register ranging [8, 15] (3 bits)
  static void c_patch_compressed_reg(address a, unsigned lsb, Register reg) {
    c_patch(a, lsb + 2, lsb, reg->compressed_encoding_nocheck());
  }

  // RVC: patch a 16-bit instruction with a float register ranging [0, 31] (5 bits)
  static void c_patch_reg(address a, unsigned lsb, FloatRegister reg) {
    c_patch(a, lsb + 4, lsb, reg->encoding_nocheck());
  }

  // RVC: patch a 16-bit instruction with a float register ranging [8, 15] (3 bits)
  static void c_patch_compressed_reg(address a, unsigned lsb, FloatRegister reg) {
    c_patch(a, lsb + 2, lsb, reg->compressed_encoding_nocheck());
  }

public:

// RVC: Compressed Instructions

// --------------  RVC Instruction Definitions  --------------

  void c_nop() {
    c_addi(x0, 0);
  }

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd_Rs1, int32_t imm) {                                                  \
    assert_cond(is_imm_in_range(imm, 6, 0));                                                 \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 6, 2, (imm & right_n_bits(5)));                                  \
    c_patch_reg((address)&insn, 7, Rd_Rs1);                                                  \
    c_patch((address)&insn, 12, 12, (imm & nth_bit(5)) >> 5);                                \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_addi,   0b000, 0b01);
  INSN(c_addiw,  0b001, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(int32_t imm) {                                                                   \
    assert_cond(is_imm_in_range(imm, 10, 0));                                                \
    assert_cond((imm & 0b1111) == 0);                                                        \
    assert_cond(imm != 0);                                                                   \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 2, 2, (imm & nth_bit(5)) >> 5);                                  \
    c_patch((address)&insn, 4, 3, (imm & right_n_bits(9)) >> 7);                             \
    c_patch((address)&insn, 5, 5, (imm & nth_bit(6)) >> 6);                                  \
    c_patch((address)&insn, 6, 6, (imm & nth_bit(4)) >> 4);                                  \
    c_patch_reg((address)&insn, 7, sp);                                                      \
    c_patch((address)&insn, 12, 12, (imm & nth_bit(9)) >> 9);                                \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_addi16sp, 0b011, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, uint32_t uimm) {                                                    \
    assert_cond(is_unsigned_imm_in_range(uimm, 10, 0));                                      \
    assert_cond((uimm & 0b11) == 0);                                                         \
    assert_cond(uimm != 0);                                                                  \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_compressed_reg((address)&insn, 2, Rd);                                           \
    c_patch((address)&insn, 5, 5, (uimm & nth_bit(3)) >> 3);                                 \
    c_patch((address)&insn, 6, 6, (uimm & nth_bit(2)) >> 2);                                 \
    c_patch((address)&insn, 10, 7, (uimm & right_n_bits(10)) >> 6);                          \
    c_patch((address)&insn, 12, 11, (uimm & right_n_bits(6)) >> 4);                          \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_addi4spn, 0b000, 0b00);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd_Rs1, uint32_t shamt) {                                               \
    assert_cond(is_unsigned_imm_in_range(shamt, 6, 0));                                      \
    assert_cond(shamt != 0);                                                                 \
    assert_cond(Rd_Rs1 != x0);                                                               \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 6, 2, (shamt & right_n_bits(5)));                                \
    c_patch_reg((address)&insn, 7, Rd_Rs1);                                                  \
    c_patch((address)&insn, 12, 12, (shamt & nth_bit(5)) >> 5);                              \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_slli, 0b000, 0b10);

#undef INSN

#define INSN(NAME, funct3, funct2, op)                                                       \
  void NAME(Register Rd_Rs1, uint32_t shamt) {                                               \
    assert_cond(is_unsigned_imm_in_range(shamt, 6, 0));                                      \
    assert_cond(shamt != 0);                                                                 \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 6, 2, (shamt & right_n_bits(5)));                                \
    c_patch_compressed_reg((address)&insn, 7, Rd_Rs1);                                       \
    c_patch((address)&insn, 11, 10, funct2);                                                 \
    c_patch((address)&insn, 12, 12, (shamt & nth_bit(5)) >> 5);                              \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_srli, 0b100, 0b00, 0b01);
  INSN(c_srai, 0b100, 0b01, 0b01);

#undef INSN

#define INSN(NAME, funct3, funct2, op)                                                       \
  void NAME(Register Rd_Rs1, int32_t imm) {                                                  \
    assert_cond(is_imm_in_range(imm, 6, 0));                                                 \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 6, 2, (imm & right_n_bits(5)));                                  \
    c_patch_compressed_reg((address)&insn, 7, Rd_Rs1);                                       \
    c_patch((address)&insn, 11, 10, funct2);                                                 \
    c_patch((address)&insn, 12, 12, (imm & nth_bit(5)) >> 5);                                \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_andi, 0b100, 0b10, 0b01);

#undef INSN

#define INSN(NAME, funct6, funct2, op)                                                       \
  void NAME(Register Rd_Rs1, Register Rs2) {                                                 \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_compressed_reg((address)&insn, 2, Rs2);                                          \
    c_patch((address)&insn, 6, 5, funct2);                                                   \
    c_patch_compressed_reg((address)&insn, 7, Rd_Rs1);                                       \
    c_patch((address)&insn, 15, 10, funct6);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_sub,  0b100011, 0b00, 0b01);
  INSN(c_xor,  0b100011, 0b01, 0b01);
  INSN(c_or,   0b100011, 0b10, 0b01);
  INSN(c_and,  0b100011, 0b11, 0b01);
  INSN(c_subw, 0b100111, 0b00, 0b01);
  INSN(c_addw, 0b100111, 0b01, 0b01);

#undef INSN

#define INSN(NAME, funct4, op)                                                               \
  void NAME(Register Rd_Rs1, Register Rs2) {                                                 \
    assert_cond(Rd_Rs1 != x0);                                                               \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_reg((address)&insn, 2, Rs2);                                                     \
    c_patch_reg((address)&insn, 7, Rd_Rs1);                                                  \
    c_patch((address)&insn, 15, 12, funct4);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_mv,  0b1000, 0b10);
  INSN(c_add, 0b1001, 0b10);

#undef INSN

#define INSN(NAME, funct4, op)                                                               \
  void NAME(Register Rs1) {                                                                  \
    assert_cond(Rs1 != x0);                                                                  \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_reg((address)&insn, 2, x0);                                                      \
    c_patch_reg((address)&insn, 7, Rs1);                                                     \
    c_patch((address)&insn, 15, 12, funct4);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_jr,   0b1000, 0b10);
  INSN(c_jalr, 0b1001, 0b10);

#undef INSN

  typedef void (Assembler::* j_c_insn)(address dest);
  typedef void (Assembler::* compare_and_branch_c_insn)(Register Rs1, address dest);

  void wrap_label(Label &L, j_c_insn insn) {
    if (L.is_bound()) {
      (this->*insn)(target(L));
    } else {
      L.add_patch_at(code(), locator());
      (this->*insn)(pc());
    }
  }

  void wrap_label(Label &L, Register r, compare_and_branch_c_insn insn) {
    if (L.is_bound()) {
      (this->*insn)(r, target(L));
    } else {
      L.add_patch_at(code(), locator());
      (this->*insn)(r, pc());
    }
  }

#define INSN(NAME, funct3, op)                                                               \
  void NAME(int32_t offset) {                                                                \
    assert_cond(is_imm_in_range(offset, 11, 1));                                             \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 2, 2, (offset & nth_bit(5)) >> 5);                               \
    c_patch((address)&insn, 5, 3, (offset & right_n_bits(4)) >> 1);                          \
    c_patch((address)&insn, 6, 6, (offset & nth_bit(7)) >> 7);                               \
    c_patch((address)&insn, 7, 7, (offset & nth_bit(6)) >> 6);                               \
    c_patch((address)&insn, 8, 8, (offset & nth_bit(10)) >> 10);                             \
    c_patch((address)&insn, 10, 9, (offset & right_n_bits(10)) >> 8);                        \
    c_patch((address)&insn, 11, 11, (offset & nth_bit(4)) >> 4);                             \
    c_patch((address)&insn, 12, 12, (offset & nth_bit(11)) >> 11);                           \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }                                                                                          \
  void NAME(address dest) {                                                                  \
    assert_cond(dest != NULL);                                                               \
    int64_t distance = dest - pc();                                                          \
    assert_cond(is_imm_in_range(distance, 11, 1));                                           \
    c_j(distance);                                                                           \
  }                                                                                          \
  void NAME(Label &L) {                                                                      \
    wrap_label(L, &Assembler::NAME);                                                         \
  }

  INSN(c_j, 0b101, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rs1, int32_t imm) {                                                     \
    assert_cond(is_imm_in_range(imm, 8, 1));                                                 \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 2, 2, (imm & nth_bit(5)) >> 5);                                  \
    c_patch((address)&insn, 4, 3, (imm & right_n_bits(3)) >> 1);                             \
    c_patch((address)&insn, 6, 5, (imm & right_n_bits(8)) >> 6);                             \
    c_patch_compressed_reg((address)&insn, 7, Rs1);                                          \
    c_patch((address)&insn, 11, 10, (imm & right_n_bits(5)) >> 3);                           \
    c_patch((address)&insn, 12, 12, (imm & nth_bit(8)) >> 8);                                \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }                                                                                          \
  void NAME(Register Rs1, address dest) {                                                    \
    assert_cond(dest != NULL);                                                               \
    int64_t distance = dest - pc();                                                          \
    assert_cond(is_imm_in_range(distance, 8, 1));                                            \
    NAME(Rs1, distance);                                                                     \
  }                                                                                          \
  void NAME(Register Rs1, Label &L) {                                                        \
    wrap_label(L, Rs1, &Assembler::NAME);                                                    \
  }

  INSN(c_beqz, 0b110, 0b01);
  INSN(c_bnez, 0b111, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, int32_t imm) {                                                      \
    assert_cond(is_imm_in_range(imm, 18, 0));                                                \
    assert_cond((imm & 0xfff) == 0);                                                         \
    assert_cond(imm != 0);                                                                   \
    assert_cond(Rd != x0 && Rd != x2);                                                       \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 6, 2, (imm & right_n_bits(17)) >> 12);                           \
    c_patch_reg((address)&insn, 7, Rd);                                                      \
    c_patch((address)&insn, 12, 12, (imm & nth_bit(17)) >> 17);                              \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_lui, 0b011, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, int32_t imm) {                                                      \
    assert_cond(is_imm_in_range(imm, 6, 0));                                                 \
    assert_cond(Rd != x0);                                                                   \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 6, 2, (imm & right_n_bits(5)));                                  \
    c_patch_reg((address)&insn, 7, Rd);                                                      \
    c_patch((address)&insn, 12, 12, (imm & right_n_bits(6)) >> 5);                           \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_li, 0b010, 0b01);

#undef INSN

#define INSN(NAME, funct3, op, REGISTER_TYPE, CHECK)                                         \
  void NAME(REGISTER_TYPE Rd, uint32_t uimm) {                                               \
    assert_cond(is_unsigned_imm_in_range(uimm, 9, 0));                                       \
    assert_cond((uimm & 0b111) == 0);                                                        \
    IF(CHECK, assert_cond(Rd != x0);)                                                        \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 4, 2, (uimm & right_n_bits(9)) >> 6);                            \
    c_patch((address)&insn, 6, 5, (uimm & right_n_bits(5)) >> 3);                            \
    c_patch_reg((address)&insn, 7, Rd);                                                      \
    c_patch((address)&insn, 12, 12, (uimm & nth_bit(5)) >> 5);                               \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

#define IF(BOOL, ...)       IF_##BOOL(__VA_ARGS__)
#define IF_true(code)       code
#define IF_false(code)

  INSN(c_ldsp,  0b011, 0b10, Register,      true);
  INSN(c_fldsp, 0b001, 0b10, FloatRegister, false);

#undef IF_false
#undef IF_true
#undef IF
#undef INSN

#define INSN(NAME, funct3, op, REGISTER_TYPE)                                                \
  void NAME(REGISTER_TYPE Rd_Rs2, Register Rs1, uint32_t uimm) {                             \
    assert_cond(is_unsigned_imm_in_range(uimm, 8, 0));                                       \
    assert_cond((uimm & 0b111) == 0);                                                        \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_compressed_reg((address)&insn, 2, Rd_Rs2);                                       \
    c_patch((address)&insn, 6, 5, (uimm & right_n_bits(8)) >> 6);                            \
    c_patch_compressed_reg((address)&insn, 7, Rs1);                                          \
    c_patch((address)&insn, 12, 10, (uimm & right_n_bits(6)) >> 3);                          \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_ld,  0b011, 0b00, Register);
  INSN(c_sd,  0b111, 0b00, Register);
  INSN(c_fld, 0b001, 0b00, FloatRegister);
  INSN(c_fsd, 0b101, 0b00, FloatRegister);

#undef INSN

#define INSN(NAME, funct3, op, REGISTER_TYPE)                                                \
  void NAME(REGISTER_TYPE Rs2, uint32_t uimm) {                                              \
    assert_cond(is_unsigned_imm_in_range(uimm, 9, 0));                                       \
    assert_cond((uimm & 0b111) == 0);                                                        \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_reg((address)&insn, 2, Rs2);                                                     \
    c_patch((address)&insn, 9, 7, (uimm & right_n_bits(9)) >> 6);                            \
    c_patch((address)&insn, 12, 10, (uimm & right_n_bits(6)) >> 3);                          \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_sdsp,  0b111, 0b10, Register);
  INSN(c_fsdsp, 0b101, 0b10, FloatRegister);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rs2, uint32_t uimm) {                                                   \
    assert_cond(is_unsigned_imm_in_range(uimm, 8, 0));                                       \
    assert_cond((uimm & 0b11) == 0);                                                         \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_reg((address)&insn, 2, Rs2);                                                     \
    c_patch((address)&insn, 8, 7, (uimm & right_n_bits(8)) >> 6);                            \
    c_patch((address)&insn, 12, 9, (uimm & right_n_bits(6)) >> 2);                           \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_swsp, 0b110, 0b10);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, uint32_t uimm) {                                                    \
    assert_cond(is_unsigned_imm_in_range(uimm, 8, 0));                                       \
    assert_cond((uimm & 0b11) == 0);                                                         \
    assert_cond(Rd != x0);                                                                   \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 3, 2, (uimm & right_n_bits(8)) >> 6);                            \
    c_patch((address)&insn, 6, 4, (uimm & right_n_bits(5)) >> 2);                            \
    c_patch_reg((address)&insn, 7, Rd);                                                      \
    c_patch((address)&insn, 12, 12, (uimm & nth_bit(5)) >> 5);                               \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_lwsp, 0b010, 0b10);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd_Rs2, Register Rs1, uint32_t uimm) {                                  \
    assert_cond(is_unsigned_imm_in_range(uimm, 7, 0));                                       \
    assert_cond((uimm & 0b11) == 0);                                                         \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch_compressed_reg((address)&insn, 2, Rd_Rs2);                                       \
    c_patch((address)&insn, 5, 5, (uimm & nth_bit(6)) >> 6);                                 \
    c_patch((address)&insn, 6, 6, (uimm & nth_bit(2)) >> 2);                                 \
    c_patch_compressed_reg((address)&insn, 7, Rs1);                                          \
    c_patch((address)&insn, 12, 10, (uimm & right_n_bits(6)) >> 3);                          \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_lw, 0b010, 0b00);
  INSN(c_sw, 0b110, 0b00);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME() {                                                                              \
    uint16_t insn = 0;                                                                       \
    c_patch((address)&insn, 1, 0, op);                                                       \
    c_patch((address)&insn, 11, 2, 0x0);                                                     \
    c_patch((address)&insn, 12, 12, 0b1);                                                    \
    c_patch((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(c_ebreak, 0b100, 0b10);

#undef INSN

// --------------  RVC Transformation Macros  --------------

// two RVC macros
#define COMPRESSIBLE          true
#define NOT_COMPRESSIBLE      false

// a pivotal dispatcher for RVC
#define EMIT_MAY_COMPRESS(C, NAME, ...)               EMIT_MAY_COMPRESS_##C(NAME, __VA_ARGS__)
#define EMIT_MAY_COMPRESS_true(NAME, ...)             EMIT_MAY_COMPRESS_##NAME(__VA_ARGS__)
#define EMIT_MAY_COMPRESS_false(NAME, ...)

#define IS_COMPRESSIBLE(...)                          if (__VA_ARGS__)
#define CHECK_CEXT_AND_COMPRESSIBLE(...)              IS_COMPRESSIBLE(UseRVC && in_compressible_region() && __VA_ARGS__)
#define CHECK_CEXT()                                  if (UseRVC && in_compressible_region())

// RVC transformation macros
#define EMIT_RVC_cond(PREFIX, COND, EMIT) {                                            \
    PREFIX                                                                             \
    CHECK_CEXT_AND_COMPRESSIBLE(COND) {                                                \
      EMIT;                                                                            \
      return;                                                                          \
    }                                                                                  \
  }

#define EMIT_RVC_cond2(PREFIX, COND1, EMIT1, COND2, EMIT2) {                           \
    PREFIX                                                                             \
    CHECK_CEXT() {                                                                     \
      IS_COMPRESSIBLE(COND1) {                                                         \
        EMIT1;                                                                         \
        return;                                                                        \
      } else IS_COMPRESSIBLE(COND2) {                                                  \
        EMIT2;                                                                         \
        return;                                                                        \
      }                                                                                \
    }                                                                                  \
  }

#define EMIT_RVC_cond4(PREFIX, COND1, EMIT1, COND2, EMIT2, COND3, EMIT3, COND4, EMIT4) {  \
    PREFIX                                                                             \
    CHECK_CEXT() {                                                                     \
      IS_COMPRESSIBLE(COND1) {                                                         \
        EMIT1;                                                                         \
        return;                                                                        \
      } else IS_COMPRESSIBLE(COND2) {                                                  \
        EMIT2;                                                                         \
        return;                                                                        \
      } else IS_COMPRESSIBLE(COND3) {                                                  \
        EMIT3;                                                                         \
        return;                                                                        \
      } else IS_COMPRESSIBLE(COND4) {                                                  \
        EMIT4;                                                                         \
        return;                                                                        \
      }                                                                                \
    }                                                                                  \
  }

// --------------------------
// Register instructions
// --------------------------
// add -> c.add
#define EMIT_MAY_COMPRESS_add(Rd, Rs1, Rs2)                                            \
  EMIT_RVC_cond(                                                                       \
    Register src = noreg;,                                                             \
    Rs1 != x0 && Rs2 != x0 && ((src = Rs1, Rs2 == Rd) || (src = Rs2, Rs1 == Rd)),      \
    c_add(Rd, src)                                                                     \
  )

// --------------------------
// sub/subw -> c.sub/c.subw
#define EMIT_MAY_COMPRESS_sub_helper(C_NAME, Rd, Rs1, Rs2)                             \
  EMIT_RVC_cond(,                                                                      \
    Rs1 == Rd && Rd->is_compressed_valid() && Rs2->is_compressed_valid(),              \
    C_NAME(Rd, Rs2)                                                                    \
  )

#define EMIT_MAY_COMPRESS_sub(Rd, Rs1, Rs2)                                            \
  EMIT_MAY_COMPRESS_sub_helper(c_sub, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_subw(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_sub_helper(c_subw, Rd, Rs1, Rs2)

// --------------------------
// xor/or/and/addw -> c.xor/c.or/c.and/c.addw
#define EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(C_NAME, Rd, Rs1, Rs2)              \
  EMIT_RVC_cond(                                                                       \
    Register src = noreg;,                                                             \
    Rs1->is_compressed_valid() && Rs2->is_compressed_valid() &&                        \
      ((src = Rs1, Rs2 == Rd) || (src = Rs2, Rs1 == Rd)),                              \
    C_NAME(Rd, src)                                                                    \
  )

#define EMIT_MAY_COMPRESS_xorr(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(c_xor, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_orr(Rd, Rs1, Rs2)                                            \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(c_or, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_andr(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(c_and, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_addw(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(c_addw, Rd, Rs1, Rs2)

// --------------------------
// Load/store register (all modes)
// --------------------------
private:

#define FUNC(NAME, funct3, bits)                                                       \
  bool NAME(Register rs1, Register rd_rs2, int32_t imm12, bool ld) {                   \
    return rs1 == sp &&                                                                \
      is_unsigned_imm_in_range(imm12, bits, 0) &&                                      \
      (intx(imm12) & funct3) == 0x0 &&                                                 \
      (!ld || rd_rs2 != x0);                                                           \
  }                                                                                    \

  FUNC(is_c_ldsdsp,  0b111, 9);
  FUNC(is_c_lwswsp,  0b011, 8);
#undef FUNC

#define FUNC(NAME, funct3, bits)                                                       \
  bool NAME(Register rs1, int32_t imm12) {                                             \
    return rs1 == sp &&                                                                \
      is_unsigned_imm_in_range(imm12, bits, 0) &&                                      \
      (intx(imm12) & funct3) == 0x0;                                                   \
  }                                                                                    \

  FUNC(is_c_fldsdsp, 0b111, 9);
#undef FUNC

#define FUNC(NAME, REG_TYPE, funct3, bits)                                             \
  bool NAME(Register rs1, REG_TYPE rd_rs2, int32_t imm12) {                            \
    return rs1->is_compressed_valid() &&                                               \
      rd_rs2->is_compressed_valid() &&                                                 \
      is_unsigned_imm_in_range(imm12, bits, 0) &&                                      \
      (intx(imm12) & funct3) == 0x0;                                                   \
  }                                                                                    \

  FUNC(is_c_ldsd,  Register,      0b111, 8);
  FUNC(is_c_lwsw,  Register,      0b011, 7);
  FUNC(is_c_fldsd, FloatRegister, 0b111, 8);
#undef FUNC

public:
// --------------------------
// ld -> c.ldsp/c.ld
#define EMIT_MAY_COMPRESS_ld(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_c_ldsdsp(Rs, Rd, offset, true),                                                \
     c_ldsp(Rd, offset),                                                               \
     is_c_ldsd(Rs, Rd, offset),                                                        \
     c_ld(Rd, Rs, offset)                                                              \
  )

// --------------------------
// sd -> c.sdsp/c.sd
#define EMIT_MAY_COMPRESS_sd(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_c_ldsdsp(Rs, Rd, offset, false),                                               \
     c_sdsp(Rd, offset),                                                               \
     is_c_ldsd(Rs, Rd, offset),                                                        \
     c_sd(Rd, Rs, offset)                                                              \
  )

// --------------------------
// lw -> c.lwsp/c.lw
#define EMIT_MAY_COMPRESS_lw(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_c_lwswsp(Rs, Rd, offset, true),                                                \
     c_lwsp(Rd, offset),                                                               \
     is_c_lwsw(Rs, Rd, offset),                                                        \
     c_lw(Rd, Rs, offset)                                                              \
  )

// --------------------------
// sw -> c.swsp/c.sw
#define EMIT_MAY_COMPRESS_sw(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_c_lwswsp(Rs, Rd, offset, false),                                               \
     c_swsp(Rd, offset),                                                               \
     is_c_lwsw(Rs, Rd, offset),                                                        \
     c_sw(Rd, Rs, offset)                                                              \
  )

// --------------------------
// fld -> c.fldsp/c.fld
#define EMIT_MAY_COMPRESS_fld(Rd, Rs, offset)                                          \
  EMIT_RVC_cond2(,                                                                     \
     is_c_fldsdsp(Rs, offset),                                                         \
     c_fldsp(Rd, offset),                                                              \
     is_c_fldsd(Rs, Rd, offset),                                                       \
     c_fld(Rd, Rs, offset)                                                             \
  )

// --------------------------
// fsd -> c.fsdsp/c.fsd
#define EMIT_MAY_COMPRESS_fsd(Rd, Rs, offset)                                          \
  EMIT_RVC_cond2(,                                                                     \
     is_c_fldsdsp(Rs, offset),                                                         \
     c_fsdsp(Rd, offset),                                                              \
     is_c_fldsd(Rs, Rd, offset),                                                       \
     c_fsd(Rd, Rs, offset)                                                             \
  )

// --------------------------
// Conditional branch instructions
// --------------------------
// beq/bne -> c.beqz/c.bnez

// Note: offset == 0 means this beqz/benz is jumping forward and we cannot know the future position
//   so we cannot compress this instrution.
#define EMIT_MAY_COMPRESS_beqz_bnez_helper(C_NAME, Rs1, Rs2, offset)                   \
  EMIT_RVC_cond(,                                                                      \
    offset != 0 && Rs2 == x0 && Rs1->is_compressed_valid() &&                          \
      is_imm_in_range(offset, 8, 1),                                                   \
    C_NAME(Rs1, offset)                                                                \
  )

#define EMIT_MAY_COMPRESS_beq(Rs1, Rs2, offset)                                        \
  EMIT_MAY_COMPRESS_beqz_bnez_helper(c_beqz, Rs1, Rs2, offset)

#define EMIT_MAY_COMPRESS_bne(Rs1, Rs2, offset)                                        \
  EMIT_MAY_COMPRESS_beqz_bnez_helper(c_bnez, Rs1, Rs2, offset)

// --------------------------
// Unconditional branch instructions
// --------------------------
// jalr/jal -> c.jr/c.jalr/c.j

#define EMIT_MAY_COMPRESS_jalr(Rd, Rs, offset)                                         \
  EMIT_RVC_cond2(,                                                                     \
    offset == 0 && Rd == x1 && Rs != x0,                                               \
    c_jalr(Rs),                                                                        \
    offset == 0 && Rd == x0 && Rs != x0,                                               \
    c_jr(Rs)                                                                           \
  )

// Note: offset == 0 means this j() is jumping forward and we cannot know the future position
//   so we cannot compress this instrution.
#define EMIT_MAY_COMPRESS_jal(Rd, offset)                                              \
  EMIT_RVC_cond(,                                                                      \
    offset != 0 && Rd == x0 && is_imm_in_range(offset, 11, 1),                         \
    c_j(offset)                                                                        \
  )

// --------------------------
// Upper Immediate Instruction
// --------------------------
// lui -> c.lui
#define EMIT_MAY_COMPRESS_lui(Rd, imm)                                                 \
  EMIT_RVC_cond(,                                                                      \
    Rd != x0 && Rd != x2 && imm != 0 && is_imm_in_range(imm, 18, 0),                   \
    c_lui(Rd, imm)                                                                     \
  )

// --------------------------
// Miscellaneous Instructions
// --------------------------
// ebreak -> c.ebreak
#define EMIT_MAY_COMPRESS_ebreak()                                                     \
  EMIT_RVC_cond(,                                                                      \
    true,                                                                              \
    c_ebreak()                                                                         \
  )

// --------------------------
// Immediate Instructions
// --------------------------
// addi -> c.addi/c.nop/c.mv/c.addi16sp/c.addi4spn.
#define EMIT_MAY_COMPRESS_addi(Rd, Rs1, imm)                                                          \
  EMIT_RVC_cond4(,                                                                                    \
    Rd == Rs1 && is_imm_in_range(imm, 6, 0),                                                          \
    c_addi(Rd, imm),                                                                                  \
    imm == 0 && Rd != x0 && Rs1 != x0,                                                                \
    c_mv(Rd, Rs1),                                                                                    \
    Rs1 == sp && Rd == Rs1 && imm != 0 && (imm & 0b1111) == 0x0 && is_imm_in_range(imm, 10, 0),       \
    c_addi16sp(imm),                                                                                  \
    Rs1 == sp && Rd->is_compressed_valid() && imm != 0 && (imm & 0b11) == 0x0 && is_unsigned_imm_in_range(imm, 10, 0),  \
    c_addi4spn(Rd, imm)                                                                               \
  )

// --------------------------
// addiw -> c.addiw
#define EMIT_MAY_COMPRESS_addiw(Rd, Rs1, imm)                                          \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd != x0 && is_imm_in_range(imm, 6, 0),                               \
    c_addiw(Rd, imm)                                                                   \
  )

// --------------------------
// and_imm12 -> c.andi
#define EMIT_MAY_COMPRESS_and_imm12(Rd, Rs1, imm)                                      \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd->is_compressed_valid() && is_imm_in_range(imm, 6, 0),              \
    c_andi(Rd, imm)                                                                    \
  )

// --------------------------
// Shift Immediate Instructions
// --------------------------
// slli -> c.slli
#define EMIT_MAY_COMPRESS_slli(Rd, Rs1, shamt)                                         \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd != x0 && shamt != 0,                                               \
    c_slli(Rd, shamt)                                                                  \
  )

// --------------------------
// srai/srli -> c.srai/c.srli
#define EMIT_MAY_COMPRESS_srai_srli_helper(C_NAME, Rd, Rs1, shamt)                     \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd->is_compressed_valid() && shamt != 0,                              \
    C_NAME(Rd, shamt)                                                                  \
  )

#define EMIT_MAY_COMPRESS_srai(Rd, Rs1, shamt)                                         \
  EMIT_MAY_COMPRESS_srai_srli_helper(c_srai, Rd, Rs1, shamt)

#define EMIT_MAY_COMPRESS_srli(Rd, Rs1, shamt)                                         \
  EMIT_MAY_COMPRESS_srai_srli_helper(c_srli, Rd, Rs1, shamt)

// --------------------------

public:
// RVC: a compressible region
class CompressibleRegion : public StackObj {
protected:
  Assembler *_masm;
  bool _prev_in_compressible_region;
public:
  CompressibleRegion(Assembler *_masm)
  : _masm(_masm)
  , _prev_in_compressible_region(_masm->in_compressible_region()) {
    _masm->set_in_compressible_region(true);
  }
  ~CompressibleRegion() {
    _masm->set_in_compressible_region(_prev_in_compressible_region);
  }
};

#endif // CPU_RISCV_ASSEMBLER_RISCV_C_HPP