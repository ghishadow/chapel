//===-- NoopAnalysis.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines a NoopAnalysis class that just uses the builtin transfer.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_FLOWSENSITIVE_NOOPANALYSIS_H
#define LLVM_CLANG_ANALYSIS_FLOWSENSITIVE_NOOPANALYSIS_H

#include "clang/AST/ASTContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/FlowSensitive/DataflowAnalysis.h"
#include "clang/Analysis/FlowSensitive/DataflowEnvironment.h"
#include "clang/Analysis/FlowSensitive/NoopLattice.h"

namespace clang {
namespace dataflow {

class NoopAnalysis : public DataflowAnalysis<NoopAnalysis, NoopLattice> {
public:
  /// Deprecated. Use the `DataflowAnalysisOptions` constructor instead.
  NoopAnalysis(ASTContext &Context, bool ApplyBuiltinTransfer)
      : DataflowAnalysis<NoopAnalysis, NoopLattice>(Context,
                                                    ApplyBuiltinTransfer) {}

  /// `ApplyBuiltinTransfer` controls whether to run the built-in transfer
  /// functions that model memory during the analysis. Their results are not
  /// used by `NoopAnalysis`, but tests that need to inspect the environment
  /// should enable them.
  NoopAnalysis(ASTContext &Context, DataflowAnalysisOptions Options)
      : DataflowAnalysis<NoopAnalysis, NoopLattice>(Context, Options) {}

  static NoopLattice initialElement() { return {}; }

  void transfer(const CFGElement &E, NoopLattice &L, Environment &Env) {}
};

} // namespace dataflow
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_FLOWSENSITIVE_NOOPANALYSIS_H
