//===- unittests/Analysis/FlowSensitive/CFGMatchSwitchTest.cpp ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/FlowSensitive/CFGMatchSwitch.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Analysis/CFG.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "gtest/gtest.h"

using namespace clang;
using namespace dataflow;
using namespace ast_matchers;

namespace {
// State for tracking the number of matches on each kind of CFGElement by the
// CFGMatchSwitch. Currently only tracks CFGStmt and CFGInitializer.
struct CFGElementMatches {
  unsigned StmtMatches = 0;
  unsigned InitializerMatches = 0;
};

// Returns a match switch that counts the number of local variables
// (singly-declared) and fields initialized to the integer literal 42.
auto buildCFGMatchSwitch() {
  return CFGMatchSwitchBuilder<CFGElementMatches>()
      .CaseOfCFGStmt<DeclStmt>(
          declStmt(hasSingleDecl(
              varDecl(hasInitializer(integerLiteral(equals(42)))))),
          [](const DeclStmt *, const MatchFinder::MatchResult &,
             CFGElementMatches &Counter) { Counter.StmtMatches++; })
      .CaseOfCFGInit<CXXCtorInitializer>(
          cxxCtorInitializer(withInitializer(integerLiteral(equals(42)))),
          [](const CXXCtorInitializer *, const MatchFinder::MatchResult &,
             CFGElementMatches &Counter) { Counter.InitializerMatches++; })
      .Build();
}

// Runs the match switch `MS` on the control flow graph generated from `Code`,
// tracking information in state `S`. For simplicity, this test utility is
// restricted to CFGs with a single control flow block (excluding entry and
// exit blocks) - generated by `Code` with sequential flow (i.e. no branching).
//
// Requirements:
//
//  `Code` must contain a function named `f`, the body of this function will be
//  used to generate the CFG.
template <typename State>
void applySwitchToCode(CFGMatchSwitch<State> &MS, State &S,
                       llvm::StringRef Code) {
  auto Unit = tooling::buildASTFromCodeWithArgs(Code, {"-Wno-unused-value"});
  auto &Ctx = Unit->getASTContext();
  const auto *F = selectFirst<FunctionDecl>(
      "f", match(functionDecl(isDefinition(), hasName("f")).bind("f"), Ctx));

  CFG::BuildOptions BO;
  BO.AddInitializers = true;

  auto CFG = CFG::buildCFG(F, F->getBody(), &Ctx, BO);
  auto CFGBlock = *CFG->getEntry().succ_begin();
  for (auto &Elt : CFGBlock->Elements) {
    MS(Elt, Ctx, S);
  }
}

TEST(CFGMatchSwitchTest, NoInitializationTo42) {
  CFGMatchSwitch<CFGElementMatches> Switch = buildCFGMatchSwitch();
  CFGElementMatches Counter;
  applySwitchToCode(Switch, Counter, R"(
    void f() {
      42;
    }
  )");
  EXPECT_EQ(Counter.StmtMatches, 0u);
  EXPECT_EQ(Counter.InitializerMatches, 0u);
}

TEST(CFGMatchSwitchTest, SingleLocalVarInitializationTo42) {
  CFGMatchSwitch<CFGElementMatches> Switch = buildCFGMatchSwitch();
  CFGElementMatches Counter;
  applySwitchToCode(Switch, Counter, R"(
    void f() {
      int i = 42;
    }
  )");
  EXPECT_EQ(Counter.StmtMatches, 1u);
  EXPECT_EQ(Counter.InitializerMatches, 0u);
}

TEST(CFGMatchSwitchTest, SingleFieldInitializationTo42) {
  CFGMatchSwitch<CFGElementMatches> Switch = buildCFGMatchSwitch();
  CFGElementMatches Counter;
  applySwitchToCode(Switch, Counter, R"(
    struct f {
      int i;
      f(): i(42) {}
    };
  )");
  EXPECT_EQ(Counter.StmtMatches, 0u);
  EXPECT_EQ(Counter.InitializerMatches, 1u);
}

TEST(CFGMatchSwitchTest, LocalVarAndFieldInitializationTo42) {
  CFGMatchSwitch<CFGElementMatches> Switch = buildCFGMatchSwitch();
  CFGElementMatches Counter;
  applySwitchToCode(Switch, Counter, R"(
    struct f {
      int i;
      f(): i(42) {
        int j = 42;
      }
    };
  )");
  EXPECT_EQ(Counter.StmtMatches, 1u);
  EXPECT_EQ(Counter.InitializerMatches, 1u);
}
} // namespace
