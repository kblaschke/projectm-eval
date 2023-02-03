%code requires {

#include "CompilerFunctions.h"

#include <stdio.h>

typedef void* yyscan_t;
}

/* Generator options */
%locations
%no-lines
%define api.pure true
%define api.prefix {prjm_eel_}
%define api.value.type union /* Generate YYSTYPE from these types: */
%define parse.error verbose

    /* Parser and lexer arguments */
%param {prjm_eel_compiler_context_t* cctx} { yyscan_t scanner }

%code provides {

    #define YYSTYPE PRJM_EEL_STYPE
    #define YYLTYPE PRJM_EEL_LTYPE

    #define YYERROR_VERBOSE

   #define YY_DECL \
       int yylex(YYSTYPE* yylval_param, YYLTYPE* yylloc_param, prjm_eel_compiler_context_t* cctx, yyscan_t yyscanner)
   YY_DECL;

   int yyerror(YYLTYPE* yyllocp, prjm_eel_compiler_context_t* cctx, yyscan_t yyscanner, const char* message);
}

/* Token declarations */

/* Special functions */
%token GMEM

/* Operator tokens */
%token ADDOP SUBOP MODOP OROP ANDOP DIVOP MULOP POWOP EQUAL BELEQ ABOEQ NOTEQ BOOLOR BOOLAND

/* Value types */
%token <PRJM_EEL_F> NUM
%token <char*> VAR FUNC
%nterm <prjm_eel_compiler_node_t*> function program instruction-list expression parentheses
%nterm <prjm_eel_compiler_arg_list_t*> function-arglist

/* Operator precedence, lowest first, highest last. */
%precedence ','
%right '='
%right ADDOP SUBOP
%right MULOP DIVOP MODOP
%right POWOP OROP ANDOP
%right '?' ':'
%left BOOLOR
%left BOOLAND
%left '|'
/* XOR would go here, but Milkdrop defines ^ as pow() and provides no XOR operator or function. */
%left '&'
%left EQUAL NOTEQ
%left '>' ABOEQ
%left '<' BELEQ
%left '-' '+'
%left '*' '/' '%'
%right '!'
%precedence NEG POS /* unary minus or plus */
%left '^'
%left '[' ']'
%left '(' ')'

%% /* The grammar follows. */

program:
  instruction-list[topnode] {
        if($topnode) {
            cctx->compile_result = $topnode->tree_node;
        };
        free($topnode);
    }
;

/* Functions */
function:
  FUNC[name] '(' function-arglist[args] ')' { PRJM_EEL_FUNC($$, $name, $args); free($name); }
;

function-arglist:
  instruction-list[instr]                            { $$ = prjm_eel_compiler_add_argument(NULL, $instr); }
| function-arglist[args] ',' instruction-list[instr] { $$ = prjm_eel_compiler_add_argument($args, $instr); }
;

parentheses:
  '(' instruction-list[instr] ')' { $$ = $instr; }
;

instruction-list:
  expression[expr]                            { $$ = $expr; }
| instruction-list[list] ';' expression[expr] { $$ = prjm_eel_compiler_add_instruction(cctx, $list, $expr); }
| instruction-list[list] ';' empty-expression { $$ = $list; }
;

empty-expression:
  %empty
| '(' ')'
;

/* General expressions */
expression:
/* Literals */
  NUM[val]                                     { $$ = prjm_eel_compiler_create_constant(cctx, $val); }
| VAR[name] { $$ = prjm_eel_compiler_create_variable(cctx, $name); free($name); }

/* Memory access via index */
| GMEM '[' ']'                               { prjm_eel_compiler_node_t* gmem_zero_idx =  prjm_eel_compiler_create_constant(cctx, .0);
                                               PRJM_EEL_FUNC1($$, "_gmem", gmem_zero_idx)
                                               }
| GMEM '[' expression[idx] ']'               { PRJM_EEL_FUNC1($$, "_gmem", $idx) }
| expression[idx] '[' ']'                    { PRJM_EEL_FUNC1($$, "_mem", $idx) }
| expression[idx] '[' expression[offs] ']'   { /* Create additional "idx + offs" operation as arg to _mem */
                                               prjm_eel_compiler_node_t* idx_plus_offset;
                                               PRJM_EEL_FUNC2(idx_plus_offset, "_add", $idx, $offs)
                                               PRJM_EEL_FUNC1($$, "_mem", idx_plus_offset)
                                               }

/* Compund assignment operators */
| expression[left] ADDOP expression[right]         { PRJM_EEL_FUNC2($$, "_addop", $left, $right) }
| expression[left] SUBOP expression[right]         { PRJM_EEL_FUNC2($$, "_subop", $left, $right) }
| expression[left] MODOP expression[right]         { PRJM_EEL_FUNC2($$, "_modop", $left, $right) }
| expression[left] OROP expression[right]          { PRJM_EEL_FUNC2($$, "_orop", $left, $right) }
| expression[left] ANDOP expression[right]         { PRJM_EEL_FUNC2($$, "_andop", $left, $right) }
| expression[left] DIVOP expression[right]         { PRJM_EEL_FUNC2($$, "_divop", $left, $right) }
| expression[left] MULOP expression[right]         { PRJM_EEL_FUNC2($$, "_mulop", $left, $right) }
| expression[left] POWOP expression[right]         { PRJM_EEL_FUNC2($$, "_powop", $left, $right) }

/* Comparison operators */
| expression[left] EQUAL expression[right]     { PRJM_EEL_FUNC2($$, "_equal", $left, $right) }
| expression[left] BELEQ expression[right]     { PRJM_EEL_FUNC2($$, "_beleq", $left, $right) }
| expression[left] ABOEQ expression[right]     { PRJM_EEL_FUNC2($$, "_aboeq", $left, $right) }
| expression[left] NOTEQ expression[right]     { PRJM_EEL_FUNC2($$, "_noteq", $left, $right) }
| expression[left] '<' expression[right]       { PRJM_EEL_FUNC2($$, "_below", $left, $right) }
| expression[left] '>' expression[right]       { PRJM_EEL_FUNC2($$, "_above", $left, $right) }

/* Boolean operators */
| expression[left] BOOLOR expression[right]    { PRJM_EEL_FUNC2($$, "bor", $left, $right) }
| expression[left] BOOLAND expression[right]   { PRJM_EEL_FUNC2($$, "band", $left, $right) }

/* Assignment operator */
| expression[left] '=' expression[right]           { PRJM_EEL_FUNC2($$, "_set", $left, $right) }

/* Function call */
| function                        { $$ = $1; }

/* Ternary operator */
| expression[cond] '?' expression[trueval] ':' expression[falseval]   { PRJM_EEL_FUNC3($$, "_if", $cond, $trueval, $falseval) }

/* Binary operators */
| expression[left] '+' expression[right] { PRJM_EEL_FUNC2($$, "_add", $left, $right) }
| expression[left] '-' expression[right] { PRJM_EEL_FUNC2($$, "_sub", $left, $right) }
| expression[left] '*' expression[right] { PRJM_EEL_FUNC2($$, "_mul", $left, $right) }
| expression[left] '/' expression[right] { PRJM_EEL_FUNC2($$, "_div", $left, $right) }
| expression[left] '%' expression[right] { PRJM_EEL_FUNC2($$, "_mod", $left, $right) }
| expression[left] '^' expression[right] { PRJM_EEL_FUNC2($$, "pow", $left, $right) }
| expression[left] '|' expression[right] { PRJM_EEL_FUNC2($$, "bor", $left, $right) }
| expression[left] '&' expression[right] { PRJM_EEL_FUNC2($$, "band", $left, $right) }

/* Unary operators */
| '-' expression[value] %prec NEG        { PRJM_EEL_FUNC1($$, "_neg", $value) }
| '+' expression[value] %prec POS        { $$ = $value; } /* a + prefix does nothing. */
| '!' expression[value]                  { PRJM_EEL_FUNC1($$, "_not", $value) }

/* Parenthesed expression */
| parentheses[value]                      { $$ = $value; }
;

/* End of grammar. */
%%