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
#include "chpl/resolution/can-pass.h"
#include "chpl/types/all-types.h"
#include "chpl/uast/Identifier.h"
#include "chpl/uast/Module.h"
#include "chpl/uast/Record.h"
#include "chpl/uast/Variable.h"

static constexpr bool testType = true;
static constexpr bool testExact = true;

static void testPrimitive(std::string primitive, std::vector<std::tuple<const char*, const char*, bool>> args) {
  Context ctx;
  auto context = &ctx;
  ErrorGuard guard(context);

  std::stringstream ps;
  std::vector<std::string> variables;

  ps << "class C {}" << std::endl;

  int counter = 0;
  for (auto triple : args) {
    const char* expr = std::get<0>(triple);
    const char* expectedType = std::get<1>(triple);
    bool callDotType = std::get<2>(triple);

    std::string variableName = std::string("x") + std::to_string(counter++);
    variables.push_back(variableName);

    ps << "param " << variableName << " = " << "__primitive(\"" << primitive << "\", " << expr << ")";
    if (callDotType) ps << ".type ";
    ps << "== " << expectedType << ";" << std::endl;
  }

  std::cout << "--- program ---" << std::endl;
  std::cout << ps.str() << std::endl;

  auto varTypes = resolveTypesOfVariables(context, ps.str(), variables);

  for (auto type : varTypes) {
    assert(type.second.isParamTrue());
  }
}

static void test1() {
  testPrimitive("to nilable class", {
    { "new owned C()", "owned C?", testType },
    { "new owned C?()", "owned C?", testType },
    { "new shared C()", "shared C?", testType },
    { "new shared C?()", "shared C?", testType },
    { "new unmanaged C()", "unmanaged C?", testType },
    { "new unmanaged C?()", "unmanaged C?", testType },

    { "owned class", "owned class?", testExact },
    { "owned class", "owned class?", testExact },
    { "shared class", "shared class?", testExact },
    { "shared class", "shared class?", testExact },
    { "unmanaged class", "unmanaged class?", testExact },
    { "unmanaged class", "unmanaged class?", testExact },
  });
}

static void test2() {
  testPrimitive("to borrowed class", {
    { "new shared C()", "borrowed C", testType },
    { "new shared C?()", "borrowed C?", testType },
    { "new owned C()", "borrowed C" , testType },
    { "new owned C?()", "borrowed C?", testType },
    { "new unmanaged C()", "borrowed C", testType },
    { "new unmanaged C?()", "borrowed C?", testType },
  });
}

static void test3() {
  testPrimitive("to unmanaged class", {
    { "new shared C()", "unmanaged C", testType },
    { "new shared C?()", "unmanaged C?", testType },
    { "new owned C()", "unmanaged C" , testType },
    { "new owned C?()", "unmanaged C?", testType },
    { "new unmanaged C()", "unmanaged C", testType },
    { "new unmanaged C?()", "unmanaged C?", testType },
  });
}

int main() {
  test1();
  test2();
  test3();
}
