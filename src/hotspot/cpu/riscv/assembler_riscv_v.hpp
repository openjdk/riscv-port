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

#ifndef CPU_RISCV_ASSEMBLER_RISCV_V_HPP
#define CPU_RISCV_ASSEMBLER_RISCV_V_HPP

enum SEW {
  e8,
  e16,
  e32,
  e64,
  RESERVED,
};

enum LMUL {
  mf8 = 0b101,
  mf4 = 0b110,
  mf2 = 0b111,
  m1  = 0b000,
  m2  = 0b001,
  m4  = 0b010,
  m8  = 0b011,
};

enum VMA {
  mu, // undisturbed
  ma, // agnostic
};

enum VTA {
  tu, // undisturbed
  ta, // agnostic
};

static Assembler::SEW elembytes_to_sew(int ebytes) {
  assert(ebytes > 0 && ebytes <= 8, "unsupported element size");
  return (Assembler::SEW) exact_log2(ebytes);
}

static Assembler::SEW elemtype_to_sew(BasicType etype) {
  return Assembler::elembytes_to_sew(type2aelembytes(etype));
}

#define patch_vtype(hsb, lsb, vlmul, vsew, vta, vma, vill)   \
    if (vill == 1) {                                         \
      guarantee((vlmul | vsew | vta | vma == 0),             \
                "the other bits in vtype shall be zero");    \
    }                                                        \
    patch((address)&insn, lsb + 2, lsb, vlmul);              \
    patch((address)&insn, lsb + 5, lsb + 3, vsew);           \
    patch((address)&insn, lsb + 6, vta);                     \
    patch((address)&insn, lsb + 7, vma);                     \
    patch((address)&insn, hsb - 1, lsb + 8, 0);              \
    patch((address)&insn, hsb, vill)

#define INSN(NAME, op, funct3)                                            \
  void NAME(Register Rd, Register Rs1, SEW sew, LMUL lmul = m1,           \
            VMA vma = mu, VTA vta = tu, bool vill = false) {              \
    unsigned insn = 0;                                                    \
    patch((address)&insn, 6, 0, op);                                      \
    patch((address)&insn, 14, 12, funct3);                                \
    patch_vtype(30, 20, lmul, sew, vta, vma, vill);                       \
    patch((address)&insn, 31, 0);                                         \
    patch_reg((address)&insn, 7, Rd);                                     \
    patch_reg((address)&insn, 15, Rs1);                                   \
    emit(insn);                                                           \
  }

  INSN(vsetvli, 0b1010111, 0b111);

#undef INSN

#define INSN(NAME, op, funct3)                                            \
  void NAME(Register Rd, uint32_t imm, SEW sew, LMUL lmul = m1,           \
            VMA vma = mu, VTA vta = tu, bool vill = false) {              \
    unsigned insn = 0;                                                    \
    guarantee(is_unsigned_imm_in_range(imm, 5, 0), "imm is invalid");     \
    patch((address)&insn, 6, 0, op);                                      \
    patch((address)&insn, 14, 12, funct3);                                \
    patch((address)&insn, 19, 15, imm);                                   \
    patch_vtype(29, 20, lmul, sew, vta, vma, vill);                       \
    patch((address)&insn, 31, 30, 0b11);                                  \
    patch_reg((address)&insn, 7, Rd);                                     \
    emit(insn);                                                           \
  }

  INSN(vsetivli, 0b1010111, 0b111);

#undef INSN

#undef patch_vtype

#define INSN(NAME, op, funct3, funct7)                          \
  void NAME(Register Rd, Register Rs1, Register Rs2) {          \
    unsigned insn = 0;                                          \
    patch((address)&insn, 6,  0, op);                           \
    patch((address)&insn, 14, 12, funct3);                      \
    patch((address)&insn, 31, 25, funct7);                      \
    patch_reg((address)&insn, 7, Rd);                           \
    patch_reg((address)&insn, 15, Rs1);                         \
    patch_reg((address)&insn, 20, Rs2);                         \
    emit(insn);                                                 \
  }

  // Vector Configuration Instruction
  INSN(vsetvl, 0b1010111, 0b111, 0b1000000);

#undef INSN

enum VectorMask {
  v0_t = 0b0,
  unmasked = 0b1
};

#define patch_VArith(op, Reg, funct3, Reg_or_Imm5, Vs2, vm, funct6)            \
    unsigned insn = 0;                                                         \
    patch((address)&insn, 6, 0, op);                                           \
    patch((address)&insn, 14, 12, funct3);                                     \
    patch((address)&insn, 19, 15, Reg_or_Imm5);                                \
    patch((address)&insn, 25, vm);                                             \
    patch((address)&insn, 31, 26, funct6);                                     \
    patch_reg((address)&insn, 7, Reg);                                         \
    patch_reg((address)&insn, 20, Vs2);                                        \
    emit(insn)

// r2_vm
#define INSN(NAME, op, funct3, Vs1, funct6)                                    \
  void NAME(Register Rd, VectorRegister Vs2, VectorMask vm = unmasked) {       \
    patch_VArith(op, Rd, funct3, Vs1, Vs2, vm, funct6);                        \
  }

  // Vector Mask
  INSN(vpopc_m,  0b1010111, 0b010, 0b10000, 0b010000);
  INSN(vfirst_m, 0b1010111, 0b010, 0b10001, 0b010000);
#undef INSN

#define INSN(NAME, op, funct3, Vs1, funct6)                                    \
  void NAME(VectorRegister Vd, VectorRegister Vs2, VectorMask vm = unmasked) { \
    patch_VArith(op, Vd, funct3, Vs1, Vs2, vm, funct6);                        \
  }

  // Vector Integer Extension
  INSN(vzext_vf2, 0b1010111, 0b010, 0b00110, 0b010010);
  INSN(vzext_vf4, 0b1010111, 0b010, 0b00100, 0b010010);
  INSN(vzext_vf8, 0b1010111, 0b010, 0b00010, 0b010010);
  INSN(vsext_vf2, 0b1010111, 0b010, 0b00111, 0b010010);
  INSN(vsext_vf4, 0b1010111, 0b010, 0b00101, 0b010010);
  INSN(vsext_vf8, 0b1010111, 0b010, 0b00011, 0b010010);

  // Vector Mask
  INSN(vmsbf_m,   0b1010111, 0b010, 0b00001, 0b010100);
  INSN(vmsif_m,   0b1010111, 0b010, 0b00011, 0b010100);
  INSN(vmsof_m,   0b1010111, 0b010, 0b00010, 0b010100);
  INSN(viota_m,   0b1010111, 0b010, 0b10000, 0b010100);

  // Vector Single-Width Floating-Point/Integer Type-Convert Instructions
  INSN(vfcvt_xu_f_v, 0b1010111, 0b001, 0b00000, 0b010010);
  INSN(vfcvt_x_f_v,  0b1010111, 0b001, 0b00001, 0b010010);
  INSN(vfcvt_f_xu_v, 0b1010111, 0b001, 0b00010, 0b010010);
  INSN(vfcvt_f_x_v,  0b1010111, 0b001, 0b00011, 0b010010);
  INSN(vfcvt_rtz_xu_f_v, 0b1010111, 0b001, 0b00110, 0b010010);
  INSN(vfcvt_rtz_x_f_v,  0b1010111, 0b001, 0b00111, 0b010010);

  // Vector Floating-Point Instruction
  INSN(vfsqrt_v,  0b1010111, 0b001, 0b00000, 0b010011);
  INSN(vfclass_v, 0b1010111, 0b001, 0b10000, 0b010011);

#undef INSN

// r2rd
#define INSN(NAME, op, funct3, simm5, vm, funct6)         \
  void NAME(VectorRegister Vd, VectorRegister Vs2) {      \
    patch_VArith(op, Vd, funct3, simm5, Vs2, vm, funct6); \
  }

  // Vector Whole Vector Register Move
  INSN(vmv1r_v, 0b1010111, 0b011, 0b00000, 0b1, 0b100111);
  INSN(vmv2r_v, 0b1010111, 0b011, 0b00001, 0b1, 0b100111);
  INSN(vmv4r_v, 0b1010111, 0b011, 0b00011, 0b1, 0b100111);
  INSN(vmv8r_v, 0b1010111, 0b011, 0b00111, 0b1, 0b100111);

#undef INSN

#define INSN(NAME, op, funct3, Vs1, vm, funct6)           \
  void NAME(FloatRegister Rd, VectorRegister Vs2) {       \
    patch_VArith(op, Rd, funct3, Vs1, Vs2, vm, funct6);   \
  }

  // Vector Floating-Point Move Instruction
  INSN(vfmv_f_s, 0b1010111, 0b001, 0b00000, 0b1, 0b010000);

#undef INSN

#define INSN(NAME, op, funct3, Vs1, vm, funct6)          \
  void NAME(Register Rd, VectorRegister Vs2) {           \
    patch_VArith(op, Rd, funct3, Vs1, Vs2, vm, funct6);  \
  }

  // Vector Integer Scalar Move Instructions
  INSN(vmv_x_s, 0b1010111, 0b010, 0b00000, 0b1, 0b010000);

#undef INSN

// r_vm
#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, VectorRegister Vs2, uint32_t imm, VectorMask vm = unmasked) {       \
    guarantee(is_unsigned_imm_in_range(imm, 5, 0), "imm is invalid");                              \
    patch_VArith(op, Vd, funct3, (uint32_t)(imm & 0x1f), Vs2, vm, funct6);                         \
  }

  // Vector Single-Width Bit Shift Instructions
  INSN(vsra_vi,    0b1010111, 0b011, 0b101001);
  INSN(vsrl_vi,    0b1010111, 0b011, 0b101000);
  INSN(vsll_vi,    0b1010111, 0b011, 0b100101);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, VectorRegister Vs1, VectorRegister Vs2, VectorMask vm = unmasked) { \
    patch_VArith(op, Vd, funct3, Vs1->encoding_nocheck(), Vs2, vm, funct6);                        \
  }

  // Vector Single-Width Floating-Point Fused Multiply-Add Instructions
  INSN(vfnmsub_vv, 0b1010111, 0b001, 0b101011);
  INSN(vfmsub_vv,  0b1010111, 0b001, 0b101010);
  INSN(vfnmadd_vv, 0b1010111, 0b001, 0b101001);
  INSN(vfmadd_vv,  0b1010111, 0b001, 0b101000);
  INSN(vfnmsac_vv, 0b1010111, 0b001, 0b101111);
  INSN(vfmsac_vv,  0b1010111, 0b001, 0b101110);
  INSN(vfmacc_vv,  0b1010111, 0b001, 0b101100);
  INSN(vfnmacc_vv, 0b1010111, 0b001, 0b101101);

  // Vector Single-Width Integer Multiply-Add Instructions
  INSN(vnmsub_vv, 0b1010111, 0b010, 0b101011);
  INSN(vmadd_vv,  0b1010111, 0b010, 0b101001);
  INSN(vnmsac_vv, 0b1010111, 0b010, 0b101111);
  INSN(vmacc_vv,  0b1010111, 0b010, 0b101101);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, Register Rs1, VectorRegister Vs2, VectorMask vm = unmasked) {       \
    patch_VArith(op, Vd, funct3, Rs1->encoding_nocheck(), Vs2, vm, funct6);                        \
  }

  // Vector Single-Width Integer Multiply-Add Instructions
  INSN(vnmsub_vx, 0b1010111, 0b110, 0b101011);
  INSN(vmadd_vx,  0b1010111, 0b110, 0b101001);
  INSN(vnmsac_vx, 0b1010111, 0b110, 0b101111);
  INSN(vmacc_vx,  0b1010111, 0b110, 0b101101);

  INSN(vrsub_vx,  0b1010111, 0b100, 0b000011);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, FloatRegister Rs1, VectorRegister Vs2, VectorMask vm = unmasked) {  \
    patch_VArith(op, Vd, funct3, Rs1->encoding_nocheck(), Vs2, vm, funct6);                        \
  }

  // Vector Single-Width Floating-Point Fused Multiply-Add Instructions
  INSN(vfnmsub_vf, 0b1010111, 0b101, 0b101011);
  INSN(vfmsub_vf,  0b1010111, 0b101, 0b101010);
  INSN(vfnmadd_vf, 0b1010111, 0b101, 0b101001);
  INSN(vfmadd_vf,  0b1010111, 0b101, 0b101000);
  INSN(vfnmsac_vf, 0b1010111, 0b101, 0b101111);
  INSN(vfmsac_vf,  0b1010111, 0b101, 0b101110);
  INSN(vfmacc_vf,  0b1010111, 0b101, 0b101100);
  INSN(vfnmacc_vf, 0b1010111, 0b101, 0b101101);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, VectorRegister Vs2, VectorRegister Vs1, VectorMask vm = unmasked) { \
    patch_VArith(op, Vd, funct3, Vs1->encoding_nocheck(), Vs2, vm, funct6);                        \
  }

  // Vector Single-Width Floating-Point Reduction Instructions
  INSN(vfredsum_vs,   0b1010111, 0b001, 0b000001);
  INSN(vfredosum_vs,  0b1010111, 0b001, 0b000011);
  INSN(vfredmin_vs,   0b1010111, 0b001, 0b000101);
  INSN(vfredmax_vs,   0b1010111, 0b001, 0b000111);

  // Vector Single-Width Integer Reduction Instructions
  INSN(vredsum_vs,    0b1010111, 0b010, 0b000000);
  INSN(vredand_vs,    0b1010111, 0b010, 0b000001);
  INSN(vredor_vs,     0b1010111, 0b010, 0b000010);
  INSN(vredxor_vs,    0b1010111, 0b010, 0b000011);
  INSN(vredminu_vs,   0b1010111, 0b010, 0b000100);
  INSN(vredmin_vs,    0b1010111, 0b010, 0b000101);
  INSN(vredmaxu_vs,   0b1010111, 0b010, 0b000110);
  INSN(vredmax_vs,    0b1010111, 0b010, 0b000111);

  // Vector Floating-Point Compare Instructions
  INSN(vmfle_vv, 0b1010111, 0b001, 0b011001);
  INSN(vmflt_vv, 0b1010111, 0b001, 0b011011);
  INSN(vmfne_vv, 0b1010111, 0b001, 0b011100);
  INSN(vmfeq_vv, 0b1010111, 0b001, 0b011000);

  // Vector Floating-Point Sign-Injection Instructions
  INSN(vfsgnjx_vv, 0b1010111, 0b001, 0b001010);
  INSN(vfsgnjn_vv, 0b1010111, 0b001, 0b001001);
  INSN(vfsgnj_vv,  0b1010111, 0b001, 0b001000);

  // Vector Floating-Point MIN/MAX Instructions
  INSN(vfmax_vv,   0b1010111, 0b001, 0b000110);
  INSN(vfmin_vv,   0b1010111, 0b001, 0b000100);

  // Vector Single-Width Floating-Point Multiply/Divide Instructions
  INSN(vfdiv_vv,   0b1010111, 0b001, 0b100000);
  INSN(vfmul_vv,   0b1010111, 0b001, 0b100100);

  // Vector Single-Width Floating-Point Add/Subtract Instructions
  INSN(vfsub_vv, 0b1010111, 0b001, 0b000010);
  INSN(vfadd_vv, 0b1010111, 0b001, 0b000000);

  // Vector Single-Width Fractional Multiply with Rounding and Saturation
  INSN(vsmul_vv, 0b1010111, 0b000, 0b100111);

  // Vector Integer Divide Instructions
  INSN(vrem_vv,  0b1010111, 0b010, 0b100011);
  INSN(vremu_vv, 0b1010111, 0b010, 0b100010);
  INSN(vdiv_vv,  0b1010111, 0b010, 0b100001);
  INSN(vdivu_vv, 0b1010111, 0b010, 0b100000);

  // Vector Single-Width Integer Multiply Instructions
  INSN(vmulhsu_vv, 0b1010111, 0b010, 0b100110);
  INSN(vmulhu_vv,  0b1010111, 0b010, 0b100100);
  INSN(vmulh_vv,   0b1010111, 0b010, 0b100111);
  INSN(vmul_vv,    0b1010111, 0b010, 0b100101);

  // Vector Integer Min/Max Instructions
  INSN(vmax_vv,  0b1010111, 0b000, 0b000111);
  INSN(vmaxu_vv, 0b1010111, 0b000, 0b000110);
  INSN(vmin_vv,  0b1010111, 0b000, 0b000101);
  INSN(vminu_vv, 0b1010111, 0b000, 0b000100);

  // Vector Integer Comparison Instructions
  INSN(vmsle_vv,  0b1010111, 0b000, 0b011101);
  INSN(vmsleu_vv, 0b1010111, 0b000, 0b011100);
  INSN(vmslt_vv,  0b1010111, 0b000, 0b011011);
  INSN(vmsltu_vv, 0b1010111, 0b000, 0b011010);
  INSN(vmsne_vv,  0b1010111, 0b000, 0b011001);
  INSN(vmseq_vv,  0b1010111, 0b000, 0b011000);

  // Vector Single-Width Bit Shift Instructions
  INSN(vsra_vv, 0b1010111, 0b000, 0b101001);
  INSN(vsrl_vv, 0b1010111, 0b000, 0b101000);
  INSN(vsll_vv, 0b1010111, 0b000, 0b100101);

  // Vector Bitwise Logical Instructions
  INSN(vxor_vv, 0b1010111, 0b000, 0b001011);
  INSN(vor_vv,  0b1010111, 0b000, 0b001010);
  INSN(vand_vv, 0b1010111, 0b000, 0b001001);

  // Vector Single-Width Integer Add and Subtract
  INSN(vsub_vv, 0b1010111, 0b000, 0b000010);
  INSN(vadd_vv, 0b1010111, 0b000, 0b000000);

#undef INSN


#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, VectorRegister Vs2, Register Rs1, VectorMask vm = unmasked) {       \
    patch_VArith(op, Vd, funct3, Rs1->encoding_nocheck(), Vs2, vm, funct6);                        \
  }

  // Vector Integer Divide Instructions
  INSN(vrem_vx,  0b1010111, 0b110, 0b100011);
  INSN(vremu_vx, 0b1010111, 0b110, 0b100010);
  INSN(vdiv_vx,  0b1010111, 0b110, 0b100001);
  INSN(vdivu_vx, 0b1010111, 0b110, 0b100000);

  // Vector Single-Width Integer Multiply Instructions
  INSN(vmulhsu_vx, 0b1010111, 0b110, 0b100110);
  INSN(vmulhu_vx,  0b1010111, 0b110, 0b100100);
  INSN(vmulh_vx,   0b1010111, 0b110, 0b100111);
  INSN(vmul_vx,    0b1010111, 0b110, 0b100101);

  // Vector Integer Min/Max Instructions
  INSN(vmax_vx,  0b1010111, 0b100, 0b000111);
  INSN(vmaxu_vx, 0b1010111, 0b100, 0b000110);
  INSN(vmin_vx,  0b1010111, 0b100, 0b000101);
  INSN(vminu_vx, 0b1010111, 0b100, 0b000100);

  // Vector Integer Comparison Instructions
  INSN(vmsgt_vx,  0b1010111, 0b100, 0b011111);
  INSN(vmsgtu_vx, 0b1010111, 0b100, 0b011110);
  INSN(vmsle_vx,  0b1010111, 0b100, 0b011101);
  INSN(vmsleu_vx, 0b1010111, 0b100, 0b011100);
  INSN(vmslt_vx,  0b1010111, 0b100, 0b011011);
  INSN(vmsltu_vx, 0b1010111, 0b100, 0b011010);
  INSN(vmsne_vx,  0b1010111, 0b100, 0b011001);
  INSN(vmseq_vx,  0b1010111, 0b100, 0b011000);

  // Vector Narrowing Integer Right Shift Instructions
  INSN(vnsra_wx, 0b1010111, 0b100, 0b101101);
  INSN(vnsrl_wx, 0b1010111, 0b100, 0b101100);

  // Vector Single-Width Bit Shift Instructions
  INSN(vsra_vx, 0b1010111, 0b100, 0b101001);
  INSN(vsrl_vx, 0b1010111, 0b100, 0b101000);
  INSN(vsll_vx, 0b1010111, 0b100, 0b100101);

  // Vector Bitwise Logical Instructions
  INSN(vxor_vx, 0b1010111, 0b100, 0b001011);
  INSN(vor_vx,  0b1010111, 0b100, 0b001010);
  INSN(vand_vx, 0b1010111, 0b100, 0b001001);

  // Vector Single-Width Integer Add and Subtract
  INSN(vsub_vx, 0b1010111, 0b100, 0b000010);
  INSN(vadd_vx, 0b1010111, 0b100, 0b000000);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, VectorRegister Vs2, FloatRegister Rs1, VectorMask vm = unmasked) {  \
    patch_VArith(op, Vd, funct3, Rs1->encoding_nocheck(), Vs2, vm, funct6);                        \
  }

  // Vector Floating-Point Compare Instructions
  INSN(vmfge_vf, 0b1010111, 0b101, 0b011111);
  INSN(vmfgt_vf, 0b1010111, 0b101, 0b011101);
  INSN(vmfle_vf, 0b1010111, 0b101, 0b011001);
  INSN(vmflt_vf, 0b1010111, 0b101, 0b011011);
  INSN(vmfne_vf, 0b1010111, 0b101, 0b011100);
  INSN(vmfeq_vf, 0b1010111, 0b101, 0b011000);

  // Vector Floating-Point Sign-Injection Instructions
  INSN(vfsgnjx_vf, 0b1010111, 0b101, 0b001010);
  INSN(vfsgnjn_vf, 0b1010111, 0b101, 0b001001);
  INSN(vfsgnj_vf,  0b1010111, 0b101, 0b001000);

  // Vector Floating-Point MIN/MAX Instructions
  INSN(vfmax_vf, 0b1010111, 0b101, 0b000110);
  INSN(vfmin_vf, 0b1010111, 0b101, 0b000100);

  // Vector Single-Width Floating-Point Multiply/Divide Instructions
  INSN(vfdiv_vf,  0b1010111, 0b101, 0b100000);
  INSN(vfmul_vf,  0b1010111, 0b101, 0b100100);
  INSN(vfrdiv_vf, 0b1010111, 0b101, 0b100001);

  // Vector Single-Width Floating-Point Add/Subtract Instructions
  INSN(vfsub_vf,  0b1010111, 0b101, 0b000010);
  INSN(vfadd_vf,  0b1010111, 0b101, 0b000000);
  INSN(vfrsub_vf, 0b1010111, 0b101, 0b100111);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, VectorRegister Vs2, int32_t imm, VectorMask vm = unmasked) {        \
    guarantee(is_imm_in_range(imm, 5, 0), "imm is invalid");                                       \
    patch_VArith(op, Vd, funct3, (uint32_t)imm & 0x1f, Vs2, vm, funct6);                           \
  }

  INSN(vmsgt_vi,  0b1010111, 0b011, 0b011111);
  INSN(vmsgtu_vi, 0b1010111, 0b011, 0b011110);
  INSN(vmsle_vi,  0b1010111, 0b011, 0b011101);
  INSN(vmsleu_vi, 0b1010111, 0b011, 0b011100);
  INSN(vmsne_vi,  0b1010111, 0b011, 0b011001);
  INSN(vmseq_vi,  0b1010111, 0b011, 0b011000);
  INSN(vxor_vi,   0b1010111, 0b011, 0b001011);
  INSN(vor_vi,    0b1010111, 0b011, 0b001010);
  INSN(vand_vi,   0b1010111, 0b011, 0b001001);
  INSN(vadd_vi,   0b1010111, 0b011, 0b000000);

#undef INSN

#define INSN(NAME, op, funct3, funct6)                                                             \
  void NAME(VectorRegister Vd, int32_t imm, VectorRegister Vs2, VectorMask vm = unmasked) {        \
    guarantee(is_imm_in_range(imm, 5, 0), "imm is invalid");                                       \
    patch_VArith(op, Vd, funct3, (uint32_t)(imm & 0x1f), Vs2, vm, funct6);                         \
  }

  INSN(vrsub_vi, 0b1010111, 0b011, 0b000011);

#undef INSN

#define INSN(NAME, op, funct3, vm, funct6)                                   \
  void NAME(VectorRegister Vd, VectorRegister Vs2, VectorRegister Vs1) {     \
    patch_VArith(op, Vd, funct3, Vs1->encoding_nocheck(), Vs2, vm, funct6);  \
  }

  // Vector Compress Instruction
  INSN(vcompress_vm, 0b1010111, 0b010, 0b1, 0b010111);

  // Vector Mask-Register Logical Instructions
  INSN(vmxnor_mm,   0b1010111, 0b010, 0b1, 0b011111);
  INSN(vmornot_mm,  0b1010111, 0b010, 0b1, 0b011100);
  INSN(vmnor_mm,    0b1010111, 0b010, 0b1, 0b011110);
  INSN(vmor_mm,     0b1010111, 0b010, 0b1, 0b011010);
  INSN(vmxor_mm,    0b1010111, 0b010, 0b1, 0b011011);
  INSN(vmandnot_mm, 0b1010111, 0b010, 0b1, 0b011000);
  INSN(vmnand_mm,   0b1010111, 0b010, 0b1, 0b011101);
  INSN(vmand_mm,    0b1010111, 0b010, 0b1, 0b011001);

#undef INSN

#define INSN(NAME, op, funct3, Vs2, vm, funct6)                            \
  void NAME(VectorRegister Vd, int32_t imm) {                              \
    guarantee(is_imm_in_range(imm, 5, 0), "imm is invalid");               \
    patch_VArith(op, Vd, funct3, (uint32_t)(imm & 0x1f), Vs2, vm, funct6); \
  }

  // Vector Integer Move Instructions
  INSN(vmv_v_i, 0b1010111, 0b011, v0, 0b1, 0b010111);

#undef INSN

#define INSN(NAME, op, funct3, Vs2, vm, funct6)                             \
  void NAME(VectorRegister Vd, FloatRegister Rs1) {                         \
    patch_VArith(op, Vd, funct3, Rs1->encoding_nocheck(), Vs2, vm, funct6); \
  }

  // Floating-Point Scalar Move Instructions
  INSN(vfmv_s_f, 0b1010111, 0b101, v0, 0b1, 0b010000);
  // Vector Floating-Point Move Instruction
  INSN(vfmv_v_f, 0b1010111, 0b101, v0, 0b1, 0b010111);

#undef INSN

#define INSN(NAME, op, funct3, Vs2, vm, funct6)                             \
  void NAME(VectorRegister Vd, VectorRegister Vs1) {                        \
    patch_VArith(op, Vd, funct3, Vs1->encoding_nocheck(), Vs2, vm, funct6); \
  }

  // Vector Integer Move Instructions
  INSN(vmv_v_v, 0b1010111, 0b000, v0, 0b1, 0b010111);

#undef INSN

#define INSN(NAME, op, funct3, Vs2, vm, funct6)                             \
   void NAME(VectorRegister Vd, Register Rs1) {                             \
    patch_VArith(op, Vd, funct3, Rs1->encoding_nocheck(), Vs2, vm, funct6); \
   }

  // Integer Scalar Move Instructions
  INSN(vmv_s_x, 0b1010111, 0b110, v0, 0b1, 0b010000);

  // Vector Integer Move Instructions
  INSN(vmv_v_x, 0b1010111, 0b100, v0, 0b1, 0b010111);

#undef INSN
#undef patch_VArith

#define INSN(NAME, op, funct13, funct6)                    \
  void NAME(VectorRegister Vd, VectorMask vm = unmasked) { \
    unsigned insn = 0;                                     \
    patch((address)&insn, 6, 0, op);                       \
    patch((address)&insn, 24, 12, funct13);                \
    patch((address)&insn, 25, vm);                         \
    patch((address)&insn, 31, 26, funct6);                 \
    patch_reg((address)&insn, 7, Vd);                      \
    emit(insn);                                            \
  }

  // Vector Element Index Instruction
  INSN(vid_v, 0b1010111, 0b0000010001010, 0b010100);

#undef INSN

enum Nf {
  g1 = 0b000,
  g2 = 0b001,
  g3 = 0b010,
  g4 = 0b011,
  g5 = 0b100,
  g6 = 0b101,
  g7 = 0b110,
  g8 = 0b111
};

#define patch_VLdSt(op, VReg, width, Rs1, Reg_or_umop, vm, mop, mew, nf) \
    unsigned insn = 0;                                                   \
    patch((address)&insn, 6, 0, op);                                     \
    patch((address)&insn, 14, 12, width);                                \
    patch((address)&insn, 24, 20, Reg_or_umop);                          \
    patch((address)&insn, 25, vm);                                       \
    patch((address)&insn, 27, 26, mop);                                  \
    patch((address)&insn, 28, mew);                                      \
    patch((address)&insn, 31, 29, nf);                                   \
    patch_reg((address)&insn, 7, VReg);                                  \
    patch_reg((address)&insn, 15, Rs1);                                  \
    emit(insn)

#define INSN(NAME, op, lumop, vm, mop, nf)                                           \
  void NAME(VectorRegister Vd, Register Rs1, uint32_t width = 0, bool mew = false) { \
    guarantee(is_unsigned_imm_in_range(width, 3, 0), "width is invalid");            \
    patch_VLdSt(op, Vd, width, Rs1, lumop, vm, mop, mew, nf);                        \
  }

  // Vector Load/Store Instructions
  INSN(vl1r_v, 0b0000111, 0b01000, 0b1, 0b00, g1);

#undef INSN

#define INSN(NAME, op, width, sumop, vm, mop, mew, nf)           \
  void NAME(VectorRegister Vs3, Register Rs1) {                  \
    patch_VLdSt(op, Vs3, width, Rs1, sumop, vm, mop, mew, nf);   \
  }

  // Vector Load/Store Instructions
  INSN(vs1r_v, 0b0100111, 0b000, 0b01000, 0b1, 0b00, 0b0, g1);

#undef INSN

// r2_nfvm
#define INSN(NAME, op, width, umop, mop, mew)                         \
  void NAME(VectorRegister Vd_or_Vs3, Register Rs1, Nf nf = g1) {     \
    patch_VLdSt(op, Vd_or_Vs3, width, Rs1, umop, 1, mop, mew, nf);    \
  }

  // Vector Unit-Stride Instructions
  INSN(vle1_v, 0b0000111, 0b000, 0b01011, 0b00, 0b0);
  INSN(vse1_v, 0b0100111, 0b000, 0b01011, 0b00, 0b0);

#undef INSN

#define INSN(NAME, op, width, umop, mop, mew)                                               \
  void NAME(VectorRegister Vd_or_Vs3, Register Rs1, VectorMask vm = unmasked, Nf nf = g1) { \
    patch_VLdSt(op, Vd_or_Vs3, width, Rs1, umop, vm, mop, mew, nf);                         \
  }

  // Vector Unit-Stride Instructions
  INSN(vle8_v,    0b0000111, 0b000, 0b00000, 0b00, 0b0);
  INSN(vle16_v,   0b0000111, 0b101, 0b00000, 0b00, 0b0);
  INSN(vle32_v,   0b0000111, 0b110, 0b00000, 0b00, 0b0);
  INSN(vle64_v,   0b0000111, 0b111, 0b00000, 0b00, 0b0);

  // Vector unit-stride fault-only-first Instructions
  INSN(vle8ff_v,  0b0000111, 0b000, 0b10000, 0b00, 0b0);
  INSN(vle16ff_v, 0b0000111, 0b101, 0b10000, 0b00, 0b0);
  INSN(vle32ff_v, 0b0000111, 0b110, 0b10000, 0b00, 0b0);
  INSN(vle64ff_v, 0b0000111, 0b111, 0b10000, 0b00, 0b0);

  INSN(vse8_v,  0b0100111, 0b000, 0b00000, 0b00, 0b0);
  INSN(vse16_v, 0b0100111, 0b101, 0b00000, 0b00, 0b0);
  INSN(vse32_v, 0b0100111, 0b110, 0b00000, 0b00, 0b0);
  INSN(vse64_v, 0b0100111, 0b111, 0b00000, 0b00, 0b0);

#undef INSN

#define INSN(NAME, op, width, mop, mew)                                                                  \
  void NAME(VectorRegister Vd, Register Rs1, VectorRegister Vs2, VectorMask vm = unmasked, Nf nf = g1) { \
    patch_VLdSt(op, Vd, width, Rs1, Vs2->encoding_nocheck(), vm, mop, mew, nf);                          \
  }

  // Vector unordered indexed load instructions
  INSN(vluxei8_v,  0b0000111, 0b000, 0b01, 0b0);
  INSN(vluxei16_v, 0b0000111, 0b101, 0b01, 0b0);
  INSN(vluxei32_v, 0b0000111, 0b110, 0b01, 0b0);
  INSN(vluxei64_v, 0b0000111, 0b111, 0b01, 0b0);

  // Vector ordered indexed load instructions
  INSN(vloxei8_v,  0b0000111, 0b000, 0b11, 0b0);
  INSN(vloxei16_v, 0b0000111, 0b101, 0b11, 0b0);
  INSN(vloxei32_v, 0b0000111, 0b110, 0b11, 0b0);
  INSN(vloxei64_v, 0b0000111, 0b111, 0b11, 0b0);
#undef INSN

#define INSN(NAME, op, width, mop, mew)                                                                  \
  void NAME(VectorRegister Vd, Register Rs1, Register Rs2, VectorMask vm = unmasked, Nf nf = g1) {       \
    patch_VLdSt(op, Vd, width, Rs1, Rs2->encoding_nocheck(), vm, mop, mew, nf);                          \
  }

  // Vector Strided Instructions
  INSN(vlse8_v,  0b0000111, 0b000, 0b10, 0b0);
  INSN(vlse16_v, 0b0000111, 0b101, 0b10, 0b0);
  INSN(vlse32_v, 0b0000111, 0b110, 0b10, 0b0);
  INSN(vlse64_v, 0b0000111, 0b111, 0b10, 0b0);

#undef INSN
#undef patch_VLdSt

#endif // CPU_RISCV_ASSEMBLER_RISCV_V_HPP
