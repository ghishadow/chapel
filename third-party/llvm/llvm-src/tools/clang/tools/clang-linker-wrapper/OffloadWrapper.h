//===- OffloadWrapper.h -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_CLANG_LINKER_WRAPPER_OFFLOAD_WRAPPER_H
#define LLVM_CLANG_TOOLS_CLANG_LINKER_WRAPPER_OFFLOAD_WRAPPER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/Module.h"

/// Wrap the input device images into the module \p M as global symbols and
/// registers the images with the OpenMP Offloading runtime libomptarget.
llvm::Error wrapBinaries(llvm::Module &M,
                         llvm::ArrayRef<llvm::ArrayRef<char>> Images);

#endif
