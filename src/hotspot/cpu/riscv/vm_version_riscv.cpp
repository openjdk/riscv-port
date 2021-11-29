/*
 * Copyright (c) 1997, 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2020, 2021, Huawei Technologies Co., Ltd. All rights reserved.
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

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"
#include "runtime/stubCodeGenerator.hpp"
#include "runtime/vm_version.hpp"
#include "utilities/formatBuffer.hpp"
#include "utilities/macros.hpp"

#include OS_HEADER_INLINE(os)

static BufferBlob* stub_blob;
static const int vlen_stub_size=100;

extern "C" {
  typedef uint32_t (*get_vector_len_stub_t)();
}
static get_vector_len_stub_t get_vector_len_stub = NULL;

class VM_Version_StubGenerator: public StubCodeGenerator {
 public:

  VM_Version_StubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}
  ~VM_Version_StubGenerator() {}

  address generate_get_vector_len_stub(address* fault_pc, address* fault_pc2, address* continuation_pc) {
    StubCodeMark mark(this, "VM_Version", "get_vector_len_stub");
#   define __ _masm->
    address start = __ pc();

    __ enter();

    // read vcsr and vlenb, it may raise sigill
    __ mv(x10, zr);
    *fault_pc = __ pc();
    __ csrr(x10, CSR_VCSR);

    __ mv(x10, zr);
    *fault_pc2 = __ pc();
    __ csrr(x10, CSR_VLENB);

    *continuation_pc = __ pc();
    __ leave();
    __ ret();

#   undef __
    return start;
  }
};

uint32_t VM_Version::_initial_vector_length = 0;
address  VM_Version::_checkvext_fault_pc = NULL;
address  VM_Version::_checkvext_fault_pc2 = NULL;
address  VM_Version::_checkvext_continuation_pc = NULL;

void VM_Version::get_processor_features() {
  if (FLAG_IS_DEFAULT(UseFMA)) {
    FLAG_SET_DEFAULT(UseFMA, true);
  }
  if (FLAG_IS_DEFAULT(AllocatePrefetchDistance)) {
    FLAG_SET_DEFAULT(AllocatePrefetchDistance, 0);
  }

  if (UseAES || UseAESIntrinsics) {
    if (UseAES && !FLAG_IS_DEFAULT(UseAES)) {
      warning("AES instructions are not available on this CPU");
      FLAG_SET_DEFAULT(UseAES, false);
    }
    if (UseAESIntrinsics && !FLAG_IS_DEFAULT(UseAESIntrinsics)) {
      warning("AES intrinsics are not available on this CPU");
      FLAG_SET_DEFAULT(UseAESIntrinsics, false);
    }
  }

  if (UseAESCTRIntrinsics) {
    warning("AES/CTR intrinsics are not available on this CPU");
    FLAG_SET_DEFAULT(UseAESCTRIntrinsics, false);
  }

  if (UseSHA) {
    warning("SHA instructions are not available on this CPU");
    FLAG_SET_DEFAULT(UseSHA, false);
  }

  if (UseSHA1Intrinsics) {
    warning("Intrinsics for SHA-1 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA1Intrinsics, false);
  }

  if (UseSHA256Intrinsics) {
    warning("Intrinsics for SHA-224 and SHA-256 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA256Intrinsics, false);
  }

  if (UseSHA512Intrinsics) {
    warning("Intrinsics for SHA-384 and SHA-512 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA512Intrinsics, false);
  }

  if (UseSHA3Intrinsics) {
    warning("Intrinsics for SHA3-224, SHA3-256, SHA3-384 and SHA3-512 crypto hash functions not available on this CPU.");
    FLAG_SET_DEFAULT(UseSHA3Intrinsics, false);
  }

  if (UsePopCountInstruction) {
    warning("Pop count instructions are not available on this CPU.");
    FLAG_SET_DEFAULT(UsePopCountInstruction, false);
  }

  if (UseCRC32Intrinsics) {
    warning("CRC32 intrinsics are not available on this CPU.");
    FLAG_SET_DEFAULT(UseCRC32Intrinsics, false);
  }

  if (UseCRC32CIntrinsics) {
    warning("CRC32C intrinsics are not available on this CPU.");
    FLAG_SET_DEFAULT(UseCRC32CIntrinsics, false);
  }

  if (UseMD5Intrinsics) {
    warning("MD5 intrinsics are not available on this CPU.");
    FLAG_SET_DEFAULT(UseMD5Intrinsics, false);
  }

  if (UseRVV) {
    if (!(_features & CPU_V)) {
      warning("RVV is not supported on this CPU");
      FLAG_SET_DEFAULT(UseRVV, false);
    } else {
      // read vector length from vector CSR vlenb

      // try to read vector register VLENB, if success, rvv is supported
      // otherwise, csrr will trigger sigill
      ResourceMark rm;

      stub_blob = BufferBlob::create("get_vector_len_stub", vlen_stub_size);
      if (stub_blob == NULL) {
        vm_exit_during_initialization("Unable to allocate get_vector_len_stub");
      }

      CodeBuffer c(stub_blob);
      VM_Version_StubGenerator g(&c);
      get_vector_len_stub = CAST_TO_FN_PTR(get_vector_len_stub_t,
                                     g.generate_get_vector_len_stub(&VM_Version::_checkvext_fault_pc,
                                                &VM_Version::_checkvext_fault_pc2,
                                                &VM_Version::_checkvext_continuation_pc));
      _initial_vector_length = get_vector_len_stub();
      if (_initial_vector_length == 0) {
        FLAG_SET_DEFAULT(UseRVV, false);
      }
    }
  }

  if (FLAG_IS_DEFAULT(AvoidUnalignedAccesses)) {
    FLAG_SET_DEFAULT(AvoidUnalignedAccesses, true);
  }

#ifdef COMPILER2
  get_c2_processor_features();
#endif // COMPILER2
}

#ifdef COMPILER2
void VM_Version::get_c2_processor_features() {
  // lack of cmove in riscv64
  if (UseCMoveUnconditionally) {
    FLAG_SET_DEFAULT(UseCMoveUnconditionally, false);
  }
  if (ConditionalMoveLimit > 0) {
    FLAG_SET_DEFAULT(ConditionalMoveLimit, 0);
  }

  if (!UseRVV) {
    FLAG_SET_DEFAULT(SpecialEncodeISOArray, false);
  }

  if (!UseRVV && MaxVectorSize) {
    FLAG_SET_DEFAULT(MaxVectorSize, 0);
  }

  if (UseRVV) {
    if (FLAG_IS_DEFAULT(MaxVectorSize)) {
      MaxVectorSize = _initial_vector_length;
    } else if (MaxVectorSize < 16) {
      warning("RVV does not support vector length less than 16 bytes. Disabling RVV.");
      UseRVV = false;
    } else if (is_power_of_2(MaxVectorSize)) {
      if (MaxVectorSize > _initial_vector_length) {
        warning("Current system only supports max RVV vector length %d. Set MaxVectorSize to %d",
                _initial_vector_length, _initial_vector_length);
      }
      MaxVectorSize = _initial_vector_length;
    } else {
      vm_exit_during_initialization(err_msg("Unsupported MaxVectorSize: %d", (int)MaxVectorSize));
    }
  }

  // disable prefetch
  if (FLAG_IS_DEFAULT(AllocatePrefetchStyle)) {
    FLAG_SET_DEFAULT(AllocatePrefetchStyle, 0);
  }
}
#endif // COMPILER2

void VM_Version::initialize() {
  get_cpu_info();
  get_processor_features();
}
