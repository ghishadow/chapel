#ifndef _BASEAST_H_
#define _BASEAST_H_

#include "link.h"

/**
 **  Note: update astType and astName together always.
 **/
enum astType_t {
  STMT,
  STMT_NOOP,
  STMT_WITH,
  STMT_DEF,
  STMT_EXPR,
  STMT_RETURN,
  STMT_BLOCK,
  STMT_WHILELOOP,
  STMT_FORLOOP,
  STMT_COND,
  STMT_LABEL,
  STMT_GOTO,

  EXPR,
  EXPR_LITERAL,
  EXPR_BOOLLITERAL,
  EXPR_INTLITERAL,
  EXPR_FLOATLITERAL,
  EXPR_COMPLEXLITERAL,
  EXPR_STRINGLITERAL,
  EXPR_VARIABLE,
  EXPR_UNOP,
  EXPR_BINOP,
  EXPR_SPECIALBINOP,
  EXPR_ASSIGNOP,
  EXPR_SIMPLESEQ,
  EXPR_FLOOD,
  EXPR_COMPLETEDIM,
  EXPR_LET,
  EXPR_FORALL,
  EXPR_SIZEOF,
  EXPR_PARENOP,
  EXPR_CAST,
  EXPR_FNCALL,
  EXPR_IOCALL,
  EXPR_ARRAYREF,
  EXPR_TUPLESELECT,
  EXPR_MEMBERACCESS,
  EXPR_REDUCE,
  EXPR_TUPLE,
  EXPR_NAMED,

  SYMBOL,
  SYMBOL_UNRESOLVED,
  SYMBOL_MODULE,
  SYMBOL_VAR,
  SYMBOL_PARAM,
  SYMBOL_TYPE,
  SYMBOL_FN,
  SYMBOL_ENUM,
  SYMBOL_LABEL,

  TYPE,
  TYPE_BUILTIN,
  TYPE_ENUM,
  TYPE_DOMAIN,
  TYPE_INDEX,
  TYPE_ARRAY,
  TYPE_USER,
  TYPE_CLASS,
  TYPE_TUPLE,
  TYPE_SUM,
  TYPE_VARIABLE,
  TYPE_UNRESOLVED,

  AST_TYPE_END 
};

extern char* astTypeName[];

#define isSomeStmt(_x) (((_x) >= STMT) && (_x) < EXPR)
#define isSomeExpr(_x) (((_x) >= EXPR) && (_x) < SYMBOL)
#define isSomeSymbol(_x) (((_x) >= SYMBOL) && (_x) < TYPE)
#define isSomeType(_x) (((_x) >= TYPE) && (_x) < AST_TYPE_END)

#define SET_BACK(ast) \
  if (ast) (ast)->back = &(ast)

class BaseAST : public ILink {
 public:
  astType_t astType;
  long id;

  static long getNumIDs(void);

  BaseAST(void);
  BaseAST(astType_t type);
};
#define forv_BaseAST(_p, _v) forv_Vec(BaseAST, _p, _v)

class FnSymbol;

void collect_symbols(Vec<Symbol*>* syms);
void collect_symbols(Vec<Symbol*>* syms, FnSymbol* function);
// USAGE:
//   Vec<Symbol*> all_syms;
//   collect_symbols(&all_syms);
//   FnSymbol* function = ...;
//   collect_symbols(&all_syms, function);

void collect_functions(Vec<FnSymbol*>* functions);
// USAGE:
//   Vec<FnSymbol*> all_functions;
//   collect_functions(&all_functions);

void collect_asts(Vec<BaseAST*>* asts, FnSymbol* function);
// USAGE:
//   Vec<BaseAST*> all_asts;
//   FnSymbol* function = ...;
//   collect_asts(&all_asts, function);

int compar_baseast(const void *ai, const void *aj);
// for use with qsort

#endif

