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

#include "core-types.h"
#include "chpl/uast/all-uast.h"
#include "chpl/parsing/parsing-queries.h"
#include "python-types.h"

using namespace chpl;
using namespace uast;

static PyMethodDef ContextObject_methods[] = {
  { "parse", (PyCFunction) ContextObject_parse, METH_VARARGS, "Parse a top-level AST node from the given file" },
  { "is_bundled_path", (PyCFunction) ContextObject_is_bundled_path, METH_VARARGS, "Check if the given file path is within the bundled (built-in) Chapel files" },
  { "advance_to_next_revision", (PyCFunction) ContextObject_advance_to_next_revision, METH_VARARGS, "Advance the context to the next revision" },
  { "get_pyi_file", (PyCFunction) ContextObject_get_pyi_file, METH_NOARGS, "Generate a stub file for the Chapel AST nodes" },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};

PyTypeObject ContextType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "Context",
  .tp_basicsize = sizeof(ContextObject),
  .tp_itemsize = 0,
  .tp_dealloc = (destructor) ContextObject_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = PyDoc_STR("The Chapel context object that tracks various frontend state"),
  .tp_methods = ContextObject_methods,
  .tp_init = (initproc) ContextObject_init,
  .tp_new = PyType_GenericNew,
};

int ContextObject_init(ContextObject* self, PyObject* args, PyObject* kwargs) {
  chpl::Context::Configuration config;
  config.chplHome = getenv("CHPL_HOME");
  new (&self->context) chpl::Context(std::move(config));
  self->context.installErrorHandler(chpl::owned<PythonErrorHandler>(new PythonErrorHandler((PyObject*) self)));

  // Disable setting up standard search paths since for some reason this breaks
  // re-running incrementally.
  //
  // chpl::parsing::setupModuleSearchPaths(&self->context, false, false, {}, {});

  return 0;
}

void ContextObject_dealloc(ContextObject* self) {
  self->context.~Context();
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject* ContextObject_parse(ContextObject *self, PyObject* args) {
  auto context = &self->context;
  const char* fileName;
  if (!PyArg_ParseTuple(args, "s", &fileName)) {
    PyErr_BadArgument();
    return nullptr;
  }
  auto fileNameUS = chpl::UniqueString::get(context, fileName);
  auto parentPathUS = chpl::UniqueString();
  auto& builderResult = chpl::parsing::parseFileToBuilderResult(context, fileNameUS, parentPathUS);

  int listSize = builderResult.numTopLevelExpressions();
  PyObject* topExprs = PyList_New(listSize);
  for (auto i = 0; i < listSize; i++) {
    PyObject* node = wrapAstNode(self, builderResult.topLevelExpression(i));
    PyList_SetItem(topExprs, i, node);
  }
  return topExprs;
}

PyObject* ContextObject_is_bundled_path(ContextObject *self, PyObject* args) {
  auto context = &self->context;
  const char* fileName;
  if (!PyArg_ParseTuple(args, "s", &fileName)) {
    PyErr_BadArgument();
    return nullptr;
  }
  auto pathUS = chpl::UniqueString::get(context, fileName);

  bool isInternalPath =
    chpl::parsing::filePathIsInInternalModule(context, pathUS) ||
    chpl::parsing::filePathIsInStandardModule(context, pathUS) ||
    chpl::parsing::filePathIsInBundledModule(context, pathUS);

  return PyBool_FromLong(isInternalPath);
}

PyObject* ContextObject_advance_to_next_revision(ContextObject *self, PyObject* args) {
  auto context = &self->context;
  bool prepareToGc;
  if (!PyArg_ParseTuple(args, "b", &prepareToGc)) {
    PyErr_BadArgument();
    return nullptr;
  }

  context->advanceToNextRevision(prepareToGc);
  Py_RETURN_NONE;
}

template <typename Tuple, size_t ... Indices>
static void printTypedPythonFunctionArgs(std::ostringstream& ss, std::index_sequence<Indices...>) {
  int counter = 0;
  auto printArg = [&](const char* arg) {
    ss << ", arg" << counter++ << ": " << arg;
  };

  int dummy[] = { (printArg(std::tuple_element<Indices, Tuple>::type::TypeString), 0)...};
  (void) dummy;
}

static const char* tagToUserFacingStringTable[chpl::uast::asttags::NUM_AST_TAGS] = {
// define tag to string conversion
#define AST_NODE(NAME) #NAME,
#define AST_LEAF(NAME) #NAME,
#define AST_BEGIN_SUBCLASSES(NAME) #NAME,
#define AST_END_SUBCLASSES(NAME) #NAME,
// Apply the above macros to uast-classes-list.h
#include "chpl/uast/uast-classes-list.h"
// clear the macros
#undef AST_NODE
#undef AST_LEAF
#undef AST_BEGIN_SUBCLASSES
#undef AST_END_SUBCLASSES
#undef NAMESTR
};

PyObject* ContextObject_get_pyi_file(ContextObject *self, PyObject* args) {
  std::ostringstream ss;

  ss << "from typing import *" << std::endl << std::endl;

  ss << "class AstNode:" << std::endl;
  ss << "    pass" << std::endl << std::endl;

  using namespace chpl;
  using namespace uast;
  #define CLASS_BEGIN(NODE) \
    ss << "class " << tagToUserFacingStringTable[asttags::NODE] << "(AstNode):" << std::endl;
  #define METHOD(NODE, NAME, DOCSTR, TYPEFN, BODY) \
    ss << "    def " << #NAME << "(self"; \
    printTypedPythonFunctionArgs<PythonFnHelper<TYPEFN>::ArgTypeInfo>(ss, std::make_index_sequence<std::tuple_size<PythonFnHelper<TYPEFN>::ArgTypeInfo>::value>()); \
    ss << ") -> " << PythonFnHelper<TYPEFN>::ReturnTypeInfo::TypeString << ":" << std::endl;\
    ss << "        \"\"\"" << std::endl; \
    ss << "        " << DOCSTR << std::endl; \
    ss << "        \"\"\"" << std::endl; \
    ss << "        ..." << std::endl << std::endl;
  #define CLASS_END(NODE) \
    ss << std::endl;
  #include "method-tables.h"

  return Py_BuildValue("s", ss.str().c_str());
}

static PyMethodDef LocationObject_methods[] = {
  { "start", (PyCFunction) LocationObject_start, METH_VARARGS, "Get the start of a Location object" },
  { "end", (PyCFunction) LocationObject_end, METH_VARARGS, "Get the end of a Location object" },
  { "path", (PyCFunction) LocationObject_path, METH_VARARGS, "Get the path of a Location object" },
  {NULL, NULL, 0, NULL}  /* Sentinel */
};

PyTypeObject LocationType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "Location",
  .tp_basicsize = sizeof(LocationObject),
  .tp_itemsize = 0,
  .tp_dealloc = (destructor) LocationObject_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_doc = PyDoc_STR("The Chapel context object that tracks various frontend state"),
  .tp_methods = LocationObject_methods,
  .tp_init = (initproc) LocationObject_init,
  .tp_new = PyType_GenericNew,
};

int LocationObject_init(LocationObject* self, PyObject* args, PyObject* kwargs) {
  new (&self->location) chpl::Location();
  return 0;
}

void LocationObject_dealloc(LocationObject* self) {
  self->location.~Location();
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject* LocationObject_start(LocationObject *self, PyObject* Py_UNUSED(args)) {
  auto& location = self->location;
  return Py_BuildValue("ii", location.firstLine(), location.firstColumn());
}

PyObject* LocationObject_end(LocationObject *self, PyObject* Py_UNUSED(args)) {
  auto& location = self->location;
  return Py_BuildValue("ii", location.lastLine(), location.lastColumn());
}

PyObject* LocationObject_path(LocationObject *self, PyObject* Py_UNUSED(args)) {
  return Py_BuildValue("s", self->location.path().c_str());
}

static PyMethodDef AstNodeObject_methods[] = {
  {"dump", (PyCFunction) AstNodeObject_dump, METH_NOARGS, "Dump the internal representation of the given AST node"},
  {"tag", (PyCFunction) AstNodeObject_tag, METH_NOARGS, "Get a string representation of the AST node's type"},
  {"attribute_group", (PyCFunction) AstNodeObject_attribute_group, METH_NOARGS, "Get the attribute group, if any, associated with this node"},
  {"location", (PyCFunction) AstNodeObject_location, METH_NOARGS, "Get the location of this AST node in its file"},
  {"parent", (PyCFunction) AstNodeObject_parent, METH_NOARGS, "Get the parent node of this AST node"},
  {"pragmas", (PyCFunction) AstNodeObject_pragmas, METH_NOARGS, "Get the pragmas of this AST node"},
  {"unique_id", (PyCFunction) AstNodeObject_unique_id, METH_NOARGS, "Get a unique identifer for this AST node"},
  {NULL, NULL, 0, NULL} /* Sentinel */
};

PyTypeObject AstNodeType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "AstNode",
  .tp_basicsize = sizeof(AstNodeObject),
  .tp_itemsize = 0,
  .tp_dealloc = (destructor) AstNodeObject_dealloc,
  .tp_flags = Py_TPFLAGS_BASETYPE,
  .tp_doc = PyDoc_STR("The base type of Chapel AST nodes"),
  .tp_iter = (getiterfunc) AstNodeObject_iter,
  .tp_methods = AstNodeObject_methods,
  .tp_init = (initproc) AstNodeObject_init,
  .tp_new = PyType_GenericNew,
};

int AstNodeObject_init(AstNodeObject* self, PyObject* args, PyObject* kwargs) {
  PyObject* contextObjectPy;
  if (!PyArg_ParseTuple(args, "O", &contextObjectPy))
      return -1;

  Py_INCREF(contextObjectPy);
  self->astNode = nullptr;
  self->contextObject = contextObjectPy;
  return 0;
}

void AstNodeObject_dealloc(AstNodeObject* self) {
  Py_XDECREF(self->contextObject);
  Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject* AstNodeObject_dump(AstNodeObject *self, PyObject *Py_UNUSED(ignored)) {
  self->astNode->dump();
  Py_RETURN_NONE;
}

PyObject* AstNodeObject_tag(AstNodeObject *self, PyObject *Py_UNUSED(ignored)) {
  const char* nodeType = chpl::uast::asttags::tagToString(self->astNode->tag());
  return Py_BuildValue("s", nodeType);
}

PyObject* AstNodeObject_unique_id(AstNodeObject *self, PyObject *Py_UNUSED(ignored)) {
  auto uniqueID = (intptr_t)self->astNode;
  return Py_BuildValue("K", uniqueID);
}


PyObject* AstNodeObject_attribute_group(AstNodeObject *self, PyObject *Py_UNUSED(ignored)) {
  return wrapAstNode((ContextObject*) self->contextObject,
                     self->astNode->attributeGroup());
}

PyObject* AstNodeObject_pragmas(AstNodeObject *self, PyObject *Py_UNUSED(ignored)) {
  PyObject* elms = PySet_New(NULL);
  auto attrs = self->astNode->attributeGroup();
  if (attrs) {
    for (auto p: attrs->pragmas()) {
      PyObject* s = Py_BuildValue("s", chpl::uast::pragmatags::pragmaTagToName(p));
      PySet_Add(elms, s);
    }
  }
  return elms;
}

PyObject* AstNodeObject_parent(AstNodeObject* self, PyObject *Py_UNUSED(ignored)) {
  auto contextObject = (ContextObject*) self->contextObject;
  auto context = &contextObject->context;

  return wrapAstNode(contextObject, chpl::parsing::parentAst(context, self->astNode));
}

PyObject* AstNodeObject_iter(AstNodeObject *self) {
  return wrapIterPair((ContextObject*) self->contextObject, self->astNode->children());
}

PyObject* AstNodeObject_location(AstNodeObject *self) {
  auto locationObjectPy = PyObject_CallObject((PyObject *) &LocationType, nullptr);
  auto& location = ((LocationObject*) locationObjectPy)->location;
  auto context = &((ContextObject*) self->contextObject)->context;

  location = chpl::parsing::locateAst(context, self->astNode);
  return locationObjectPy;
}


PyTypeObject* parentTypeFor(chpl::uast::asttags::AstTag tag) {
#define AST_NODE(NAME)
#define AST_LEAF(NAME)
#define AST_BEGIN_SUBCLASSES(NAME)
#define AST_END_SUBCLASSES(NAME) \
  if (tag > chpl::uast::asttags::START_##NAME && tag < chpl::uast::asttags::END_##NAME) { \
    return &NAME##Type; \
  }
#include "chpl/uast/uast-classes-list.h"
#include "chpl/uast/uast-classes-list.h"
#undef AST_NODE
#undef AST_LEAF
#undef AST_BEGIN_SUBCLASSES
#undef AST_END_SUBCLASSES
  return &AstNodeType;
}

PyObject* wrapAstNode(ContextObject* context, const chpl::uast::AstNode* node) {
  PyObject* toReturn = nullptr;
  if (node == nullptr) {
    Py_RETURN_NONE;
  }
  PyObject* args = Py_BuildValue("(O)", (PyObject*) context);
  switch (node->tag()) {
#define CAST_TO(NAME) \
    case chpl::uast::asttags::NAME: \
      toReturn = PyObject_CallObject((PyObject*) &NAME##Type, args); \
      ((NAME##Object*) toReturn)->parent.astNode = node->to##NAME(); \
      break;
#define AST_NODE(NAME) CAST_TO(NAME)
#define AST_LEAF(NAME) CAST_TO(NAME)
#define AST_BEGIN_SUBCLASSES(NAME) /* No need to handle abstract parent classes. */
#define AST_END_SUBCLASSES(NAME)
#include "chpl/uast/uast-classes-list.h"
#undef AST_NODE
#undef AST_LEAF
#undef AST_BEGIN_SUBCLASSES
#undef AST_END_SUBCLASSES
    default: break;
  }
  Py_XDECREF(args);
  return toReturn;
}
