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

#include "default-functions.h"

#include "chpl/parsing/parsing-queries.h"
#include "chpl/queries/global-strings.h"
#include "chpl/queries/query-impl.h"
#include "chpl/resolution/resolution-queries.h"
#include "chpl/resolution/scope-queries.h"
#include "chpl/types/all-types.h"
#include "chpl/uast/all-uast.h"

namespace chpl {
namespace resolution {


using namespace uast;
using namespace types;


/**
  Return true if 'name' is the name of a compiler generated method.
*/
static bool isNameOfCompilerGeneratedMethod(UniqueString name) {
  // TODO: Update me over time.
  if (name == USTR("init")       ||
      name == USTR("deinit")     ||
      name == USTR("init=")) {
    return true;
  }

  return false;
}

static bool
areOverloadsPresentInDefiningScope(Context* context, const Type* type,
                                   UniqueString name) {
  const Scope* scopeForReceiverType = nullptr;

  if (auto compType = type->getCompositeType()) {
    scopeForReceiverType = scopeForId(context, compType->id());
  }

  // there is no defining scope
  if (!scopeForReceiverType) return false;

  // do not look outside the defining module
  const LookupConfig config = LOOKUP_DECLS | LOOKUP_PARENTS;

  auto vec = lookupNameInScope(context, scopeForReceiverType,
                               name,
                               config);

  // nothing found
  if (vec.size() == 0) return false;

  // loop through IDs and see if any are methods on the same type
  for (auto& ids : vec) {
    for (auto id : ids) {
      auto node = parsing::idToAst(context, id);
      assert(node);

      if (auto fn = node->toFunction()) {
        if (!fn->isMethod()) continue;

        auto ufs = UntypedFnSignature::get(context, fn);

        // TODO: way to just compute formal type instead of whole TFS?
        auto tfs = typedSignatureInitial(context, ufs);
        auto receiverQualType = tfs->formalType(0);

        // receiver type matches, return true
        if (receiverQualType.type() == type) {
          return true;
        }
      }
    }
  }

  return false;
}

bool
needCompilerGeneratedMethod(Context* context, const Type* type,
                            UniqueString name, bool parenless) {
  if (isNameOfCompilerGeneratedMethod(name)) {
    if (!areOverloadsPresentInDefiningScope(context, type, name)) {
      return true;
    }
  }

  // it's also possible that 'name' is the name of a field,
  // and we need to find the compiler-generated field accessor
  if (parenless) {
    if (isNameOfField(context, name, type)) {
      if (!areOverloadsPresentInDefiningScope(context, type, name)) {
        return true;
      }
    }
  }

  return false;
}

static TypedFnSignature*
generateInitSignature(Context* context, const CompositeType* inCompType) {
  std::vector<UntypedFnSignature::FormalDetail> ufsFormals;
  std::vector<QualifiedType> formalTypes;

  // adjust to refer to fully generic signature if needed
  auto genericCompType = inCompType->instantiatedFromCompositeType();
  auto compType = genericCompType ? genericCompType : inCompType;

  // start by adding a formal for the receiver
  auto ufsReceiver = UntypedFnSignature::FormalDetail(USTR("this"),
                                                      false,
                                                      nullptr);
  ufsFormals.push_back(std::move(ufsReceiver));

  // receiver is 'ref' because it is mutated
  formalTypes.push_back(QualifiedType(QualifiedType::REF, compType));

  // consult the fields to build up the remaining untyped formals
  const bool useGenericDefaults = false;
  auto& rf = fieldsForTypeDecl(context, compType, useGenericDefaults);

  // TODO: generic types
  if (rf.isGeneric()) {
    assert(false && "Not handled yet!");
  }

  // push all fields -> formals in order
  for (int i = 0; i < rf.numFields(); i++) {
    auto qualType = rf.fieldType(i);
    auto name = rf.fieldName(i);
    bool hasDefault = rf.fieldHasDefaultValue(i);
    const uast::Decl* node = nullptr;

    auto fd = UntypedFnSignature::FormalDetail(name, hasDefault, node);
    ufsFormals.push_back(std::move(fd));

    // for types & param, use the field kind, for values use 'in' intent
    if (qualType.isType() || qualType.isParam()) {
      formalTypes.push_back(qualType);
    } else {
      auto qt = QualifiedType(QualifiedType::IN, qualType.type());
      formalTypes.push_back(std::move(qt));
    }
  }

  // build the untyped signature
  auto ufs = UntypedFnSignature::get(context,
                        /*id*/ compType->id(),
                        /*name*/ USTR("init"),
                        /*isMethod*/ true,
                        /*isTypeConstructor*/ false,
                        /*isCompilerGenerated*/ true,
                        /*idTag*/ parsing::idToTag(context, compType->id()),
                        /*kind*/ uast::Function::Kind::PROC,
                        /*formals*/ std::move(ufsFormals),
                        /*whereClause*/ nullptr);

  // now build the other pieces of the typed signature
  auto whereClauseResult = TypedFnSignature::WHERE_NONE;
  bool needsInstantiation = rf.isGeneric();
  const TypedFnSignature* instantiatedFrom = nullptr;
  const TypedFnSignature* parentFn = nullptr;
  Bitmap formalsInstantiated;

  auto ret = new TypedFnSignature(ufs, formalTypes, whereClauseResult,
                                  needsInstantiation,
                                  instantiatedFrom,
                                  parentFn,
                                  formalsInstantiated);

  return ret;
}

TypedFnSignature*
generateFieldAccessor(Context* context, UniqueString name,
                      const CompositeType* compType) {
  std::vector<UntypedFnSignature::FormalDetail> ufsFormals;
  std::vector<QualifiedType> formalTypes;

  // start by adding a formal for the receiver
  auto ufsReceiver = UntypedFnSignature::FormalDetail(USTR("this"),
                                                      false,
                                                      nullptr);
  ufsFormals.push_back(std::move(ufsReceiver));

  const Type* thisType = compType;
  if (auto bct = thisType->toBasicClassType()) {
    auto dec = ClassTypeDecorator(ClassTypeDecorator::BORROWED_NONNIL);
    thisType = ClassType::get(context, bct, /*manager*/ nullptr, dec);
  }

  // receiver is 'ref' to allow mutation
  // TODO: indicate that its const-ness should vary with receiver const-ness
  formalTypes.push_back(QualifiedType(QualifiedType::REF, thisType));

  // build the untyped signature
  auto ufs = UntypedFnSignature::get(context,
                        /*id*/ compType->id(),
                        /*name*/ name,
                        /*isMethod*/ true,
                        /*isTypeConstructor*/ false,
                        /*isCompilerGenerated*/ true,
                        /*idTag*/ parsing::idToTag(context, compType->id()),
                        /*kind*/ uast::Function::Kind::PROC,
                        /*formals*/ std::move(ufsFormals),
                        /*whereClause*/ nullptr);

  // now build the other pieces of the typed signature
  auto whereClauseResult = TypedFnSignature::WHERE_NONE;
  bool needsInstantiation = false;
  const TypedFnSignature* instantiatedFrom = nullptr;
  const TypedFnSignature* parentFn = nullptr;
  Bitmap formalsInstantiated;

  auto ret = new TypedFnSignature(ufs, formalTypes, whereClauseResult,
                                  needsInstantiation,
                                  instantiatedFrom,
                                  parentFn,
                                  formalsInstantiated);

  return ret;
}

static const owned<TypedFnSignature>&
getCompilerGeneratedMethodQuery(Context* context, const Type* type,
                                UniqueString name, bool parenless) {
  QUERY_BEGIN(getCompilerGeneratedMethodQuery, context, type, name, parenless);

  TypedFnSignature* tfs = nullptr;

  if (needCompilerGeneratedMethod(context, type, name, parenless)) {
    auto compType = type->toCompositeType();
    if (auto clsType = type->toClassType()) {
      compType = clsType->basicClassType();
    }
    assert(compType);

    if (name == USTR("init")) {
      tfs = generateInitSignature(context, compType);
    } else if (isNameOfField(context, name, compType)) {
      tfs = generateFieldAccessor(context, name, compType);
    } else {
      assert(false && "Not implemented yet!");
    }
  }

  auto ret = toOwned(tfs);

  return QUERY_END(ret);
}

/**
  Given a type and a UniqueString representing the name of a method,
  determine if the type needs a method with such a name to be
  generated for it, and if so, generates and returns a
  TypedFnSignature representing the generated method.

  If no method was generated, returns nullptr.
*/
const TypedFnSignature*
getCompilerGeneratedMethod(Context* context, const Type* type,
                           UniqueString name, bool parenless) {
  auto& owned = getCompilerGeneratedMethodQuery(context, type, name, parenless);
  auto ret = owned.get();
  return ret;
}


} // end namespace resolution
} // end namespace chpl
