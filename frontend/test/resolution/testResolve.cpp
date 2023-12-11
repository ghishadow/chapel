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

#include "test-resolution.h"

#include "chpl/parsing/parsing-queries.h"
#include "chpl/resolution/resolution-queries.h"
#include "chpl/resolution/scope-queries.h"
#include "chpl/types/IntType.h"
#include "chpl/types/QualifiedType.h"
#include "chpl/uast/Comment.h"
#include "chpl/uast/FnCall.h"
#include "chpl/uast/Identifier.h"
#include "chpl/uast/Module.h"
#include "chpl/uast/Variable.h"

// test resolving a very simple module
static void test1() {
  printf("test1\n");
  Context ctx;
  Context* context = &ctx;

  {
    context->advanceToNextRevision(true);
    auto path = UniqueString::get(context, "input.chpl");
    std::string contents = "var x: int;\n"
                           "x;";
    setFileText(context, path, contents);

    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    assert(m->numStmts() == 2);
    const Variable* x = m->stmt(0)->toVariable();
    assert(x);
    const Identifier* xIdent = m->stmt(1)->toIdentifier();
    assert(xIdent);

    const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());

    assert(rr.byAst(x).type().type()->isIntType());
    assert(rr.byAst(xIdent).type().type()->isIntType());
    assert(rr.byAst(xIdent).toId() == x->id());

    context->collectGarbage();
  }
}

// test resolving a module in an incremental manner
static void test2() {
  printf("test2\n");
  Context ctx;
  Context* context = &ctx;

  {
    printf("part 1\n");
    context->advanceToNextRevision(true);
    auto path = UniqueString::get(context, "input.chpl");
    std::string contents = "";
    setFileText(context, path, contents);

    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    resolveModule(context, m->id());

    context->collectGarbage();
  }

  {
    printf("part 2\n");
    context->advanceToNextRevision(true);
    auto path = UniqueString::get(context, "input.chpl");
    std::string contents = "var x;";
    setFileText(context, path, contents);

    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    resolveModule(context, m->id());

    context->collectGarbage();
  }

  {
    printf("part 3\n");
    context->advanceToNextRevision(true);
    auto path = UniqueString::get(context, "input.chpl");
    std::string contents = "var x: int;";
    setFileText(context, path, contents);

    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);

    const Variable* x = m->stmt(0)->toVariable();
    assert(x);

    const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
    assert(rr.byAst(x).type().type()->isIntType());

    context->collectGarbage();
  }


  // Run it a few times to make sure there aren't errors related to
  // collectGarbage being run across multiple revisions.
  for (int i = 0; i < 3; i++) {
    printf("part %i\n", 3+i);
    context->advanceToNextRevision(true);
    auto path = UniqueString::get(context, "input.chpl");
    std::string contents = "var x: int;\n"
                           "x;";
    setFileText(context, path, contents);

    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    assert(m->numStmts() == 2);
    const Variable* x = m->stmt(0)->toVariable();
    assert(x);
    const Identifier* xIdent = m->stmt(1)->toIdentifier();
    assert(xIdent);

    const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());

    assert(rr.byAst(x).type().type()->isIntType());
    assert(rr.byAst(xIdent).type().type()->isIntType());
    assert(rr.byAst(xIdent).toId() == x->id());

    context->collectGarbage();
  }
}

static void test3() {
  printf("test3\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");

  {
    printf("part 1\n");
    context->advanceToNextRevision(true);
    std::string contents = "proc foo(arg: int) {\n"
                           "  return arg;\n"
                           "}\n"
                           "var y = foo(1);";
    setFileText(context, path, contents);
    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    const Function* procfoo = m->stmt(0)->toFunction();
    assert(procfoo && procfoo->name() == "foo");
    const Variable* y = m->stmt(1)->toVariable();
    assert(y);
    const AstNode* rhs = y->initExpression();
    assert(rhs);
    const FnCall* fnc = rhs->toFnCall();
    assert(fnc);
    const Identifier* foo = fnc->calledExpression()->toIdentifier();
    assert(foo && foo->name() == "foo");

    const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
    auto rrfoo = rr.byAst(foo);
    assert(rrfoo.toId() == procfoo->id());

    auto yt = rr.byAst(y).type().type();
    assert(yt->isIntType());

    context->collectGarbage();
  }

  {
    printf("part 2\n");
    context->advanceToNextRevision(true);
    std::string contents = "var y = foo(1);";
    setFileText(context, path, contents);
    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    const Variable* y = m->stmt(0)->toVariable();
    assert(y);
    const AstNode* rhs = y->initExpression();
    assert(rhs);
    const FnCall* fnc = rhs->toFnCall();
    assert(fnc);
    const Identifier* foo = fnc->calledExpression()->toIdentifier();
    assert(foo && foo->name() == "foo");

    const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
    auto rrfoo = rr.byAst(foo);
    assert(rrfoo.toId().isEmpty());

    auto rry = rr.byAst(y);
    auto yt = rry.type().type();
    assert(yt);
    assert(yt->isErroneousType());

    context->collectGarbage();
  }
}

// this test combines several ideas and is a more challenging
// case for instantiation, conversions, and type construction
static void test4() {
  printf("test4\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = R""""(
                           module M {
                             class Parent { }
                             class C : Parent { type t; var x: t; }

                             proc f(in arg: Parent) { }
                             var x: owned C(int);
                             f(x);
                          }
                        )"""";

  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);
  const Module* m = vec[0]->toModule();
  assert(m);
  assert(m->numStmts() == 5);
  const Call* call = m->stmt(4)->toCall();
  assert(call);

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  const ResolvedExpression& re = rr.byAst(call);

  assert(re.type().type()->isVoidType());

  auto c = re.mostSpecific().only();
  assert(c);
  assert(c.fn()->untyped()->name() == "f");
}

// this test checks a simple instantiation situation
static void test5() {
  printf("test5\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = R""""(
                           module M {
                             var x:int(64);
                             var y = x;
                             proc f(arg) { }
                             f(y);
                           }
                        )"""";

  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);
  const Module* m = vec[0]->toModule();
  assert(m);
  assert(m->numStmts() == 4);
  const Call* call = m->stmt(3)->toCall();
  assert(call);

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  const ResolvedExpression& re = rr.byAst(call);

  assert(re.type().type()->isVoidType());

  auto c = re.mostSpecific().only();
  assert(c);
  assert(c.fn()->untyped()->name() == "f");

  assert(c.fn()->numFormals() == 1);
  assert(c.fn()->formalName(0) == "arg");
  assert(c.fn()->formalType(0).kind() == QualifiedType::CONST_IN);
  assert(c.fn()->formalType(0).type() == IntType::get(context, 64));
}

// this test checks a particular incremental pattern
// that crashed earlier versions
static void test6() {
  printf("test6\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");

  {
    printf("part 1\n");
    context->advanceToNextRevision(true);
    std::string contents = R""""(
                              module M {
                                var x = 1;
                                proc f() { return x; }
                              }
                           )"""";

    setFileText(context, path, contents);
    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);
    resolveModule(context, m->id());

    context->collectGarbage();
  }

  {
    printf("part 2\n");
    context->advanceToNextRevision(true);
    std::string contents = R""""(
                              module M {
                                var x = 1;
                                proc f() { return x; }
                                f();
                              }
                           )"""";

    setFileText(context, path, contents);
    const ModuleVec& vec = parseToplevel(context, path);
    assert(vec.size() == 1);
    const Module* m = vec[0]->toModule();
    assert(m);

    resolveModule(context, m->id());

    context->collectGarbage();
  }
}

// check a parenless function call
static void test7() {
  printf("test7\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = R""""(
                           module M {
                             proc parenless { return 1; }
                             parenless;
                           }
                        )"""";

  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);
  const Module* m = vec[0]->toModule();
  assert(m);
  assert(m->numStmts() == 2);
  const Identifier* ident = m->stmt(1)->toIdentifier();
  assert(ident);

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  const ResolvedExpression& re = rr.byAst(ident);

  assert(re.type().type());
  assert(re.type().type()->isIntType());

  auto c = re.mostSpecific().only();
  assert(c);
  assert(c.fn()->untyped()->name() == "parenless");
  assert(c.fn()->numFormals() == 0);
}

// check a simple recursive function
static void test8() {
  printf("test8\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = R""""(
                           module M {
                             proc f(arg: int) {
                               f(arg);
                             }
                             var y: int;
                             f(y);
                           }
                        )"""";

  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);
  const Module* m = vec[0]->toModule();
  assert(m);
  assert(m->numStmts() == 3);
  const Call* call = m->stmt(2)->toCall();
  assert(call);

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  const ResolvedExpression& re = rr.byAst(call);

  assert(re.type().type()->isVoidType());

  auto c = re.mostSpecific().only();
  assert(c);
  assert(c.fn()->untyped()->name() == "f");

  assert(c.fn()->numFormals() == 1);
  assert(c.fn()->formalName(0) == "arg");
  assert(c.fn()->formalType(0).kind() == QualifiedType::CONST_IN);
  assert(c.fn()->formalType(0).type() == IntType::get(context, 64));
}

// check a generic recursive function
static void test9() {
  printf("test9\n");
  Context ctx;
  Context* context = &ctx;

  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = R""""(
                           module M {
                             proc f(arg) {
                               f(arg);
                             }
                             var y: int;
                             f(y);
                           }
                        )"""";

  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);
  const Module* m = vec[0]->toModule();
  assert(m);
  assert(m->numStmts() == 3);
  const Call* call = m->stmt(2)->toCall();
  assert(call);

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  const ResolvedExpression& re = rr.byAst(call);

  assert(re.type().type()->isVoidType());

  auto c = re.mostSpecific().only();
  assert(c);
  assert(c.fn()->untyped()->name() == "f");

  assert(c.fn()->numFormals() == 1);
  assert(c.fn()->formalName(0) == "arg");
  assert(c.fn()->formalType(0).kind() == QualifiedType::CONST_IN);
  assert(c.fn()->formalType(0).type() == IntType::get(context, 64));
}

// Tests 'const ref' formals disallowing coercion, and that this
// error happens after disambiguation.
static void test10() {
  printf("test10\n");
  Context ctx;
  Context* context = &ctx;
  ErrorGuard guard(context);

  auto path = UniqueString::get(context, "input.chpl");
  std::string contents = R""""(
                           module M {
                             class Parent { }
                             class Child : Parent { }

                             /* Both functions should be considered, one
                                should be picked (numeric, since we prefer
                                instantiating), and this function should be
                                rejected. */
                             proc f(const ref arg: Parent, x: int(8)) { }
                             proc f(const ref arg: Parent, x: numeric) { }

                             var x: owned Child;
                             var sixtyFourBits: int = 0;
                             f(x, sixtyFourBits);
                          }
                        )"""";

  setFileText(context, path, contents);

  const ModuleVec& vec = parseToplevel(context, path);
  assert(vec.size() == 1);
  const Module* m = vec[0]->toModule();
  assert(m);
  assert(m->numStmts() == 8);
  const Call* call = m->stmt(7)->toCall();
  assert(call);

  const ResolutionResultByPostorderID& rr = resolveModule(context, m->id());
  const ResolvedExpression& re = rr.byAst(call);

  assert(re.type().type()->isErroneousType());
  assert(guard.numErrors() == 1);
  assert(guard.error(0)->type() == chpl::ConstRefCoercion);
  guard.realizeErrors();
}

// Test transmutation primitives (for params, currently only real(64) -> uint(64)
// is possible since there's no way to get other params of these types.

static void test11() {
  printf("test11\n");
  Context ctx;
  Context* context = &ctx;
  ErrorGuard guard(context);

  std::string contents = R""""(
                          module M {
                            var real32v: real(32);
                            var real64v: real(64);
                            var uint32v: uint(32);
                            var uint64v: uint(64);

                            param x =
                              __primitive("real32 as uint32", real32v).type == uint(32) &&
                              __primitive("real64 as uint64", real64v).type == uint(64) &&
                              __primitive("uint32 as real32", uint32v).type == real(32) &&
                              __primitive("uint64 as real64", uint64v).type == real(64);
                          }
                        )"""";

  auto type = resolveTypeOfXInit(context, contents, /* requireKnown */ true);
  assert(guard.realizeErrors() == 0);
  assert(type.isParamTrue());
}

static void test12() {
  printf("test11\n");
  Context ctx;
  Context* context = &ctx;
  ErrorGuard guard(context);

  std::string contents = R""""(
                          module M {
                            param real64p = 1.0;
                            param x = __primitive("real64 as uint64", real64p);
                          }
                        )"""";

  auto type = resolveTypeOfXInit(context, contents, /* requireKnown */ true);
  assert(guard.realizeErrors() == 0);
  assert(type.isParam() && type.type()->isUintType());
  assert(type.param() && type.param()->isUintParam());
  assert(type.param()->toUintParam()->value() == 4607182418800017408);
}

static void test14() {
  Context context;
  // Make sure no errors make it to the user, even though we will get errors.
  ErrorGuard guard(&context);
  auto variables = resolveTypesOfVariablesInit(&context,
      R"""(
      param xp = 42;
      var xv = 42;
      const xcv = 42;
      param yp = "hello";
      var yv = "hello";
      const ycv = "hello";

      var r1 = __primitive("addr of", xp);
      var r2 = __primitive("addr of", xv);
      var r3 = __primitive("addr of", xcv);
      var r4 = __primitive("addr of", yp);
      var r5 = __primitive("addr of", yv);
      var r6 = __primitive("addr of", ycv);
      var r7 = __primitive("addr of", int);
      )""", { "r1", "r2", "r3", "r4", "r5", "r6", "r7" });

  auto refInt = QualifiedType(QualifiedType::REF, IntType::get(&context, 0));
  auto constRefInt = QualifiedType(QualifiedType::CONST_REF, IntType::get(&context, 0));
  auto refStr = QualifiedType(QualifiedType::REF, RecordType::getStringType(&context));
  auto constRefStr = QualifiedType(QualifiedType::CONST_REF, RecordType::getStringType(&context));

  assert(variables.at("r1") == constRefInt);
  assert(variables.at("r2") == refInt);
  assert(variables.at("r3") == constRefInt);
  assert(variables.at("r4") == constRefStr);
  assert(variables.at("r5") == refStr);
  assert(variables.at("r6") == constRefStr);
  assert(variables.at("r7").isErroneousType());

  // One error for the invalid call of "addr of" with type.
  assert(guard.realizeErrors() == 1);
}

static void test15() {
  Context context;
  // Make sure no errors make it to the user, even though we will get errors.
  ErrorGuard guard(&context);
  auto variables = resolveTypesOfVariablesInit(&context,
      R"""(
      record R {}

      var r: R;
      var x = 42;

      type t0 = __primitive("typeof", int);
      type t1 = __primitive("typeof", r);
      type t2 = __primitive("typeof", x);
      type t3 = __primitive("typeof", 42);
      type t4 = __primitive("typeof", (r, r));

      type t5 = __primitive("static typeof", int);
      type t6 = __primitive("static typeof", r);
      type t7 = __primitive("static typeof", x);
      type t8 = __primitive("static typeof", 42);
      type t9 = __primitive("static typeof", (r, r));
      )""", { "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "t9"});

  for (auto& pair : variables) {
    if (!pair.second.isErroneousType()) {
      assert(pair.second.isType());
      assert(pair.second.type() != nullptr);
    }
  }

  assert(variables.at("t0").isErroneousType());
  assert(variables.at("t1").type()->isRecordType());
  assert(variables.at("t2").type()->isIntType());
  assert(variables.at("t3").type()->isIntType());
  auto tupQt1 = variables.at("t4");
  auto tupType1 = tupQt1.type()->toTupleType();
  assert(tupType1);
  assert(tupType1->numElements() == 2);
  assert(tupType1->elementType(0).kind() == QualifiedType::VAR);
  assert(tupType1->elementType(0).type()->isRecordType());
  assert(tupType1->elementType(1).kind() == QualifiedType::VAR);
  assert(tupType1->elementType(1).type()->isRecordType());

  assert(variables.at("t5").type()->isIntType());
  assert(variables.at("t6").type()->isRecordType());
  assert(variables.at("t7").type()->isIntType());
  assert(variables.at("t8").type()->isIntType());
  auto tupQt2 = variables.at("t9");
  auto tupType2 = tupQt2.type()->toTupleType();
  assert(tupType2);
  assert(tupType2->numElements() == 2);
  assert(tupType2->elementType(0).kind() == QualifiedType::VAR);
  assert(tupType2->elementType(0).type()->isRecordType());
  assert(tupType2->elementType(1).kind() == QualifiedType::VAR);
  assert(tupType2->elementType(1).type()->isRecordType());

  // One error for the invalid call of "typeof" with type.
  assert(guard.realizeErrors() == 1);
}

static void test16() {
  Context context;
  // Make sure no errors make it to the user, even though we will get errors.
  ErrorGuard guard(&context);
  auto variables = resolveTypesOfVariablesInit(&context,
      R"""(
      record Concrete {
          var x: int;
          var y: string;
          var z: (int, string);
      };

      record Generic {
          var x;
          var y;
          var z;
      }

      var conc: Concrete;
      var inst: Generic(int, string, (int, string));

      param r1 = __primitive("static field type", conc, "x") == int;
      param r2 = __primitive("static field type", conc, "y") == string;
      param r3 = __primitive("static field type", conc, "z") == (int, string);
      param r4 = __primitive("static field type", inst, "x") == int;
      param r5 = __primitive("static field type", inst, "y") == string;
      param r6 = __primitive("static field type", inst, "z") == (int, string);
      )""", { "r1", "r2", "r3", "r4", "r5", "r6"});

  for (auto& pair : variables) {
    pair.second.isParamTrue();
  }

}

int main() {
  test1();
  test2();
  test3();
  test4();
  test5();
  test6();
  test7();
  test8();
  test9();
  test10();
  test11();
  test12();
  test14();
  test15();
  test16();

  return 0;
}
