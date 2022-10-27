/*
 * Copyright 2021-2022 Hewlett Packard Enterprise Development LP
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "call-init-deinit.h"

#include "Resolver.h"

#include "chpl/parsing/parsing-queries.h"
#include "chpl/resolution/can-pass.h"
#include "chpl/resolution/ResolvedVisitor.h"
#include "chpl/resolution/resolution-types.h"
#include "chpl/resolution/scope-queries.h"
#include "chpl/resolution/copy-elision.h"
#include "chpl/resolution/split-init.h"
#include "chpl/uast/all-uast.h"

namespace chpl {
namespace resolution {


using namespace uast;
using namespace types;


// TODO -- figure out where to store copy (associatedFns?)
//         and where to store deinit (associatedFns not so good).
//         For now it just prints these.
//
// TODO -- a default argument can have a RHS that is a reference
//         even though it is 'in' intent. As such, it would require
//         a copy, but it's hard to associate that information
//         with a call actual (because the actual doesn't exist).

struct Action {
  enum ActionKind {
    COPY_INIT, // for 'in'
    WRITE_BACK, // for 'out' and 'inout'
    DEINIT,
  };
  ActionKind action; // which action?
  ID id;             // which ID?
  Action(ActionKind action, ID id) : action(action), id(id) { }
};

// for blocks / things that behave like blocks
struct ScopeFrame {
  const Scope* scope = nullptr;

  // localsAndDefers contains both VarSymbol and DeferStmt in
  // order to create a single stack for cleanup operations to be executed.
  // In particular, the ordering between defer blocks and locals matters,
  // in addition to the ordering within each group.
  std::vector<const AstNode*> localsAndDefers;

  // Which variables are declared in this scope?
  std::set<const VarLikeDecl*> declaredVars;

  // Which variables are initialized in this scope
  // (possibly including outer variables)?
  std::set<const VarLikeDecl*> initedVars;

  // Which outer variables have been initialized in this scope?
  // This vector lists them in initialization order.
  std::vector<const VarLikeDecl*> initedOuterVars;

  // Which variables have been deinitialized early in this scope?
  std::set<const VarLikeDecl*> deinitedVars;

  // What actions should be taken at the end of the scope?
  std::vector<Action> endOfScopeActions;

  // Stores the state for variables that are currently
  //std::map<ID, InitDeinitState> varState;

  ScopeFrame(const Scope* scope) : scope(scope) { }
};

// Resolves init, deinit, and assign
// TODO: should it be renamed to include Assign?
struct CallInitDeinit {
  using RV = MutatingResolvedVisitor<CallInitDeinit>;

  // inputs to the process
  Context* context = nullptr;
  Resolver& resolver;
  std::set<ID> splitInitedVars;
  std::set<ID> elidedCopyFromIds;

  // internal variables

  // for handling calls, nested calls, end of statement actions
  std::vector<const Call*> callStack;
  std::vector<Action> endOfStatementActions;

  // for handling end of block / end of scope actions
  std::vector<ScopeFrame> scopeStack;

  // main entry point to this code
  // updates the ResolutionResultsByPostorderID
  static void process(Resolver& resolver,
                      std::set<ID> splitInitedVars,
                      std::set<ID> elidedCopyFromIds);

  void printActions(const std::vector<Action>& actions);

  CallInitDeinit(Resolver& resolver,
                 std::set<ID> splitInitedVars,
                 std::set<ID> elidedCopyFromIds);

  void checkValidAssignTypes(QualifiedType lhsType, QualifiedType rhsType);

  void enterScope(const uast::AstNode* ast);
  void exitScope(const uast::AstNode* ast);

  bool enter(const VarLikeDecl* ast, RV& rv);
  void exit(const VarLikeDecl* ast, RV& rv);

  bool enter(const OpCall* ast, RV& rv);
  void exit(const OpCall* ast, RV& rv);

  bool enter(const Call* ast, RV& rv);
  void exit(const Call* ast, RV& rv);

  bool enter(const uast::AstNode* node, RV& rv);
  void exit(const uast::AstNode* node, RV& rv);
};



void CallInitDeinit::process(Resolver& resolver,
                             std::set<ID> splitInitedVars,
                             std::set<ID> elidedCopyFromIds) {
  CallInitDeinit uv(resolver,
                    std::move(splitInitedVars),
                    std::move(elidedCopyFromIds));
  MutatingResolvedVisitor<CallInitDeinit> rv(resolver.context,
                                             resolver.symbol,
                                             uv,
                                             resolver.byPostorder);

  resolver.symbol->traverse(rv);
}

void CallInitDeinit::printActions(const std::vector<Action>& actions) {
  for (auto act : actions) {
    switch (act.action) {
      case Action::COPY_INIT:
        printf("copy-init %s\n", act.id.str().c_str());
        break;
      case Action::WRITE_BACK:
        printf("writeback %s\n", act.id.str().c_str());
        break;
      case Action::DEINIT:
        printf("deinit %s\n", act.id.str().c_str());
        break;
    }
  }
}

CallInitDeinit::CallInitDeinit(Resolver& resolver,
                               std::set<ID> splitInitedVars,
                               std::set<ID> elidedCopyFromIds)
  : context(resolver.context), resolver(resolver),
    splitInitedVars(std::move(splitInitedVars)),
    elidedCopyFromIds(std::move(elidedCopyFromIds))
{
}

// if lhsType is not a record, check
void CallInitDeinit::checkValidAssignTypes(QualifiedType lhsType, QualifiedType rhsType) {
}

void CallInitDeinit::enterScope(const AstNode* ast) {
  if (createsScope(ast->tag())) {
    const Scope* scope = scopeForId(context, ast->id());
    scopeStack.push_back(ScopeFrame(scope));
  }
}
void CallInitDeinit::exitScope(const AstNode* ast) {
  if (createsScope(ast->tag())) {
    ScopeFrame& frame = scopeStack.back();
    printActions(frame.endOfScopeActions);

    assert(!scopeStack.empty());
    scopeStack.pop_back();
  }
}

bool CallInitDeinit::enter(const VarLikeDecl* ast, RV& rv) {
  printf("ENTER VAR %s\n", ast->id().str().c_str());

  enterScope(ast);

  return true;
}
void CallInitDeinit::exit(const VarLikeDecl* ast, RV& rv) {
  assert(!scopeStack.empty());
  if (!scopeStack.empty()) {
    ScopeFrame& frame = scopeStack.back();
    frame.declaredVars.insert(ast);
  }

  exitScope(ast);
}

// TODO: visit nested calls & record their IDs
// in InitDeinitState to record required deinit actions

bool CallInitDeinit::enter(const OpCall* ast, RV& rv) {
  printf("ENTER OP %s\n", ast->id().str().c_str());

  if (auto op = ast->toOpCall()) {
    if (op->op() == USTR("=")) {
      // What is the RHS and LHS of the '=' call?
      auto lhsAst = ast->actual(0);
      auto rhsAst = ast->actual(1);

      // TODO: should it visit the RHS first?

      ResolvedExpression& lhsRe = rv.byAst(lhsAst);
      QualifiedType lhsType = lhsRe.type();
      ID toId = lhsRe.toId();

      ResolvedExpression& rhsRe = rv.byAst(rhsAst);
      QualifiedType rhsType = rhsRe.type();

      bool resolveAssign = false;
      bool resolveInitAssign = false;

      if (lhsType.isType() || lhsType.isParam()) {
        // these are basically 'move' initialization
      } else if (!toId.isEmpty() && splitInitedVars.count(toId) > 0) {
        if (elidedCopyFromIds.count(rhsAst->id())) {
          // it is move initialization

          // Future TODO: might need to call something provided by the record
          // author to be a hook for move initialization across locales
          // (see issue #15676).

          // Otherwise, no need to resolve anything else.
        } else {
          // it is copy initialization, so use init= for records
          // TODO: and tuples?
          if (lhsType.type() != nullptr && lhsType.type()->isRecordType()) {
            resolveInitAssign = true;
          } else {
            resolveAssign = true;
          }
        }
      } else {
        // it is assignment, so resolve the '=' call
        resolveAssign = true;
      }

      if (resolveAssign) {
        printf("Resolving =\n");

        std::vector<CallInfoActual> actuals;
        actuals.push_back(CallInfoActual(lhsType, UniqueString()));
        actuals.push_back(CallInfoActual(rhsType, UniqueString()));
        auto ci = CallInfo (/* name */ USTR("="),
                            /* calledType */ QualifiedType(),
                            /* isMethodCall */ false,
                            /* hasQuestionArg */ false,
                            /* isParenless */ false,
                            actuals);
        const Scope* scope = scopeForId(context, op->id());
        auto c = resolveGeneratedCall(context, op, ci, scope,
                                      resolver.poiScope);
        ResolvedExpression& opR = rv.byAst(op);
        resolver.handleResolvedAssociatedCall(opR, op, ci, c);
      } else if (resolveInitAssign) {
        printf("Resolving init=n");

        std::vector<CallInfoActual> actuals;
        actuals.push_back(CallInfoActual(lhsType, USTR("this")));
        actuals.push_back(CallInfoActual(rhsType, UniqueString()));
        auto ci = CallInfo (/* name */ USTR("init="),
                            /* calledType */ QualifiedType(),
                            /* isMethodCall */ true,
                            /* hasQuestionArg */ false,
                            /* isParenless */ false,
                            actuals);
        const Scope* scope = scopeForId(context, op->id());
        auto c = resolveGeneratedCall(context, op, ci, scope,
                                      resolver.poiScope);
        ResolvedExpression& opR = rv.byAst(op);
        resolver.handleResolvedAssociatedCall(opR, op, ci, c);
      } else {
        // if it's move initialization, check that the types are compatable
        if (!canPass(context, rhsType, lhsType).passes()) {
          context->error(ast, "types not compatable for move-init");
        }
      }
    }
  }

  callStack.push_back(ast);

  return true;
}
void CallInitDeinit::exit(const OpCall* ast, RV& rv) {
  // TODO: handle in/out/inout temporaries for nested calls by adding
  // to the call's CallFrame (which is currently at callStack.back()).
  callStack.pop_back();
}


bool CallInitDeinit::enter(const Call* ast, RV& rv) {
  callStack.push_back(ast);

  return true;
}
void CallInitDeinit::exit(const Call* ast, RV& rv) {
  // TODO: handle in/out/inout temporaries for nested calls by adding
  // to the call's CallFrame (which is currently at callStack.back()).
  callStack.pop_back();
}


bool CallInitDeinit::enter(const AstNode* ast, RV& rv) {
  printf("ENTER AST %s\n", ast->id().str().c_str());

  enterScope(ast);

  return true;
}
void CallInitDeinit::exit(const AstNode* ast, RV& rv) {
  exitScope(ast);
}


void callInitDeinit(Resolver& resolver) {
  printf("IN CALLINITDEINIT\n");

  std::set<ID> splitInitedVars = computeSplitInits(resolver.context,
                                                   resolver.symbol,
                                                   resolver.byPostorder);
  std::set<ID> elidedCopyFromIds = computeElidedCopies(resolver.context,
                                                       resolver.symbol,
                                                       resolver.byPostorder);

  printf("\nSplit Init Report:\n");
  for (auto varId : splitInitedVars) {
    auto ast = parsing::idToAst(resolver.context, varId);
    if (ast) {
      if (auto vd = ast->toVarLikeDecl()) {
        printf("  Split initing '%s' with ID %s\n",
               vd->name().c_str(), vd->id().str().c_str());
      }
    }
  }
  printf("\n");

  printf("\nCopy Elision Report:\n");
  for (auto id : elidedCopyFromIds) {
    printf("  Copy eliding ID %s\n",
           id.str().c_str());
  }
  printf("\n");


  CallInitDeinit::process(resolver,
                          std::move(splitInitedVars),
                          std::move(elidedCopyFromIds));
}

} // end namespace resolution
} // end namespace chpl
