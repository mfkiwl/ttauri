%define api.pure full
%define parse.lac full
%define parse.error verbose
%locations
%param { yyscan_t scanner }
%param { TTauri::Config::ParseContext* context }

%code top {
  #include <stdio.h>
  #define YYPRINT(a, b, c)

  #define NEW_NODE(ast_type, location, ...) new TTauri::Config::ast_type({location.first_line, location.last_line, location.first_column, location.last_column}, __VA_ARGS__)
  #define NEW_UNARY_OPERATOR(op, location, right) NEW_NODE(ASTUnaryOperator, location, TTauri::Config::ASTUnaryOperator::Type:: ## op, right)
  #define NEW_BINARY_OPERATOR(op, location, left, right) NEW_NODE(ASTBinaryOperator, location, TTauri::Config::ASTBinaryOperator::Type:: ## op, left, right)
} 
%code requires {
  #include <string>
  #include <cstdint>
  #include <memory>
  #include "TTauri/Config/AST.hpp"
  #include "TTauri/Config/ParseContext.hpp"
  typedef void* yyscan_t;
  int TTauriConfig_yyfind_token(char *text);
}

%code {
  int yylex(YYSTYPE* yylvalp, YYLTYPE* yyllocp, yyscan_t scanner, TTauri::Config::ParseContext* context);
  void yyerror(YYLTYPE* yyllocp, yyscan_t scanner, TTauri::Config::ParseContext* context, const char* msg) {
    context->setError({yyllocp->first_line, yyllocp->last_line, yyllocp->first_column, yyllocp->last_column}, msg);
  }
}

%union {
    char *string;
    int64_t integer;
    double real;
    TTauri::Config::ASTExpression *expression;
    TTauri::Config::ASTExpressions *expressions;
    TTauri::Config::ASTObject *object;
    TTauri::Config::ASTArray *array;
}

%token-table
%token <string> T_IDENTIFIER
%token <integer> T_INTEGER
%token <real> T_FLOAT
%token <string> T_STRING

// Same precedence order as C++ operators.
%left ';'
%right '='
%left "or"
%left "xor"
%left "and"
%left '|'
%left '^'
%left '&'
%left "==" "!="
%left '<' '>' "<=" ">="
%left "<<" ">>"
%left '+' '-'
%left '*' '/' '%'
%right '~' "not" UMINUS
%left '(' '['
%left '.'

%type <object> root
%type <expression> expression
%type <object> object
%type <array> array
%type <expressions> expressions
%type <expressions> statements
%start root
%%

array:
      '[' ']'                                                       { $$ = NEW_NODE(ASTArray, @1); }
    | '[' expressions ']'                                           { $$ = NEW_NODE(ASTArray, @1, $2); }
    | '[' expressions ';' ']'                                       { $$ = NEW_NODE(ASTArray, @1, $2); }
    ;

object:
      '{' '}'                                                       { $$ = NEW_NODE(ASTObject, @1); }
    | '{' expressions '}'                                           { $$ = NEW_NODE(ASTObject, @1, $2); }
    | '{' expressions ';' '}'                                       { $$ = NEW_NODE(ASTObject, @1, $2); }
    ;

expression:
      '(' expression ')'                                            { $$ = $2; }
    | object                                                        { $$ = $1; }
    | array                                                         { $$ = $1; }
    | T_INTEGER                                                     { $$ = NEW_NODE(ASTInteger, @1, $1); }
    | T_FLOAT                                                       { $$ = NEW_NODE(ASTFloat, @1, $1); }
    | "true"                                                        { $$ = NEW_NODE(ASTBoolean, @1, true); }
    | "false"                                                       { $$ = NEW_NODE(ASTBoolean, @1, false); }
    | "null"                                                        { $$ = NEW_NODE(ASTNull, @1); }
    | T_STRING                                                      { $$ = NEW_NODE(ASTString, @1, $1); }
    | T_IDENTIFIER                                                  { $$ = NEW_NODE(ASTName, @1, $1); }
    | '~' expression                                                { $$ = NEW_UNARY_OPERATOR(NOT, @1, $2); }
    | '-' expression %prec UMINUS                                   { $$ = NEW_UNARY_OPERATOR(NEG, @1, $2); }
    | "not" expression                                              { $$ = NEW_UNARY_OPERATOR(LOGICAL_NOT, @1, $2); }
    | expression '=' expression                                     { $$ = NEW_NODE(ASTAssignment, @2, $1, $3); }
    | expression '.' T_IDENTIFIER                                   { $$ = NEW_NODE(ASTMember, @2, $1, $3); }
    | expression '*' expression                                     { $$ = NEW_BINARY_OPERATOR(MUL, @2, $1, $3 ); }
    | expression '/' expression                                     { $$ = NEW_BINARY_OPERATOR(DIV, @2, $1, $3 ); }
    | expression '%' expression                                     { $$ = NEW_BINARY_OPERATOR(MOD, @2, $1, $3 ); }
    | expression '+' expression                                     { $$ = NEW_BINARY_OPERATOR(ADD, @2, $1, $3 ); }
    | expression '-' expression                                     { $$ = NEW_BINARY_OPERATOR(SUB, @2, $1, $3 ); }
    | expression "<<" expression                                    { $$ = NEW_BINARY_OPERATOR(SHL, @2, $1, $3 ); }
    | expression ">>" expression                                    { $$ = NEW_BINARY_OPERATOR(SHR, @2, $1, $3 ); }
    | expression '<' expression                                     { $$ = NEW_BINARY_OPERATOR(LT, @2, $1, $3 ); }
    | expression '>' expression                                     { $$ = NEW_BINARY_OPERATOR(GT, @2, $1, $3 ); }
    | expression "<=" expression                                    { $$ = NEW_BINARY_OPERATOR(LE, @2, $1, $3 ); }
    | expression ">=" expression                                    { $$ = NEW_BINARY_OPERATOR(GE, @2, $1, $3 ); }
    | expression "==" expression                                    { $$ = NEW_BINARY_OPERATOR(EQ, @2, $1, $3 ); }
    | expression "!=" expression                                    { $$ = NEW_BINARY_OPERATOR(NE, @2, $1, $3 ); }
    | expression '&' expression                                     { $$ = NEW_BINARY_OPERATOR(AND, @2, $1, $3 ); }
    | expression '^' expression                                     { $$ = NEW_BINARY_OPERATOR(XOR, @2, $1, $3 ); }
    | expression '|' expression                                     { $$ = NEW_BINARY_OPERATOR(OR, @2, $1, $3 ); }
    | expression "and" expression                                   { $$ = NEW_BINARY_OPERATOR(LOGICAL_AND, @2, $1, $3 ); }
    | expression "xor" expression                                   { $$ = NEW_BINARY_OPERATOR(LOGICAL_XOR, @2, $1, $3 ); }
    | expression "or" expression                                    { $$ = NEW_BINARY_OPERATOR(LOGICAL_OR, @2, $1, $3 ); }
    | expression '[' expression ']'                                 { $$ = NEW_NODE(ASTIndex, @2, $1, $3); }
    | expression '(' expressions ')'                                { $$ = NEW_NODE(ASTCall, @2, $1, $3); }
    ;

expressions:
      expression                                                    { $$ = NEW_NODE(ASTExpressions, @1, $1); }
    | expressions ';' expression                                    { $1->expressions.push_back($3); $$ = $1; }
    ;

statements:
      expression ';'                                                { $$ = NEW_NODE(ASTExpressions, @1, $1); }
    | statements expression ';'                                     { $1->expressions.push_back($2); $$ = $1; }
    ;

root:
      /* empty */                                                   { $$ = new TTauri::Config::ASTObject({0,0,0,0}); context->object = $$; }
    | object                                                        { $$ = $1; context->object = $$; };
    | statements                                                    { $$ = NEW_NODE(ASTObject, @1, $1); context->object = $$; }
    ;

%%

int TTauriConfig_yyfind_token(char *text)
{
    size_t size = strlen(text);

    for (size_t symbolNumber = 0; symbolNumber < YYNTOKENS; symbolNumber++) {
        if (
            (yytname[symbolNumber] != 0) &&
            (yytname[symbolNumber][0] == '"') &&
            (strncmp (yytname[symbolNumber] + 1, text, size) == 0) &&
            (yytname[symbolNumber][size + 1] == '"') &&
            (yytname[symbolNumber][size + 2] == 0)
        ) {
            return yytoknum[symbolNumber];
        }
    }
    return yytoknum[1]; // "error" is the second entry in yytname.
}