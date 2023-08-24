/*
 * Copyright 2021-2023 Hewlett Packard Enterprise Development LP
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

#include "chpl/uast/AggregateDecl.h"

#include "chpl/uast/Builder.h"
#include "chpl/uast/FnCall.h"
#include "chpl/uast/Identifier.h"

namespace chpl {
namespace uast {


bool AggregateDecl::validAggregateChildren(AstListIteratorPair<AstNode> it) {
  for (auto elt: it) {
    if (elt->isComment() || elt->isErroneousExpression() ||
        elt->isEmptyStmt()) {
      // OK
    } else if (elt->isDecl()) {
      if (elt->isVariable() || elt->isFunction() || elt->isTupleDecl() ||
          elt->isMultiDecl() ||
          elt->isAggregateDecl() ||
          elt->isForwardingDecl() ||
          elt->isTypeDecl()) {
        // OK
      } else {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

AggregateDecl::~AggregateDecl() {
}

const Identifier* AggregateDecl::getInheritExprIdent(const AstNode* ast,
                                                     bool& markedGeneric) {
  if (ast != nullptr) {
    if (ast->isIdentifier()) {
      // inheriting from e.g. Parent is OK
      markedGeneric = false;
      return ast->toIdentifier();
    } else if (auto call = ast->toFnCall()) {
      const AstNode* calledExpr = call->calledExpression();
      if (calledExpr != nullptr && calledExpr->isIdentifier() &&
          call->numActuals() == 1) {
        if (const AstNode* actual = call->actual(0)) {
          if (auto id = actual->toIdentifier()) {
            if (id->name() == USTR("?")) {
              // inheriting from e.g. Parent(?) is OK
              markedGeneric = true;
              return calledExpr->toIdentifier();
            }
          }
        }
      }
    }
  }

  markedGeneric = false;
  return nullptr;
}

bool AggregateDecl::isAcceptableInheritExpr(const AstNode* ast) {
  bool ignoredMarkedGeneric = false;
  return getInheritExprIdent(ast, ignoredMarkedGeneric) != nullptr;
}


} // namespace uast
} // namespace chpl
