/*
 * Copyright (c) 1997, 2021, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2021, Alibaba Group Holding Limited. All rights reserved.
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

#ifndef CPU_RISCV_ASSEMBLER_RISCV_CEXT_HPP
#define CPU_RISCV_ASSEMBLER_RISCV_CEXT_HPP

  // C-Ext: If an instruction is compressible, then
  //   we will implicitly emit a 16-bit compressed instruction instead of the 32-bit
  //   instruction in Assembler. All below logic follows Chapter -
  //   "C" Standard Extension for Compressed Instructions, Version 2.0.
  //   We can get code size reduction and performance improvement with this extension,
  //   considering the reduction of instruction size and the code density increment.

  // Note:
  //   1. When UseRVC is enabled, some of normal instructions will be implicitly
  //      changed to its 16-bit version.
  //   2. C-Ext's instructions in Assembler always end with '_c' suffix, as 'li_c',
  //      but most of time we have no need to explicitly use these instructions.
  //      (Although spec says 'c.li', we use 'li_c' to unify related names - see below.
  //   3. In some cases, we need to force using one instruction's uncompressed version,
  //      for instance code being patched should remain its general and longest version
  //      to cover all possible cases, or code requiring a fixed length.
  //      So we introduce '_nc' suffix (short for: not compressible) to force an instruction
  //      to remain its normal 4-byte version.
  //     An example:
  //      j() (32-bit) could become j_c() (16-bit) with -XX:+UseRVC if compressible. We could
  //      use j_nc() to force it to remain its normal 4-byte version.
  //   4. Using -XX:PrintAssemblyOptions=no-aliases could print C-Ext instructions instead of
  //      normal ones.
  //

  // C-Ext: incompressible version
  void j_nc(const address &dest, Register temp = t0);
  void j_nc(const Address &adr, Register temp = t0) ;
  void j_nc(Label &l, Register temp = t0);
  void jal_nc(Label &l, Register temp = t0);
  void jal_nc(const address &dest, Register temp = t0);
  void jal_nc(const Address &adr, Register temp = t0);
  void jr_nc(Register Rs);
  void jalr_nc(Register Rs);
  void call_nc(const address &dest, Register temp = t0);
  void tail_nc(const address &dest, Register temp = t0);

  // C-Ext: extract a 16-bit instruction.
  static inline uint16_t extract_c(uint16_t val, unsigned msb, unsigned lsb) {
    assert_cond(msb >= lsb && msb <= 15);
    unsigned nbits = msb - lsb + 1;
    uint16_t mask = (1U << nbits) - 1;
    uint16_t result = val >> lsb;
    result &= mask;
    return result;
  }

  static inline int16_t sextract_c(uint16_t val, unsigned msb, unsigned lsb) {
    assert_cond(msb >= lsb && msb <= 15);
    int16_t result = val << (15 - msb);
    result >>= (15 - msb + lsb);
    return result;
  }

  // C-Ext: patch a 16-bit instruction.
  static void patch_c(address a, unsigned msb, unsigned lsb, uint16_t val) {
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

  static void patch_c(address a, unsigned bit, uint16_t val) {
    patch_c(a, bit, bit, val);
  }

  // C-Ext: patch a 16-bit instruction with a general purpose register ranging [0, 31] (5 bits)
  static void patch_reg_c(address a, unsigned lsb, Register reg) {
    patch_c(a, lsb + 4, lsb, reg->encoding_nocheck());
  }

  // C-Ext: patch a 16-bit instruction with a general purpose register ranging [8, 15] (3 bits)
  static void patch_compressed_reg_c(address a, unsigned lsb, Register reg) {
    patch_c(a, lsb + 2, lsb, reg->compressed_encoding_nocheck());
  }

  // C-Ext: patch a 16-bit instruction with a float register ranging [0, 31] (5 bits)
  static void patch_reg_c(address a, unsigned lsb, FloatRegister reg) {
    patch_c(a, lsb + 4, lsb, reg->encoding_nocheck());
  }

  // C-Ext: patch a 16-bit instruction with a float register ranging [8, 15] (3 bits)
  static void patch_compressed_reg_c(address a, unsigned lsb, FloatRegister reg) {
    patch_c(a, lsb + 2, lsb, reg->compressed_encoding_nocheck());
  }

public:

// C-Ext: Compressed Instructions

// --------------  C-Ext Instruction Definitions  --------------

  void nop_c() {
    addi_c(x0, 0);
  }

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd_Rs1, int32_t imm) {                                                  \
    assert_cond(is_imm_in_range(imm, 6, 0));                                                 \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 6, 2, (imm & right_n_bits(5)));                                  \
    patch_reg_c((address)&insn, 7, Rd_Rs1);                                                  \
    patch_c((address)&insn, 12, 12, (imm & nth_bit(5)) >> 5);                                \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(addi_c,   0b000, 0b01);
  INSN(addiw_c,  0b001, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(int32_t imm) {                                                                   \
    assert_cond(is_imm_in_range(imm, 10, 0));                                                \
    assert_cond((imm & 0b1111) == 0);                                                        \
    assert_cond(imm != 0);                                                                   \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 2, 2, (imm & nth_bit(5)) >> 5);                                  \
    patch_c((address)&insn, 4, 3, (imm & right_n_bits(9)) >> 7);                             \
    patch_c((address)&insn, 5, 5, (imm & nth_bit(6)) >> 6);                                  \
    patch_c((address)&insn, 6, 6, (imm & nth_bit(4)) >> 4);                                  \
    patch_reg_c((address)&insn, 7, sp);                                                      \
    patch_c((address)&insn, 12, 12, (imm & nth_bit(9)) >> 9);                                \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(addi16sp_c, 0b011, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, uint32_t uimm) {                                                    \
    assert_cond(is_unsigned_imm_in_range(uimm, 10, 0));                                      \
    assert_cond((uimm & 0b11) == 0);                                                         \
    assert_cond(uimm != 0);                                                                  \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_compressed_reg_c((address)&insn, 2, Rd);                                           \
    patch_c((address)&insn, 5, 5, (uimm & nth_bit(3)) >> 3);                                 \
    patch_c((address)&insn, 6, 6, (uimm & nth_bit(2)) >> 2);                                 \
    patch_c((address)&insn, 10, 7, (uimm & right_n_bits(10)) >> 6);                          \
    patch_c((address)&insn, 12, 11, (uimm & right_n_bits(6)) >> 4);                          \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(addi4spn_c, 0b000, 0b00);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd_Rs1, uint32_t shamt) {                                               \
    assert_cond(is_unsigned_imm_in_range(shamt, 6, 0));                                      \
    assert_cond(shamt != 0);                                                                 \
    assert_cond(Rd_Rs1 != x0);                                                               \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 6, 2, (shamt & right_n_bits(5)));                                \
    patch_reg_c((address)&insn, 7, Rd_Rs1);                                                  \
    patch_c((address)&insn, 12, 12, (shamt & nth_bit(5)) >> 5);                              \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(slli_c, 0b000, 0b10);

#undef INSN

#define INSN(NAME, funct3, funct2, op)                                                       \
  void NAME(Register Rd_Rs1, uint32_t shamt) {                                               \
    assert_cond(is_unsigned_imm_in_range(shamt, 6, 0));                                      \
    assert_cond(shamt != 0);                                                                 \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 6, 2, (shamt & right_n_bits(5)));                                \
    patch_compressed_reg_c((address)&insn, 7, Rd_Rs1);                                       \
    patch_c((address)&insn, 11, 10, funct2);                                                 \
    patch_c((address)&insn, 12, 12, (shamt & nth_bit(5)) >> 5);                              \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(srli_c, 0b100, 0b00, 0b01);
  INSN(srai_c, 0b100, 0b01, 0b01);

#undef INSN

#define INSN(NAME, funct3, funct2, op)                                                       \
  void NAME(Register Rd_Rs1, int32_t imm) {                                                  \
    assert_cond(is_imm_in_range(imm, 6, 0));                                                 \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 6, 2, (imm & right_n_bits(5)));                                  \
    patch_compressed_reg_c((address)&insn, 7, Rd_Rs1);                                       \
    patch_c((address)&insn, 11, 10, funct2);                                                 \
    patch_c((address)&insn, 12, 12, (imm & nth_bit(5)) >> 5);                                \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(andi_c, 0b100, 0b10, 0b01);

#undef INSN

#define INSN(NAME, funct6, funct2, op)                                                       \
  void NAME(Register Rd_Rs1, Register Rs2) {                                                 \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_compressed_reg_c((address)&insn, 2, Rs2);                                          \
    patch_c((address)&insn, 6, 5, funct2);                                                   \
    patch_compressed_reg_c((address)&insn, 7, Rd_Rs1);                                       \
    patch_c((address)&insn, 15, 10, funct6);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(sub_c,  0b100011, 0b00, 0b01);
  INSN(xor_c,  0b100011, 0b01, 0b01);
  INSN(or_c,   0b100011, 0b10, 0b01);
  INSN(and_c,  0b100011, 0b11, 0b01);
  INSN(subw_c, 0b100111, 0b00, 0b01);
  INSN(addw_c, 0b100111, 0b01, 0b01);

#undef INSN

#define INSN(NAME, funct4, op)                                                               \
  void NAME(Register Rd_Rs1, Register Rs2) {                                                 \
    assert_cond(Rd_Rs1 != x0);                                                               \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_reg_c((address)&insn, 2, Rs2);                                                     \
    patch_reg_c((address)&insn, 7, Rd_Rs1);                                                  \
    patch_c((address)&insn, 15, 12, funct4);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(mv_c,  0b1000, 0b10);
  INSN(add_c, 0b1001, 0b10);

#undef INSN

#define INSN(NAME, funct4, op)                                                               \
  void NAME(Register Rs1) {                                                                  \
    assert_cond(Rs1 != x0);                                                                  \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_reg_c((address)&insn, 2, x0);                                                      \
    patch_reg_c((address)&insn, 7, Rs1);                                                     \
    patch_c((address)&insn, 15, 12, funct4);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(jr_c,   0b1000, 0b10);
  INSN(jalr_c, 0b1001, 0b10);

#undef INSN

  typedef void (Assembler::* j_c_insn)(address dest);
  typedef void (Assembler::* compare_and_branch_c_insn)(Register Rs1, address dest);

  void wrap_label(Label &L, j_c_insn insn);
  void wrap_label(Label &L, Register r, compare_and_branch_c_insn insn);

#define INSN(NAME, funct3, op)                                                               \
  void NAME(int32_t offset) {                                                                \
    assert_cond(is_imm_in_range(offset, 11, 1));                                             \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 2, 2, (offset & nth_bit(5)) >> 5);                               \
    patch_c((address)&insn, 5, 3, (offset & right_n_bits(4)) >> 1);                          \
    patch_c((address)&insn, 6, 6, (offset & nth_bit(7)) >> 7);                               \
    patch_c((address)&insn, 7, 7, (offset & nth_bit(6)) >> 6);                               \
    patch_c((address)&insn, 8, 8, (offset & nth_bit(10)) >> 10);                             \
    patch_c((address)&insn, 10, 9, (offset & right_n_bits(10)) >> 8);                        \
    patch_c((address)&insn, 11, 11, (offset & nth_bit(4)) >> 4);                             \
    patch_c((address)&insn, 12, 12, (offset & nth_bit(11)) >> 11);                           \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }                                                                                          \
  void NAME(address dest) {                                                                  \
    assert_cond(dest != NULL);                                                               \
    int64_t distance = dest - pc();                                                          \
    assert_cond(is_imm_in_range(distance, 11, 1));                                           \
    j_c(distance);                                                                           \
  }                                                                                          \
  void NAME(Label &L) {                                                                      \
    wrap_label(L, &Assembler::NAME);                                                         \
  }

  INSN(j_c, 0b101, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rs1, int32_t imm) {                                                     \
    assert_cond(is_imm_in_range(imm, 8, 1));                                                 \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 2, 2, (imm & nth_bit(5)) >> 5);                                  \
    patch_c((address)&insn, 4, 3, (imm & right_n_bits(3)) >> 1);                             \
    patch_c((address)&insn, 6, 5, (imm & right_n_bits(8)) >> 6);                             \
    patch_compressed_reg_c((address)&insn, 7, Rs1);                                          \
    patch_c((address)&insn, 11, 10, (imm & right_n_bits(5)) >> 3);                           \
    patch_c((address)&insn, 12, 12, (imm & nth_bit(8)) >> 8);                                \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
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

  INSN(beqz_c, 0b110, 0b01);
  INSN(bnez_c, 0b111, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, int32_t imm) {                                                      \
    assert_cond(is_imm_in_range(imm, 18, 0));                                                \
    assert_cond((imm & 0xfff) == 0);                                                         \
    assert_cond(imm != 0);                                                                   \
    assert_cond(Rd != x0 && Rd != x2);                                                       \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 6, 2, (imm & right_n_bits(17)) >> 12);                           \
    patch_reg_c((address)&insn, 7, Rd);                                                      \
    patch_c((address)&insn, 12, 12, (imm & nth_bit(17)) >> 17);                              \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(lui_c, 0b011, 0b01);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, int32_t imm) {                                                      \
    assert_cond(is_imm_in_range(imm, 6, 0));                                                 \
    assert_cond(Rd != x0);                                                                   \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 6, 2, (imm & right_n_bits(5)));                                  \
    patch_reg_c((address)&insn, 7, Rd);                                                      \
    patch_c((address)&insn, 12, 12, (imm & right_n_bits(6)) >> 5);                           \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(li_c, 0b010, 0b01);

#undef INSN

#define INSN(NAME, funct3, op, REGISTER_TYPE, CHECK)                                         \
  void NAME(REGISTER_TYPE Rd, uint32_t uimm) {                                               \
    assert_cond(is_unsigned_imm_in_range(uimm, 9, 0));                                       \
    assert_cond((uimm & 0b111) == 0);                                                        \
    IF(CHECK, assert_cond(Rd != x0);)                                                        \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 4, 2, (uimm & right_n_bits(9)) >> 6);                            \
    patch_c((address)&insn, 6, 5, (uimm & right_n_bits(5)) >> 3);                            \
    patch_reg_c((address)&insn, 7, Rd);                                                      \
    patch_c((address)&insn, 12, 12, (uimm & nth_bit(5)) >> 5);                               \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

#define IF(BOOL, ...)       IF_##BOOL(__VA_ARGS__)
#define IF_true(code)       code
#define IF_false(code)

  INSN(ldsp_c,  0b011, 0b10, Register,      true);
  INSN(fldsp_c, 0b001, 0b10, FloatRegister, false);

#undef IF_false
#undef IF_true
#undef IF
#undef INSN

#define INSN(NAME, funct3, op, REGISTER_TYPE)                                                \
  void NAME(REGISTER_TYPE Rd_Rs2, Register Rs1, uint32_t uimm) {                             \
    assert_cond(is_unsigned_imm_in_range(uimm, 8, 0));                                       \
    assert_cond((uimm & 0b111) == 0);                                                        \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_compressed_reg_c((address)&insn, 2, Rd_Rs2);                                       \
    patch_c((address)&insn, 6, 5, (uimm & right_n_bits(8)) >> 6);                            \
    patch_compressed_reg_c((address)&insn, 7, Rs1);                                          \
    patch_c((address)&insn, 12, 10, (uimm & right_n_bits(6)) >> 3);                          \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(ld_c,  0b011, 0b00, Register);
  INSN(sd_c,  0b111, 0b00, Register);
  INSN(fld_c, 0b001, 0b00, FloatRegister);
  INSN(fsd_c, 0b101, 0b00, FloatRegister);

#undef INSN

#define INSN(NAME, funct3, op, REGISTER_TYPE)                                                \
  void NAME(REGISTER_TYPE Rs2, uint32_t uimm) {                                              \
    assert_cond(is_unsigned_imm_in_range(uimm, 9, 0));                                       \
    assert_cond((uimm & 0b111) == 0);                                                        \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_reg_c((address)&insn, 2, Rs2);                                                     \
    patch_c((address)&insn, 9, 7, (uimm & right_n_bits(9)) >> 6);                            \
    patch_c((address)&insn, 12, 10, (uimm & right_n_bits(6)) >> 3);                          \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(sdsp_c,  0b111, 0b10, Register);
  INSN(fsdsp_c, 0b101, 0b10, FloatRegister);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rs2, uint32_t uimm) {                                                   \
    assert_cond(is_unsigned_imm_in_range(uimm, 8, 0));                                       \
    assert_cond((uimm & 0b11) == 0);                                                         \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_reg_c((address)&insn, 2, Rs2);                                                     \
    patch_c((address)&insn, 8, 7, (uimm & right_n_bits(8)) >> 6);                            \
    patch_c((address)&insn, 12, 9, (uimm & right_n_bits(6)) >> 2);                           \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(swsp_c, 0b110, 0b10);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd, uint32_t uimm) {                                                    \
    assert_cond(is_unsigned_imm_in_range(uimm, 8, 0));                                       \
    assert_cond((uimm & 0b11) == 0);                                                         \
    assert_cond(Rd != x0);                                                                   \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 3, 2, (uimm & right_n_bits(8)) >> 6);                            \
    patch_c((address)&insn, 6, 4, (uimm & right_n_bits(5)) >> 2);                            \
    patch_reg_c((address)&insn, 7, Rd);                                                      \
    patch_c((address)&insn, 12, 12, (uimm & nth_bit(5)) >> 5);                               \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(lwsp_c, 0b010, 0b10);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME(Register Rd_Rs2, Register Rs1, uint32_t uimm) {                                  \
    assert_cond(is_unsigned_imm_in_range(uimm, 7, 0));                                       \
    assert_cond((uimm & 0b11) == 0);                                                         \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_compressed_reg_c((address)&insn, 2, Rd_Rs2);                                       \
    patch_c((address)&insn, 5, 5, (uimm & nth_bit(6)) >> 6);                                 \
    patch_c((address)&insn, 6, 6, (uimm & nth_bit(2)) >> 2);                                 \
    patch_compressed_reg_c((address)&insn, 7, Rs1);                                          \
    patch_c((address)&insn, 12, 10, (uimm & right_n_bits(6)) >> 3);                          \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(lw_c, 0b010, 0b00);
  INSN(sw_c, 0b110, 0b00);

#undef INSN

#define INSN(NAME, funct3, op)                                                               \
  void NAME() {                                                                              \
    uint16_t insn = 0;                                                                       \
    patch_c((address)&insn, 1, 0, op);                                                       \
    patch_c((address)&insn, 11, 2, 0x0);                                                     \
    patch_c((address)&insn, 12, 12, 0b1);                                                    \
    patch_c((address)&insn, 15, 13, funct3);                                                 \
    emit_int16(insn);                                                                        \
  }

  INSN(ebreak_c, 0b100, 0b10);

#undef INSN

// --------------  C-Ext Transformation Macros  --------------

// a pivotal dispatcher for C-Ext
#define EMIT_MAY_COMPRESS(COMPRESSIBLE, NAME, ...)    EMIT_MAY_COMPRESS_##COMPRESSIBLE(NAME, __VA_ARGS__)
#define EMIT_MAY_COMPRESS_true(NAME, ...)             EMIT_MAY_COMPRESS_##NAME(__VA_ARGS__)
#define EMIT_MAY_COMPRESS_false(NAME, ...)

#define IS_COMPRESSIBLE(...)                          if (__VA_ARGS__)
#define CHECK_CEXT_AND_COMPRESSIBLE(...)              IS_COMPRESSIBLE(UseRVC && __VA_ARGS__)
#define CHECK_CEXT()                                  if (UseRVC)

// C-Ext transformation macros
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
    add_c(Rd, src)                                                                     \
  )

// --------------------------
// sub/subw -> c.sub/c.subw
#define EMIT_MAY_COMPRESS_sub_helper(NAME_C, Rd, Rs1, Rs2)                             \
  EMIT_RVC_cond(,                                                                      \
    Rs1 == Rd && Rd->is_compressed_valid() && Rs2->is_compressed_valid(),              \
    NAME_C(Rd, Rs2)                                                                    \
  )

#define EMIT_MAY_COMPRESS_sub(Rd, Rs1, Rs2)                                            \
  EMIT_MAY_COMPRESS_sub_helper(sub_c, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_subw(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_sub_helper(subw_c, Rd, Rs1, Rs2)

// --------------------------
// xor/or/and/addw -> c.xor/c.or/c.and/c.addw
#define EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(NAME_C, Rd, Rs1, Rs2)              \
  EMIT_RVC_cond(                                                                       \
    Register src = noreg;,                                                             \
    Rs1->is_compressed_valid() && Rs2->is_compressed_valid() &&                        \
      ((src = Rs1, Rs2 == Rd) || (src = Rs2, Rs1 == Rd)),                              \
    NAME_C(Rd, src)                                                                    \
  )

#define EMIT_MAY_COMPRESS_xorr(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(xor_c, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_orr(Rd, Rs1, Rs2)                                            \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(or_c, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_andr(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(and_c, Rd, Rs1, Rs2)

#define EMIT_MAY_COMPRESS_addw(Rd, Rs1, Rs2)                                           \
  EMIT_MAY_COMPRESS_xorr_orr_andr_addw_helper(addw_c, Rd, Rs1, Rs2)

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

  FUNC(is_ldsdsp_c,  0b111, 9);
  FUNC(is_lwswsp_c,  0b011, 8);
#undef FUNC

#define FUNC(NAME, funct3, bits)                                                       \
  bool NAME(Register rs1, int32_t imm12) {                                             \
    return rs1 == sp &&                                                                \
      is_unsigned_imm_in_range(imm12, bits, 0) &&                                      \
      (intx(imm12) & funct3) == 0x0;                                                   \
  }                                                                                    \

  FUNC(is_fldsdsp_c, 0b111, 9);
#undef FUNC

#define FUNC(NAME, REG_TYPE, funct3, bits)                                             \
  bool NAME(Register rs1, REG_TYPE rd_rs2, int32_t imm12) {                            \
    return rs1->is_compressed_valid() &&                                               \
      rd_rs2->is_compressed_valid() &&                                                 \
      is_unsigned_imm_in_range(imm12, bits, 0) &&                                      \
      (intx(imm12) & funct3) == 0x0;                                                   \
  }                                                                                    \

  FUNC(is_ldsd_c,  Register,      0b111, 8);
  FUNC(is_lwsw_c,  Register,      0b011, 7);
  FUNC(is_fldsd_c, FloatRegister, 0b111, 8);
#undef FUNC

public:
// --------------------------
// ld -> c.ldsp/c.ld
#define EMIT_MAY_COMPRESS_ld(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_ldsdsp_c(Rs, Rd, offset, true),                                                \
     ldsp_c(Rd, offset),                                                               \
     is_ldsd_c(Rs, Rd, offset),                                                        \
     ld_c(Rd, Rs, offset)                                                              \
  )

// --------------------------
// sd -> c.sdsp/c.sd
#define EMIT_MAY_COMPRESS_sd(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_ldsdsp_c(Rs, Rd, offset, false),                                               \
     sdsp_c(Rd, offset),                                                               \
     is_ldsd_c(Rs, Rd, offset),                                                        \
     sd_c(Rd, Rs, offset)                                                              \
  )

// --------------------------
// lw -> c.lwsp/c.lw
#define EMIT_MAY_COMPRESS_lw(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_lwswsp_c(Rs, Rd, offset, true),                                                \
     lwsp_c(Rd, offset),                                                               \
     is_lwsw_c(Rs, Rd, offset),                                                        \
     lw_c(Rd, Rs, offset)                                                              \
  )

// --------------------------
// sw -> c.swsp/c.sw
#define EMIT_MAY_COMPRESS_sw(Rd, Rs, offset)                                           \
  EMIT_RVC_cond2(,                                                                     \
     is_lwswsp_c(Rs, Rd, offset, false),                                               \
     swsp_c(Rd, offset),                                                               \
     is_lwsw_c(Rs, Rd, offset),                                                        \
     sw_c(Rd, Rs, offset)                                                              \
  )

// --------------------------
// fld -> c.fldsp/c.fld
#define EMIT_MAY_COMPRESS_fld(Rd, Rs, offset)                                          \
  EMIT_RVC_cond2(,                                                                     \
     is_fldsdsp_c(Rs, offset),                                                         \
     fldsp_c(Rd, offset),                                                              \
     is_fldsd_c(Rs, Rd, offset),                                                       \
     fld_c(Rd, Rs, offset)                                                             \
  )

// --------------------------
// fsd -> c.fsdsp/c.fsd
#define EMIT_MAY_COMPRESS_fsd(Rd, Rs, offset)                                          \
  EMIT_RVC_cond2(,                                                                     \
     is_fldsdsp_c(Rs, offset),                                                         \
     fsdsp_c(Rd, offset),                                                              \
     is_fldsd_c(Rs, Rd, offset),                                                       \
     fsd_c(Rd, Rs, offset)                                                             \
  )

// --------------------------
// Conditional branch instructions
// --------------------------
// beq/bne -> c.beqz/c.bnez

// TODO: Removing the below 'offset != 0' check needs us to fix lots of '__ beqz() / __ benz()'
//   to '__ beqz_nc() / __ bnez_nc()' everywhere.
#define EMIT_MAY_COMPRESS_beqz_bnez_helper(NAME_C, Rs1, Rs2, offset)                   \
  EMIT_RVC_cond(,                                                                      \
    offset != 0 && Rs2 == x0 && Rs1->is_compressed_valid() &&                          \
      is_imm_in_range(offset, 8, 1),                                                   \
    NAME_C(Rs1, offset)                                                                \
  )

#define EMIT_MAY_COMPRESS_beq(Rs1, Rs2, offset)                                        \
  EMIT_MAY_COMPRESS_beqz_bnez_helper(beqz_c, Rs1, Rs2, offset)

#define EMIT_MAY_COMPRESS_bne(Rs1, Rs2, offset)                                        \
  EMIT_MAY_COMPRESS_beqz_bnez_helper(bnez_c, Rs1, Rs2, offset)

// --------------------------
// Unconditional branch instructions
// --------------------------
// jalr/jal -> c.jr/c.jalr/c.j

#define EMIT_MAY_COMPRESS_jalr(Rd, Rs, offset)                                         \
  EMIT_RVC_cond2(,                                                                     \
    offset == 0 && Rd == x1 && Rs != x0,                                               \
    jalr_c(Rs),                                                                        \
    offset == 0 && Rd == x0 && Rs != x0,                                               \
    jr_c(Rs)                                                                           \
  )

// TODO: Removing the 'offset != 0' check needs us to fix lots of '__ j()'
//   to '__ j_nc()' manually everywhere.
#define EMIT_MAY_COMPRESS_jal(Rd, offset)                                              \
  EMIT_RVC_cond(,                                                                      \
    offset != 0 && Rd == x0 && is_imm_in_range(offset, 11, 1),                         \
    j_c(offset)                                                                        \
  )

// --------------------------
// Upper Immediate Instruction
// --------------------------
// lui -> c.lui
#define EMIT_MAY_COMPRESS_lui(Rd, imm)                                                 \
  EMIT_RVC_cond(,                                                                      \
    Rd != x0 && Rd != x2 && imm != 0 && is_imm_in_range(imm, 18, 0),                   \
    lui_c(Rd, imm)                                                                     \
  )

// --------------------------
// Miscellaneous Instructions
// --------------------------
// ebreak -> c.ebreak
#define EMIT_MAY_COMPRESS_ebreak()                                                     \
  EMIT_RVC_cond(,                                                                      \
    true,                                                                              \
    ebreak_c()                                                                         \
  )

// --------------------------
// Immediate Instructions
// --------------------------
// addi -> c.addi16sp/c.addi4spn/c.mv/c.addi/. An addi instruction able to transform to c.nop will be ignored.
#define EMIT_MAY_COMPRESS_addi(Rd, Rs1, imm)                                                          \
  EMIT_RVC_cond4(,                                                                                    \
    Rs1 == sp && Rd == Rs1 && imm != 0 && (imm & 0b1111) == 0x0 && is_imm_in_range(imm, 10, 0),       \
    addi16sp_c(imm),                                                                                  \
    Rs1 == sp && Rd->is_compressed_valid() && imm != 0 && (imm & 0b11) == 0x0 && is_unsigned_imm_in_range(imm, 10, 0),  \
    addi4spn_c(Rd, imm),                                                                              \
    Rd == Rs1 && is_imm_in_range(imm, 6, 0),                                                          \
    if (imm != 0) { addi_c(Rd, imm); },                                                               \
    imm == 0 && Rd != x0 && Rs1 != x0,                                                                \
    mv_c(Rd, Rs1)                                                                                     \
  )

// --------------------------
// addiw -> c.addiw
#define EMIT_MAY_COMPRESS_addiw(Rd, Rs1, imm)                                          \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd != x0 && is_imm_in_range(imm, 6, 0),                               \
    addiw_c(Rd, imm)                                                                   \
  )

// --------------------------
// and_imm12 -> c.andi
#define EMIT_MAY_COMPRESS_and_imm12(Rd, Rs1, imm)                                      \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd->is_compressed_valid() && is_imm_in_range(imm, 6, 0),              \
    andi_c(Rd, imm)                                                                    \
  )

// --------------------------
// Shift Immediate Instructions
// --------------------------
// slli -> c.slli
#define EMIT_MAY_COMPRESS_slli(Rd, Rs1, shamt)                                         \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd != x0 && shamt != 0,                                               \
    slli_c(Rd, shamt)                                                                  \
  )

// --------------------------
// srai/srli -> c.srai/c.srli
#define EMIT_MAY_COMPRESS_srai_srli_helper(NAME_C, Rd, Rs1, shamt)                     \
  EMIT_RVC_cond(,                                                                      \
    Rd == Rs1 && Rd->is_compressed_valid() && shamt != 0,                              \
    NAME_C(Rd, shamt)                                                                  \
  )

#define EMIT_MAY_COMPRESS_srai(Rd, Rs1, shamt)                                         \
  EMIT_MAY_COMPRESS_srai_srli_helper(srai_c, Rd, Rs1, shamt)

#define EMIT_MAY_COMPRESS_srli(Rd, Rs1, shamt)                                         \
  EMIT_MAY_COMPRESS_srai_srli_helper(srli_c, Rd, Rs1, shamt)

// --------------------------

// a compile time dispatcher
#define EMIT_MAY_COMPRESS_NAME_true(NAME, ARGS)            NAME ARGS
#define EMIT_MAY_COMPRESS_NAME_false(NAME, ARGS)           NAME##_nc ARGS
#define EMIT_MAY_COMPRESS_NAME(COMPRESSIBLE, NAME, ARGS)   EMIT_MAY_COMPRESS_NAME_##COMPRESSIBLE(NAME, ARGS)

// a runtime dispatcher (if clause is needed)
#define EMIT_MAY_COMPRESS_INST(COMPRESSIBLE, NAME, ARGS) \
  if (COMPRESSIBLE) {                                    \
    EMIT_MAY_COMPRESS_NAME_true(NAME, ARGS);             \
  } else {                                               \
    EMIT_MAY_COMPRESS_NAME_false(NAME, ARGS);            \
  }

#endif // CPU_RISCV_ASSEMBLER_RISCV_CEXT_HPP