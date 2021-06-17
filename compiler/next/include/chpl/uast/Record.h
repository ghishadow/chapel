/*
 * Copyright 2021 Hewlett Packard Enterprise Development LP
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

#ifndef CHPL_UAST_RECORD_H
#define CHPL_UAST_RECORD_H

#include "chpl/queries/Location.h"
#include "chpl/uast/AggregateDecl.h"
#include "chpl/uast/Comment.h"
#include "chpl/uast/Identifier.h"

namespace chpl {
namespace uast {


/**
  This class represents a record declaration. For example:

  \rst
  .. code-block:: chapel

      record myRecord {
        var a: int;
        proc method() { }
      }

  \endrst

  The record itself (myRecord) is represented by a Record AST node. The Record
  AST node contains Field and Function children.
 */
class Record final : public AggregateDecl {
 private:
  Record(ASTList children, Visibility vis, UniqueString name, int numChildren)
    : AggregateDecl(asttags::Record, std::move(children), vis, name,
                    (numChildren>0)?0:-1, numChildren) {
  }

  bool contentsMatchInner(const ASTNode* other) const override {
    const Record* lhs = this;
    const Record* rhs = (const Record*) other;
    return lhs->aggregateDeclContentsMatchInner(rhs);
  }
  void markUniqueStringsInner(Context* context) const override {
    aggregateDeclMarkUniqueStringsInner(context);
  }

 public:
  ~Record() override = default;

  static owned<Record> build(Builder* builder, Location loc,
                             Decl::Visibility vis, UniqueString name,
                             ASTList contents);
};


} // end namespace uast
} // end namespace chpl

#endif
