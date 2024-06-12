/*
 * Copyright 2021-2024 Hewlett Packard Enterprise Development LP
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
#include "chpl/types/EnumType.h"

#include "chpl/parsing/parsing-queries.h"
#include "chpl/framework/query-impl.h"

namespace chpl {
namespace types {

const owned<EnumType>&
EnumType::getEnumType(Context* context, ID id, UniqueString name) {
  QUERY_BEGIN(getEnumType, context, id, name);

  bool isAbstract = true;
  bool isConcrete = false;

  // An enum is abstract if none of its elements have an init-part. It is
  // concrete if the _first_ element has an init-part.
  if (id) {
    if (auto ast = parsing::idToAst(context, id)) {
      if (auto et = ast->toEnum()) {
        bool first = true;
        for (auto e : et->enumElements()) {
          if (e->initExpression()) {
            isAbstract = false;
            isConcrete = first;
            break;
          }
          first = false;
        }
      }
    }
  }

  auto result = toOwned(new EnumType(id, name, isAbstract, isConcrete));

  return QUERY_END(result);
}

const EnumType* EnumType::get(Context* context, ID id, UniqueString name) {
  return EnumType::getEnumType(context, id, name).get();
}

const EnumType* EnumType::getBoundKindType(Context* context) {
  auto name = UniqueString::get(context, "boundKind");
  auto id = parsing::getSymbolFromTopLevelModule(context, "ChapelRange", "boundKind");
  return EnumType::get(context, id, name);
}

void EnumType::stringify(std::ostream& ss, StringifyKind stringKind) const {
  name().stringify(ss, stringKind);
}

} // end namespace types
} // end namespace chpl
