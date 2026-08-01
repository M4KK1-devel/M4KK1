/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 156 "./cgram.y"

# include "pass1.h"
# include <stdarg.h>
# include <string.h>
# include <stdlib.h>

int fun_inline;	/* Reading an inline function */
int oldstyle;	/* Current function being defined */
static struct symtab *xnf;
extern int enummer, tvaloff, inattr;
extern struct rstack *rpole;
static int widestr, alwinl;
NODE *cftnod;
static int attrwarn = 1;

#define	NORETYP	SNOCREAT /* no return type, save in unused field in symtab */

       NODE *bdty(int op, ...);
static void fend(struct symtab *);
static struct symtab *fundef(NODE *tp, NODE *p);
static void olddecl(NODE *p, NODE *a);
static struct symtab *init_declarator(NODE *tn, NODE *p, int assign, NODE *a);
static void resetbc(int mask);
static void swend(void);
static void addcase(NODE *p);
#ifdef GCC_COMPAT
static void gcccase(NODE *p, NODE *);
#endif
static struct attr *gcc_attr_wrapper(NODE *p);
static void adddef(void);
static void savebc(void);
static void swstart(int, TWORD);
static void genswitch(int, TWORD, struct swents **, int);
static char *mkpstr(char *str);
static struct symtab *clbrace(NODE *);
static NODE *cmop(NODE *l, NODE *r);
static NODE *xcmop(NODE *out, NODE *in, NODE *str);
static void mkxasm(char *str, NODE *p);
static NODE *xasmop(char *str, NODE *p);
static int maxstlen(char *str);
static char *stradd(char *old, char *new);
static NODE *biop(int op, NODE *l, NODE *r);
static void flend(void);
static char * simname(char *s);
static NODE *tyof(NODE *);	/* COMPAT_GCC */
static NODE *voidcon(void);	/* COMPAT_GCC */
static NODE *funargs(NODE *p);
static void oldargs(NODE *p);
static void uawarn(NODE *p, char *s);
static int con_e(NODE *p);
static void dainit(NODE *d, NODE *a);
static NODE *tymfix(NODE *p);
static NODE *namekill(NODE *p, int clr);
static NODE *aryfix(NODE *p);

#define	TYMFIX(inp) { \
	NODE *pp = inp; \
	inp = tymerge(pp->n_left, pp->n_right); \
	nfree(pp->n_left); nfree(pp); }
/*
 * State for saving current switch state (when nested switches).
 */
struct savbc {
	struct savbc *next;
	int brklab;
	int contlab;
	int flostat;
	int swx;
} *savbc, *savctx;


#line 143 "y.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    C_STRING = 258,                /* C_STRING  */
    C_ICON = 259,                  /* C_ICON  */
    C_FCON = 260,                  /* C_FCON  */
    C_NAME = 261,                  /* C_NAME  */
    C_TYPENAME = 262,              /* C_TYPENAME  */
    C_ANDAND = 263,                /* C_ANDAND  */
    C_OROR = 264,                  /* C_OROR  */
    C_GOTO = 265,                  /* C_GOTO  */
    C_RETURN = 266,                /* C_RETURN  */
    C_TYPE = 267,                  /* C_TYPE  */
    C_CLASS = 268,                 /* C_CLASS  */
    C_ASOP = 269,                  /* C_ASOP  */
    C_RELOP = 270,                 /* C_RELOP  */
    C_EQUOP = 271,                 /* C_EQUOP  */
    C_DIVOP = 272,                 /* C_DIVOP  */
    C_SHIFTOP = 273,               /* C_SHIFTOP  */
    C_INCOP = 274,                 /* C_INCOP  */
    C_UNOP = 275,                  /* C_UNOP  */
    C_STROP = 276,                 /* C_STROP  */
    C_STRUCT = 277,                /* C_STRUCT  */
    C_IF = 278,                    /* C_IF  */
    C_ELSE = 279,                  /* C_ELSE  */
    C_SWITCH = 280,                /* C_SWITCH  */
    C_BREAK = 281,                 /* C_BREAK  */
    C_CONTINUE = 282,              /* C_CONTINUE  */
    C_WHILE = 283,                 /* C_WHILE  */
    C_DO = 284,                    /* C_DO  */
    C_FOR = 285,                   /* C_FOR  */
    C_DEFAULT = 286,               /* C_DEFAULT  */
    C_CASE = 287,                  /* C_CASE  */
    C_SIZEOF = 288,                /* C_SIZEOF  */
    C_ALIGNOF = 289,               /* C_ALIGNOF  */
    C_ENUM = 290,                  /* C_ENUM  */
    C_ELLIPSIS = 291,              /* C_ELLIPSIS  */
    C_QUALIFIER = 292,             /* C_QUALIFIER  */
    C_FUNSPEC = 293,               /* C_FUNSPEC  */
    C_ASM = 294,                   /* C_ASM  */
    NOMATCH = 295,                 /* NOMATCH  */
    C_TYPEOF = 296,                /* C_TYPEOF  */
    C_ATTRIBUTE = 297,             /* C_ATTRIBUTE  */
    PCC_OFFSETOF = 298,            /* PCC_OFFSETOF  */
    GCC_DESIG = 299,               /* GCC_DESIG  */
    CXX_NAMESPACE = 300,           /* CXX_NAMESPACE  */
    CXX_DUALCC = 301,              /* CXX_DUALCC  */
    CXX_TEMPLATE = 302,            /* CXX_TEMPLATE  */
    CXX_USING = 303,               /* CXX_USING  */
    CXX_TYPENAME = 304,            /* CXX_TYPENAME  */
    CXX_CASTS = 305,               /* CXX_CASTS  */
    CXX_THROW = 306,               /* CXX_THROW  */
    CXX_MORENM = 307,              /* CXX_MORENM  */
    CXX_NEW = 308,                 /* CXX_NEW  */
    CXX_DELETE = 309,              /* CXX_DELETE  */
    CXX_CLASS = 310                /* CXX_CLASS  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define C_STRING 258
#define C_ICON 259
#define C_FCON 260
#define C_NAME 261
#define C_TYPENAME 262
#define C_ANDAND 263
#define C_OROR 264
#define C_GOTO 265
#define C_RETURN 266
#define C_TYPE 267
#define C_CLASS 268
#define C_ASOP 269
#define C_RELOP 270
#define C_EQUOP 271
#define C_DIVOP 272
#define C_SHIFTOP 273
#define C_INCOP 274
#define C_UNOP 275
#define C_STROP 276
#define C_STRUCT 277
#define C_IF 278
#define C_ELSE 279
#define C_SWITCH 280
#define C_BREAK 281
#define C_CONTINUE 282
#define C_WHILE 283
#define C_DO 284
#define C_FOR 285
#define C_DEFAULT 286
#define C_CASE 287
#define C_SIZEOF 288
#define C_ALIGNOF 289
#define C_ENUM 290
#define C_ELLIPSIS 291
#define C_QUALIFIER 292
#define C_FUNSPEC 293
#define C_ASM 294
#define NOMATCH 295
#define C_TYPEOF 296
#define C_ATTRIBUTE 297
#define PCC_OFFSETOF 298
#define GCC_DESIG 299
#define CXX_NAMESPACE 300
#define CXX_DUALCC 301
#define CXX_TEMPLATE 302
#define CXX_USING 303
#define CXX_TYPENAME 304
#define CXX_CASTS 305
#define CXX_THROW 306
#define CXX_MORENM 307
#define CXX_NEW 308
#define CXX_DELETE 309
#define CXX_CLASS 310

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 228 "./cgram.y"

	int intval;
	NODE *nodep;
	struct symtab *symp;
	struct rstack *rp;
	char *strp;

#line 314 "y.tab.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_C_STRING = 3,                   /* C_STRING  */
  YYSYMBOL_C_ICON = 4,                     /* C_ICON  */
  YYSYMBOL_C_FCON = 5,                     /* C_FCON  */
  YYSYMBOL_C_NAME = 6,                     /* C_NAME  */
  YYSYMBOL_C_TYPENAME = 7,                 /* C_TYPENAME  */
  YYSYMBOL_C_ANDAND = 8,                   /* C_ANDAND  */
  YYSYMBOL_C_OROR = 9,                     /* C_OROR  */
  YYSYMBOL_C_GOTO = 10,                    /* C_GOTO  */
  YYSYMBOL_C_RETURN = 11,                  /* C_RETURN  */
  YYSYMBOL_C_TYPE = 12,                    /* C_TYPE  */
  YYSYMBOL_C_CLASS = 13,                   /* C_CLASS  */
  YYSYMBOL_C_ASOP = 14,                    /* C_ASOP  */
  YYSYMBOL_C_RELOP = 15,                   /* C_RELOP  */
  YYSYMBOL_C_EQUOP = 16,                   /* C_EQUOP  */
  YYSYMBOL_C_DIVOP = 17,                   /* C_DIVOP  */
  YYSYMBOL_C_SHIFTOP = 18,                 /* C_SHIFTOP  */
  YYSYMBOL_C_INCOP = 19,                   /* C_INCOP  */
  YYSYMBOL_C_UNOP = 20,                    /* C_UNOP  */
  YYSYMBOL_C_STROP = 21,                   /* C_STROP  */
  YYSYMBOL_C_STRUCT = 22,                  /* C_STRUCT  */
  YYSYMBOL_C_IF = 23,                      /* C_IF  */
  YYSYMBOL_C_ELSE = 24,                    /* C_ELSE  */
  YYSYMBOL_C_SWITCH = 25,                  /* C_SWITCH  */
  YYSYMBOL_C_BREAK = 26,                   /* C_BREAK  */
  YYSYMBOL_C_CONTINUE = 27,                /* C_CONTINUE  */
  YYSYMBOL_C_WHILE = 28,                   /* C_WHILE  */
  YYSYMBOL_C_DO = 29,                      /* C_DO  */
  YYSYMBOL_C_FOR = 30,                     /* C_FOR  */
  YYSYMBOL_C_DEFAULT = 31,                 /* C_DEFAULT  */
  YYSYMBOL_C_CASE = 32,                    /* C_CASE  */
  YYSYMBOL_C_SIZEOF = 33,                  /* C_SIZEOF  */
  YYSYMBOL_C_ALIGNOF = 34,                 /* C_ALIGNOF  */
  YYSYMBOL_C_ENUM = 35,                    /* C_ENUM  */
  YYSYMBOL_C_ELLIPSIS = 36,                /* C_ELLIPSIS  */
  YYSYMBOL_C_QUALIFIER = 37,               /* C_QUALIFIER  */
  YYSYMBOL_C_FUNSPEC = 38,                 /* C_FUNSPEC  */
  YYSYMBOL_C_ASM = 39,                     /* C_ASM  */
  YYSYMBOL_NOMATCH = 40,                   /* NOMATCH  */
  YYSYMBOL_C_TYPEOF = 41,                  /* C_TYPEOF  */
  YYSYMBOL_C_ATTRIBUTE = 42,               /* C_ATTRIBUTE  */
  YYSYMBOL_PCC_OFFSETOF = 43,              /* PCC_OFFSETOF  */
  YYSYMBOL_GCC_DESIG = 44,                 /* GCC_DESIG  */
  YYSYMBOL_CXX_NAMESPACE = 45,             /* CXX_NAMESPACE  */
  YYSYMBOL_CXX_DUALCC = 46,                /* CXX_DUALCC  */
  YYSYMBOL_CXX_TEMPLATE = 47,              /* CXX_TEMPLATE  */
  YYSYMBOL_CXX_USING = 48,                 /* CXX_USING  */
  YYSYMBOL_CXX_TYPENAME = 49,              /* CXX_TYPENAME  */
  YYSYMBOL_CXX_CASTS = 50,                 /* CXX_CASTS  */
  YYSYMBOL_CXX_THROW = 51,                 /* CXX_THROW  */
  YYSYMBOL_CXX_MORENM = 52,                /* CXX_MORENM  */
  YYSYMBOL_CXX_NEW = 53,                   /* CXX_NEW  */
  YYSYMBOL_CXX_DELETE = 54,                /* CXX_DELETE  */
  YYSYMBOL_CXX_CLASS = 55,                 /* CXX_CLASS  */
  YYSYMBOL_56_ = 56,                       /* ','  */
  YYSYMBOL_57_ = 57,                       /* '='  */
  YYSYMBOL_58_ = 58,                       /* '?'  */
  YYSYMBOL_59_ = 59,                       /* ':'  */
  YYSYMBOL_60_ = 60,                       /* '|'  */
  YYSYMBOL_61_ = 61,                       /* '^'  */
  YYSYMBOL_62_ = 62,                       /* '&'  */
  YYSYMBOL_63_ = 63,                       /* '+'  */
  YYSYMBOL_64_ = 64,                       /* '-'  */
  YYSYMBOL_65_ = 65,                       /* '*'  */
  YYSYMBOL_66_ = 66,                       /* '['  */
  YYSYMBOL_67_ = 67,                       /* '('  */
  YYSYMBOL_68_ = 68,                       /* ';'  */
  YYSYMBOL_69_ = 69,                       /* ')'  */
  YYSYMBOL_70_ = 70,                       /* '}'  */
  YYSYMBOL_71_ = 71,                       /* '{'  */
  YYSYMBOL_72_ = 72,                       /* ']'  */
  YYSYMBOL_YYACCEPT = 73,                  /* $accept  */
  YYSYMBOL_edf = 74,                       /* edf  */
  YYSYMBOL_ext_def_list = 75,              /* ext_def_list  */
  YYSYMBOL_external_def = 76,              /* external_def  */
  YYSYMBOL_funtype = 77,                   /* funtype  */
  YYSYMBOL_kr_args = 78,                   /* kr_args  */
  YYSYMBOL_blockdcl = 79,                  /* blockdcl  */
  YYSYMBOL_using_x = 80,                   /* using_x  */
  YYSYMBOL_tnopt = 81,                     /* tnopt  */
  YYSYMBOL_ns_alias = 82,                  /* ns_alias  */
  YYSYMBOL_qual_ns_sp = 83,                /* qual_ns_sp  */
  YYSYMBOL_nested_name_sp = 84,            /* nested_name_sp  */
  YYSYMBOL_nmtnm = 85,                     /* nmtnm  */
  YYSYMBOL_ccopt = 86,                     /* ccopt  */
  YYSYMBOL_simple_decl = 87,               /* simple_decl  */
  YYSYMBOL_namespace = 88,                 /* namespace  */
  YYSYMBOL_nsname = 89,                    /* nsname  */
  YYSYMBOL_ns_body = 90,                   /* ns_body  */
  YYSYMBOL_nsbeg = 91,                     /* nsbeg  */
  YYSYMBOL_extlink = 92,                   /* extlink  */
  YYSYMBOL_eb = 93,                        /* eb  */
  YYSYMBOL_declaration_specifiers = 94,    /* declaration_specifiers  */
  YYSYMBOL_merge_attribs = 95,             /* merge_attribs  */
  YYSYMBOL_type_sq = 96,                   /* type_sq  */
  YYSYMBOL_cf_spec = 97,                   /* cf_spec  */
  YYSYMBOL_typeof = 98,                    /* typeof  */
  YYSYMBOL_new_ma = 99,                    /* new_ma  */
  YYSYMBOL_new_ds = 100,                   /* new_ds  */
  YYSYMBOL_new_type_sq = 101,              /* new_type_sq  */
  YYSYMBOL_attribute_specifier = 102,      /* attribute_specifier  */
  YYSYMBOL_attribute_list = 103,           /* attribute_list  */
  YYSYMBOL_attribute = 104,                /* attribute  */
  YYSYMBOL_declarator = 105,               /* declarator  */
  YYSYMBOL_type_qualifier_list = 106,      /* type_qualifier_list  */
  YYSYMBOL_identifier_list = 107,          /* identifier_list  */
  YYSYMBOL_parameter_type_list = 108,      /* parameter_type_list  */
  YYSYMBOL_parameter_list = 109,           /* parameter_list  */
  YYSYMBOL_parameter_declaration = 110,    /* parameter_declaration  */
  YYSYMBOL_abstract_declarator = 111,      /* abstract_declarator  */
  YYSYMBOL_ib2 = 112,                      /* ib2  */
  YYSYMBOL_arg_dcl_list = 113,             /* arg_dcl_list  */
  YYSYMBOL_arg_declaration = 114,          /* arg_declaration  */
  YYSYMBOL_arg_param_list = 115,           /* arg_param_list  */
  YYSYMBOL_block_item_list = 116,          /* block_item_list  */
  YYSYMBOL_block_item = 117,               /* block_item  */
  YYSYMBOL_declaration = 118,              /* declaration  */
  YYSYMBOL_init_declarator_list = 119,     /* init_declarator_list  */
  YYSYMBOL_120_1 = 120,                    /* @1  */
  YYSYMBOL_enum_dcl = 121,                 /* enum_dcl  */
  YYSYMBOL_enum_head = 122,                /* enum_head  */
  YYSYMBOL_moe_list = 123,                 /* moe_list  */
  YYSYMBOL_moe = 124,                      /* moe  */
  YYSYMBOL_struct_dcl = 125,               /* struct_dcl  */
  YYSYMBOL_attr_var = 126,                 /* attr_var  */
  YYSYMBOL_attr_spec_list = 127,           /* attr_spec_list  */
  YYSYMBOL_str_head = 128,                 /* str_head  */
  YYSYMBOL_struct_dcl_list = 129,          /* struct_dcl_list  */
  YYSYMBOL_struct_declaration = 130,       /* struct_declaration  */
  YYSYMBOL_optsemi = 131,                  /* optsemi  */
  YYSYMBOL_specifier_qualifier_list = 132, /* specifier_qualifier_list  */
  YYSYMBOL_merge_specifiers = 133,         /* merge_specifiers  */
  YYSYMBOL_struct_declarator_list = 134,   /* struct_declarator_list  */
  YYSYMBOL_135_2 = 135,                    /* @2  */
  YYSYMBOL_struct_declarator = 136,        /* struct_declarator  */
  YYSYMBOL_xnfdeclarator = 137,            /* xnfdeclarator  */
  YYSYMBOL_init_declarator = 138,          /* init_declarator  */
  YYSYMBOL_begbr = 139,                    /* begbr  */
  YYSYMBOL_initializer = 140,              /* initializer  */
  YYSYMBOL_init_list = 141,                /* init_list  */
  YYSYMBOL_designation = 142,              /* designation  */
  YYSYMBOL_designator_list = 143,          /* designator_list  */
  YYSYMBOL_designator = 144,               /* designator  */
  YYSYMBOL_optcomma = 145,                 /* optcomma  */
  YYSYMBOL_ibrace = 146,                   /* ibrace  */
  YYSYMBOL_compoundstmt = 147,             /* compoundstmt  */
  YYSYMBOL_begin = 148,                    /* begin  */
  YYSYMBOL_statement = 149,                /* statement  */
  YYSYMBOL_asmstatement = 150,             /* asmstatement  */
  YYSYMBOL_mvol = 151,                     /* mvol  */
  YYSYMBOL_xasm = 152,                     /* xasm  */
  YYSYMBOL_oplist = 153,                   /* oplist  */
  YYSYMBOL_oper = 154,                     /* oper  */
  YYSYMBOL_cnstr = 155,                    /* cnstr  */
  YYSYMBOL_label = 156,                    /* label  */
  YYSYMBOL_doprefix = 157,                 /* doprefix  */
  YYSYMBOL_ifprefix = 158,                 /* ifprefix  */
  YYSYMBOL_ifelprefix = 159,               /* ifelprefix  */
  YYSYMBOL_whprefix = 160,                 /* whprefix  */
  YYSYMBOL_forprefix = 161,                /* forprefix  */
  YYSYMBOL_162_3 = 162,                    /* $@3  */
  YYSYMBOL_switchpart = 163,               /* switchpart  */
  YYSYMBOL_164_e = 164,                    /* .e  */
  YYSYMBOL_elist = 165,                    /* elist  */
  YYSYMBOL_e = 166,                        /* e  */
  YYSYMBOL_xbegin = 167,                   /* xbegin  */
  YYSYMBOL_addrlbl = 168,                  /* addrlbl  */
  YYSYMBOL_term = 169,                     /* term  */
  YYSYMBOL_nmrec = 170,                    /* nmrec  */
  YYSYMBOL_xa = 171,                       /* xa  */
  YYSYMBOL_clbrace = 172,                  /* clbrace  */
  YYSYMBOL_string = 173,                   /* string  */
  YYSYMBOL_cast_type = 174                 /* cast_type  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  66
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2524

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  73
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  102
/* YYNRULES -- Number of rules.  */
#define YYNRULES  310
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  558

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   310


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    62,     2,
      67,    69,    65,    63,    56,    64,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    59,    68,
       2,    57,     2,    58,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    66,     2,    72,    61,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    71,    60,    70,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   263,   263,   264,   267,   268,   271,   272,   273,   274,
     275,   276,   277,   280,   284,   287,   288,   291,   292,   293,
     294,   297,   298,   301,   302,   305,   308,   311,   312,   313,
     316,   317,   320,   321,   324,   327,   330,   331,   334,   335,
     338,   341,   342,   343,   346,   363,   366,   367,   368,   369,
     372,   373,   381,   382,   383,   384,   385,   388,   389,   393,
     394,   397,   398,   399,   400,   403,   406,   407,   415,   416,
     417,   421,   424,   425,   428,   433,   434,   442,   443,   447,
     448,   449,   453,   454,   455,   461,   462,   463,   466,   470,
     474,   475,   480,   483,   488,   489,   499,   500,   510,   511,
     512,   521,   526,   529,   535,   536,   540,   541,   545,   546,
     550,   554,   558,   561,   562,   565,   568,   573,   578,   579,
     583,   588,   592,   601,   602,   605,   606,   616,   617,   628,
     629,   629,   635,   636,   639,   640,   643,   644,   647,   648,
     649,   650,   653,   662,   666,   670,   673,   685,   688,   689,
     692,   693,   696,   697,   701,   704,   707,   708,   712,   715,
     716,   720,   721,   721,   725,   735,   741,   753,   768,   780,
     783,   793,   794,   803,   811,   815,   816,   819,   822,   823,
     824,   825,   828,   829,   832,   833,   834,   835,   838,   839,
     842,   850,   855,   862,   863,   866,   871,   872,   875,   898,
     899,   900,   901,   907,   916,   929,   940,   948,   956,   964,
     975,  1000,  1001,  1002,  1003,  1004,  1005,  1006,  1009,  1010,
    1013,  1014,  1017,  1018,  1019,  1022,  1023,  1026,  1027,  1032,
    1033,  1036,  1037,  1038,  1039,  1044,  1047,  1055,  1060,  1070,
    1084,  1097,  1097,  1111,  1136,  1137,  1140,  1141,  1142,  1143,
    1153,  1154,  1155,  1156,  1159,  1164,  1165,  1166,  1167,  1168,
    1169,  1170,  1171,  1172,  1173,  1174,  1175,  1176,  1177,  1180,
    1185,  1197,  1198,  1199,  1200,  1201,  1202,  1208,  1209,  1212,
    1213,  1217,  1221,  1229,  1234,  1239,  1245,  1253,  1254,  1255,
    1256,  1257,  1260,  1277,  1278,  1279,  1280,  1281,  1289,  1297,
    1298,  1299,  1302,  1307,  1308,  1311,  1314,  1317,  1318,  1321,
    1324
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "C_STRING", "C_ICON",
  "C_FCON", "C_NAME", "C_TYPENAME", "C_ANDAND", "C_OROR", "C_GOTO",
  "C_RETURN", "C_TYPE", "C_CLASS", "C_ASOP", "C_RELOP", "C_EQUOP",
  "C_DIVOP", "C_SHIFTOP", "C_INCOP", "C_UNOP", "C_STROP", "C_STRUCT",
  "C_IF", "C_ELSE", "C_SWITCH", "C_BREAK", "C_CONTINUE", "C_WHILE", "C_DO",
  "C_FOR", "C_DEFAULT", "C_CASE", "C_SIZEOF", "C_ALIGNOF", "C_ENUM",
  "C_ELLIPSIS", "C_QUALIFIER", "C_FUNSPEC", "C_ASM", "NOMATCH", "C_TYPEOF",
  "C_ATTRIBUTE", "PCC_OFFSETOF", "GCC_DESIG", "CXX_NAMESPACE",
  "CXX_DUALCC", "CXX_TEMPLATE", "CXX_USING", "CXX_TYPENAME", "CXX_CASTS",
  "CXX_THROW", "CXX_MORENM", "CXX_NEW", "CXX_DELETE", "CXX_CLASS", "','",
  "'='", "'?'", "':'", "'|'", "'^'", "'&'", "'+'", "'-'", "'*'", "'['",
  "'('", "';'", "')'", "'}'", "'{'", "']'", "$accept", "edf",
  "ext_def_list", "external_def", "funtype", "kr_args", "blockdcl",
  "using_x", "tnopt", "ns_alias", "qual_ns_sp", "nested_name_sp", "nmtnm",
  "ccopt", "simple_decl", "namespace", "nsname", "ns_body", "nsbeg",
  "extlink", "eb", "declaration_specifiers", "merge_attribs", "type_sq",
  "cf_spec", "typeof", "new_ma", "new_ds", "new_type_sq",
  "attribute_specifier", "attribute_list", "attribute", "declarator",
  "type_qualifier_list", "identifier_list", "parameter_type_list",
  "parameter_list", "parameter_declaration", "abstract_declarator", "ib2",
  "arg_dcl_list", "arg_declaration", "arg_param_list", "block_item_list",
  "block_item", "declaration", "init_declarator_list", "@1", "enum_dcl",
  "enum_head", "moe_list", "moe", "struct_dcl", "attr_var",
  "attr_spec_list", "str_head", "struct_dcl_list", "struct_declaration",
  "optsemi", "specifier_qualifier_list", "merge_specifiers",
  "struct_declarator_list", "@2", "struct_declarator", "xnfdeclarator",
  "init_declarator", "begbr", "initializer", "init_list", "designation",
  "designator_list", "designator", "optcomma", "ibrace", "compoundstmt",
  "begin", "statement", "asmstatement", "mvol", "xasm", "oplist", "oper",
  "cnstr", "label", "doprefix", "ifprefix", "ifelprefix", "whprefix",
  "forprefix", "$@3", "switchpart", ".e", "elist", "e", "xbegin",
  "addrlbl", "term", "nmrec", "xa", "clbrace", "string", "cast_type", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-353)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-246)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     434,  -353,  -353,  -353,  -353,    77,    63,   106,  -353,  -353,
     141,   146,   155,   237,    14,   196,   221,   174,  -353,   227,
     282,   841,  -353,  2205,  -353,  -353,  -353,  -353,  -353,  -353,
      43,  -353,  2205,  2205,  -353,  -353,   186,  -353,  -353,   218,
    -353,   224,  -353,   292,  -353,  -353,    49,    63,   238,  -353,
     256,  1527,   260,   273,    63,   294,  -353,   294,  -353,  -353,
    -353,   186,   340,   354,   174,  -353,  -353,  -353,  -353,   266,
     167,  2205,  -353,  -353,  1725,    79,   288,  -353,  -353,  -353,
     892,  1987,   361,   878,  -353,  1976,   289,   357,  -353,   369,
    -353,  -353,  -353,   312,  1659,  1659,  -353,  -353,   316,   389,
     504,  1552,  1659,  1659,  1659,  1659,  1283,   981,   130,  -353,
     922,   169,   400,   407,   347,   412,   294,   359,  -353,   402,
     402,  -353,  -353,   186,  -353,   358,  -353,  -353,   524,     3,
     194,  -353,   366,   381,    63,  -353,  1324,  1659,  1158,  -353,
    1250,  -353,  -353,   120,    -5,   373,   388,   390,   398,   424,
     429,  -353,   431,  -353,   140,  1927,  -353,   959,    43,  -353,
    -353,     6,  1659,   169,   169,  1684,   433,   981,   981,  -353,
    -353,  -353,   981,   485,  -353,  -353,   485,  -353,  -353,   432,
     169,   169,   169,   169,   169,  -353,  1024,   737,   427,  -353,
     291,  1164,   321,   370,  1659,  1659,  1659,  1659,  1659,  1659,
    1659,  1659,  1593,  1618,  1659,  1659,  1659,  1659,  1659,  1659,
    -353,  -353,   451,  1659,  1659,  -353,  -353,  -353,   441,   437,
     465,   456,   402,  -353,  1008,  -353,  -353,   464,   487,   472,
    -353,   245,    74,   501,    55,  1437,   478,   497,   500,   502,
     505,  -353,   506,   510,  1659,  -353,  -353,   595,  -353,  -353,
    -353,  -353,   507,   806,   806,   806,   806,   806,  1659,   806,
    2207,  -353,   167,  -353,   369,  -353,   573,  -353,   185,  2459,
    -353,  1757,  -353,  -353,   259,   212,     3,   370,   574,  -353,
    -353,  2119,  -353,  1659,  1659,   361,   511,  -353,  1659,   755,
     248,  -353,  -353,  -353,  -353,  1057,   135,   369,  -353,   513,
     148,  2459,  1283,   169,   981,   527,   569,   516,  -353,  -353,
    1659,  -353,   666,  1396,   447,   370,    63,  1770,  -353,   411,
    2205,  1229,   521,  1679,  1178,  2459,   285,   242,  -353,   229,
    2459,  2459,  -353,  1659,  2366,  2439,  1117,   385,    16,    16,
    -353,  -353,  -353,  1829,   164,  1659,   528,   412,  -353,  -353,
    1106,   523,  -353,   136,  -353,  -353,  -353,    63,    63,   535,
    1659,  -353,  2220,  1659,  1659,  -353,  -353,  1659,  1473,  -353,
    2308,  -353,  -353,  -353,   501,  -353,   576,   585,  -353,  -353,
     541,  2388,  -353,  -353,     3,    17,   167,  -353,   481,  -353,
    1659,  -353,   555,  1360,     5,  -353,  -353,   332,  -353,  -353,
    -353,  -353,  2459,  2459,  -353,  -353,  2459,  1659,  -353,  -353,
    -353,   544,  -353,   554,   560,    18,  -353,  1527,  -353,   550,
     562,  1659,   568,  -353,   169,   570,  2278,  -353,   169,   315,
     370,  -353,    63,  -353,   571,    63,  1842,  -353,  2205,  1943,
    1659,  -353,  -353,   215,  -353,  -353,  -353,   402,  -353,  -353,
    -353,  -353,  2291,  -353,  2018,  2034,  2049,  2205,   575,  1659,
    -353,   577,  -353,   806,  -353,   137,  -353,  -353,  -353,  1744,
     349,   572,  -353,  -353,   439,  2459,  -353,  -353,  1659,  -353,
    2401,   140,  -353,   369,   369,  1659,  2459,  -353,  1401,  -353,
     345,  1659,  -353,   580,  -353,   555,  -353,  -353,  -353,    63,
     582,  1943,  -353,  -353,  -353,  -353,  -353,  -353,  1659,  1659,
    2377,  1659,  -353,  -353,  1659,  -353,  1360,  -353,  -353,   555,
    1855,    63,  -353,   587,    50,  2107,  -353,  2121,   583,   584,
    -353,  -353,   588,   593,  -353,  2135,  1914,  -353,   594,   369,
    1659,  -353,  -353,  -353,  -353,  -353,  -353,   600,   596,  -353,
     599,   407,  2193,  -353,  -353,   369,  -353,   407
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       0,    12,    80,    51,    50,    57,   146,   134,    54,    58,
     220,     0,     0,    37,    24,   303,     0,     0,    11,     0,
       0,     0,     5,    15,    10,    20,    19,    17,     8,     9,
       0,    45,    46,    48,    56,    55,    13,     7,    53,     0,
      52,     0,    18,     0,    44,   148,   150,   147,   133,   221,
       0,     0,     0,    36,   146,    33,    23,    33,   304,    90,
      92,    77,     0,     0,     0,    34,     1,     4,    57,     0,
       0,    16,   118,   127,    14,     0,     0,   129,    47,    49,
       0,     0,     0,     0,    79,     0,   143,     0,   149,     0,
     307,   293,   294,   290,     0,     0,   305,   305,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   160,   309,   158,
       0,   268,     0,   295,     0,    74,    33,     0,    32,     0,
       0,    91,    93,    78,    82,     0,   198,     6,     0,   146,
       0,   119,     0,   171,   146,   128,     0,     0,     0,    85,
       0,    94,    89,   103,     0,     0,    96,    98,   138,   139,
     193,   136,     0,   145,   168,     0,   152,     0,     0,    43,
     144,     0,   246,   278,   277,     0,     0,     0,     0,    67,
      66,    70,     0,    63,    65,   299,    61,    69,    68,     0,
     301,   273,   275,   274,   272,   269,     0,     0,     0,   159,
     104,     0,   117,   310,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      59,   271,     0,     0,   246,   291,   308,    60,    75,     0,
      72,     0,     0,    40,     0,    30,    31,     0,    27,     0,
      81,     0,   290,    51,     0,     0,     0,     0,     0,     0,
       0,   236,     0,     0,     0,   214,   197,     0,   123,   125,
     200,   126,     0,     0,     0,     0,     0,     0,   245,     0,
       0,   121,     0,   120,     0,   130,     0,   177,   187,   173,
     176,     0,    86,    83,   104,   117,   146,   102,     0,    88,
      87,     0,    99,     0,     0,   194,     0,   155,     0,   146,
       0,   161,   142,   153,    42,     0,   146,   225,   218,     0,
       0,   247,     0,   279,     0,     0,     0,     0,    64,    62,
       0,   296,     0,     0,   105,   106,   146,     0,   113,     0,
       0,     0,   117,   256,   255,   252,   261,   260,   265,   262,
     250,   251,   267,     0,     0,   257,   258,   259,   263,   264,
     266,   288,   289,     0,     0,   246,     0,    74,    25,    26,
       0,     0,    22,     0,    21,   215,   216,   146,   146,     0,
       0,   209,     0,     0,     0,   207,   208,     0,   241,   235,
       0,   196,   124,   213,     0,   217,     0,   201,   202,   203,
       0,   244,   206,   199,   146,     0,     0,   270,     0,   185,
       0,   175,   193,     0,     0,   188,    84,   105,   101,    95,
      97,   100,   140,   141,   137,   132,   165,     0,   164,   162,
     156,   154,    41,   222,   226,     0,   219,     0,   286,     0,
       0,     0,     0,   300,   302,     0,     0,   306,   280,   187,
     107,   109,   146,   108,     0,   146,     0,   115,     0,   254,
       0,   285,   287,     0,    71,    73,    35,     0,    28,   231,
     232,   211,     0,   210,     0,     0,     0,     0,     0,     0,
     233,     0,   238,     0,   122,   146,   131,   192,   191,     0,
     187,     0,   195,   182,   187,   178,   179,   184,     0,   189,
     166,   168,   157,   225,     0,     0,   248,   249,   281,   282,
       0,     0,   298,   199,   284,   193,   110,   114,   111,   146,
       0,   253,    76,    29,   212,   237,   243,   239,   245,   245,
       0,     0,   205,   172,     0,   190,     0,   174,   181,   193,
       0,   167,   163,   223,     0,     0,   292,     0,     0,     0,
     112,   116,     0,     0,   234,     0,     0,   183,     0,     0,
       0,   227,   276,   297,   283,   242,   240,     0,     0,   180,
     224,   229,     0,   204,   186,     0,   228,   230
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -353,  -353,  -135,   -15,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -108,  -353,    75,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,    53,   486,   -28,   -73,  -353,  -119,  -353,  -353,    -6,
     327,  -353,    57,  -172,  -353,  -268,  -353,   360,  -100,   353,
    -353,   609,  -353,   494,  -203,   -81,  -353,  -353,   -69,  -353,
    -353,   397,   -58,    -4,   -14,  -353,  -353,   529,  -353,  -353,
     603,  -353,  -353,   202,  -353,   301,  -353,   190,  -352,   220,
    -353,   308,  -326,  -353,   642,   -99,  -240,    25,  -353,  -353,
     230,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,  -353,
    -353,  -344,  -174,   -46,  -353,  -106,   -66,    67,   615,  -353,
     -88,   -74
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,    20,    21,    22,    23,    69,    24,    25,    57,    26,
     221,   227,   228,   119,    27,    28,    54,   351,   224,    29,
      85,    30,    31,    32,    33,    34,   174,   175,   176,    35,
     219,   220,    36,    62,   144,   145,   146,   147,   315,   320,
      71,    72,   130,   247,   248,    37,    75,   386,    38,    39,
     150,   151,    40,   133,    47,    41,   155,   156,   411,   108,
     109,   290,   481,   291,    76,    77,   268,   473,   392,   393,
     394,   395,   286,   474,   250,   128,   251,   252,    50,   299,
     413,   414,   550,   253,   254,   255,   256,   257,   258,   457,
     259,   380,   300,   260,   187,   476,   111,   112,   165,   429,
     113,   114
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      45,   161,    46,    64,   159,   110,    67,   185,   193,   216,
      60,    45,   229,   375,   376,   377,   378,   379,   314,   382,
     216,   216,   295,   107,   458,    42,   388,   173,   163,   164,
     270,   177,   188,   199,   140,   180,   181,   182,   183,   184,
     344,    88,   178,   277,   372,    12,    42,   249,    45,     2,
     117,   278,   434,   216,   308,    86,   122,   309,    88,    55,
     186,   359,   477,    56,   279,   297,   471,    43,    45,    80,
      81,   478,   184,    61,    63,   298,    70,   495,   107,   107,
      44,   209,    58,    43,    43,   485,   465,    74,    43,   350,
     269,   271,   319,   305,   306,    15,   332,    43,   307,   303,
     173,    15,   397,   173,   177,    12,   249,   177,    16,   372,
      17,    73,    48,    87,   349,   178,   301,   540,   178,   123,
     360,   125,   519,    45,    70,   261,     2,   129,    45,    43,
     265,    43,   120,   357,   143,   134,   154,    43,   158,   107,
     107,   162,   225,   226,   107,   317,     2,   135,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   334,   335,   336,
     337,   338,   339,   340,   532,   533,   249,   343,   301,   529,
     500,   443,    15,     2,   132,   319,   385,    12,    49,    12,
       2,   158,    42,   447,    60,   274,   191,   275,   211,   362,
     212,   222,    15,   538,  -170,   190,   191,   192,   370,   288,
     276,    80,    81,   185,   417,    16,   388,    17,   154,   415,
      43,   289,   381,    51,   430,   296,    12,   418,     2,    15,
     417,    43,    52,   512,    43,    43,    15,     2,   419,   389,
     420,   249,    16,   442,    17,   213,   214,   402,   403,    16,
     158,    17,   406,    53,   424,   448,   199,   428,    15,    42,
     262,   390,    80,    81,    12,   391,   186,   197,    59,   199,
     200,    64,   263,    12,    15,     2,   426,    65,    60,    45,
      45,   417,   398,    15,   107,   436,   107,   274,   191,   275,
      67,   318,    66,    45,   502,   408,    16,   439,    17,    82,
      45,    43,   207,   208,   209,    83,    59,   430,    84,   301,
     158,    12,   199,   200,   409,   207,   208,   209,   122,  -135,
      45,    15,   431,   355,   452,   356,   410,   454,   455,   384,
      42,   456,   381,    89,   274,   191,   275,   115,    59,    43,
     116,    61,    63,    12,   143,    67,   388,   126,     2,   503,
     118,    43,    43,   487,   469,   136,     2,   475,   207,   208,
     209,    45,    45,   449,   450,   490,   190,   191,   192,   389,
    -151,   480,    43,   160,   211,   158,   212,   148,   149,   121,
     388,   486,    90,   143,    12,    42,   508,   121,    45,   162,
     464,   390,    12,   167,    15,   494,   190,   191,   192,   107,
     318,   122,    15,   389,   501,   415,   524,   274,   191,   275,
     197,   198,   199,   200,   168,    16,   215,    17,   225,   226,
     216,   213,   214,   510,   526,   390,   217,    43,   218,  -194,
      80,    81,   428,   124,    80,    81,    45,   230,   496,    45,
     223,   498,   520,   264,    -3,     1,   321,   322,  -169,   525,
       2,     3,   280,   296,   281,   527,     4,     5,   207,   208,
     209,   551,   282,    43,   123,   283,     6,   341,   342,    45,
     388,   513,   381,   381,    43,   535,   521,   557,   536,     7,
     475,     8,     9,    10,    45,    11,    12,   321,   322,    13,
     433,   284,    14,   389,   121,   285,    15,   467,   468,    12,
     287,   143,   169,    45,   552,   530,   313,   170,    68,    16,
     304,    17,    18,    19,   310,   390,   346,     6,   345,   518,
     158,   169,   190,   191,   192,    88,   170,    68,    78,    79,
       7,   347,   171,     9,   348,   231,     6,    90,    91,    92,
     232,   233,   352,   353,   234,   235,     4,    68,   289,     7,
     354,   171,     9,    94,    95,   363,     6,   236,    43,   237,
     238,   239,   240,   241,   242,   243,   244,    96,    97,     7,
     358,     8,     9,    10,   364,    11,    12,    98,   365,   369,
     366,   172,   367,   368,    99,   373,    15,   100,   101,   387,
     399,   405,   416,   421,   422,   423,   102,   103,   104,   105,
     437,   106,   245,   446,   246,   126,   231,   444,    90,    91,
      92,   232,   233,   451,   461,   234,   235,     4,    68,   462,
     463,   470,   482,   483,    94,    95,   484,     6,   236,   488,
     237,   238,   239,   240,   241,   242,   243,   244,    96,    97,
       7,   489,     8,     9,    10,   491,    11,    12,    98,   492,
     497,   401,   517,   509,   511,    99,   539,    15,   100,   101,
     528,   531,   543,   554,   544,   555,   545,   102,   103,   104,
     105,   546,   106,   245,   549,   371,   126,   231,   553,    90,
      91,    92,   232,   233,   445,   438,   234,   235,     4,    68,
     131,   312,   404,   522,   293,    94,    95,   466,     6,   236,
     516,   237,   238,   239,   240,   241,   242,   243,   244,    96,
      97,     7,   479,     8,     9,    10,   537,    11,    12,    98,
     189,   127,   166,   523,     0,     0,    99,     0,    15,   100,
     101,     0,     0,     0,     0,     0,     0,     0,   102,   103,
     104,   105,     0,   106,   245,     0,   425,   126,   231,     0,
      90,    91,    92,   232,   233,     0,     0,   234,   235,     4,
      68,     0,     0,     0,     0,     0,    94,    95,     0,     6,
     236,     0,   237,   238,   239,   240,   241,   242,   243,   244,
      96,    97,     7,     0,     8,     9,    10,     0,    11,    12,
      98,     0,     0,     0,     0,     0,     0,    99,     0,    15,
     100,   101,     0,     0,     0,     0,     0,    12,     0,   102,
     103,   104,   105,     0,   106,   245,     0,   231,   126,    90,
      91,    92,   232,   374,   407,     0,   234,   235,     0,     0,
       0,    80,    81,     0,     0,    94,    95,     0,     0,   236,
       0,   237,   238,   239,   240,   241,   242,   243,   244,    96,
      97,    -2,     1,     0,     0,    10,     0,     2,     3,    98,
       0,     0,     0,     4,     5,     0,    99,     0,    15,   100,
     101,     0,     0,     6,     0,     0,     0,     0,   102,   103,
     104,   105,     0,   106,   245,     0,     7,   126,     8,     9,
      10,     0,    11,    12,   152,     3,    13,     0,     0,    14,
       4,    68,     0,    15,     0,    90,    91,    92,    93,     0,
       6,     0,     0,     0,     0,   137,    16,     0,    17,    18,
      19,    94,    95,     7,     0,     8,     9,     0,     0,    11,
      12,     0,     0,     0,     0,    96,    97,     0,     0,     0,
     194,   195,     0,     0,     0,    98,   196,   197,   198,   199,
     200,     0,    99,     0,    15,   100,   101,     0,   153,     0,
       0,     0,     0,     0,   102,   103,   104,   138,     0,   106,
       1,     0,     0,     0,   139,     2,     3,     0,     0,     0,
       0,     4,     5,     0,     0,     0,     0,     0,   201,   202,
     203,     6,   204,   205,   206,   207,   208,   209,     3,     0,
       0,   210,     0,     4,     7,     0,     8,     9,    10,     0,
      11,    12,     0,     6,    13,     0,     0,    14,     0,     1,
       0,    15,     0,     0,     2,     3,     7,     0,     8,     0,
       4,     5,    11,    12,    16,     0,    17,    18,    19,   294,
       6,     0,   194,   195,     0,     0,     0,     0,   196,   197,
     198,   199,   200,     7,     0,     8,     9,    10,     0,    11,
      12,     0,     0,    13,     0,     0,    14,     0,     1,     0,
      15,     0,     0,     2,     3,     0,     0,     0,     0,     4,
       5,     0,     0,    16,     0,    17,    18,    19,   -39,     6,
     201,   202,   203,     0,   204,   205,   206,   207,   208,   209,
       0,     0,     7,   311,     8,     9,    10,     0,    11,    12,
       0,     0,    13,     0,     0,    14,     0,     1,     0,    15,
       0,     0,     2,     3,     0,     0,     0,     0,     4,     5,
       0,     0,    16,     0,    17,    18,    19,   412,     6,     0,
       0,     0,   197,   198,   199,   200,     0,     0,     0,     0,
       0,     7,     0,     8,     9,    10,     0,    11,    12,     0,
       0,    13,     0,     0,    14,     0,     0,     0,    15,     0,
       0,    90,    91,    92,    93,     0,     0,    90,    91,    92,
      93,    16,     0,    17,    18,    19,   -38,    94,    95,   206,
     207,   208,   209,    94,    95,     0,   194,     0,     0,     0,
       0,    96,    97,   197,   198,   199,   200,    96,    97,     0,
       0,    98,     0,     0,     0,     0,     0,    98,    99,     0,
      15,   100,   101,     0,    99,     0,    15,   100,   101,     0,
     102,   103,   104,   105,     0,   106,   102,   103,   104,   105,
     272,   106,    90,    91,    92,    93,   316,     0,   204,   205,
     206,   207,   208,   209,     0,     0,     0,     0,    94,    95,
       0,     0,     0,     0,     0,     0,     0,     0,   194,   195,
       0,     0,    96,    97,   196,   197,   198,   199,   200,     0,
       0,     0,    98,     0,     0,     0,     0,     0,     0,    99,
       0,    15,   100,   101,     0,     0,    90,    91,    92,    93,
       3,   102,   103,   104,   105,     4,   106,     0,     0,     0,
       0,   435,    94,    95,     0,     6,   201,   202,   203,     0,
     204,   205,   206,   207,   208,   209,    96,    97,     7,     0,
       8,     0,   273,     0,    11,    12,    98,    90,    91,    92,
      93,     0,   266,    99,     0,    15,   100,   101,     0,     0,
       0,     0,     0,    94,    95,   102,   103,   104,   105,     0,
     106,     0,     0,     0,   126,     0,     0,    96,    97,     0,
       0,     0,     0,    90,    91,    92,    93,    98,   266,     0,
       0,     0,     0,     0,    99,     0,    15,   100,   101,    94,
      95,     0,     0,     0,     0,     0,   102,   103,   104,   105,
       0,   106,     0,    96,    97,   267,     0,     0,     0,    90,
      91,    92,    93,    98,    90,    91,    92,    93,     0,     0,
      99,     0,    15,   100,   101,    94,    95,     0,     0,     0,
      94,    95,   102,   103,   104,   105,     0,   106,     0,    96,
      97,   472,     0,     0,    96,    97,     0,     0,     0,    98,
      90,    91,    92,    93,    98,     0,    99,     0,    15,   100,
     101,    99,     0,    15,   100,   101,    94,    95,   102,   103,
     104,   105,     0,   106,     0,     0,     0,   427,   106,     0,
      96,    97,   427,     0,     0,     0,    90,    91,    92,    93,
      98,     0,     0,     0,     0,     0,     0,    99,     0,    15,
     100,   101,    94,    95,     0,     0,     0,     0,     0,   102,
     103,   104,   105,     0,   106,   361,    96,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    98,     0,     0,     0,
       0,     0,     0,    99,     0,    15,   100,   101,     0,     0,
      90,    91,    92,    93,     3,   102,   103,   104,   105,     4,
     106,  -245,     0,     0,     0,     0,    94,    95,     0,     6,
       0,     0,     0,     0,     0,    90,    91,    92,    93,     0,
      96,    97,     7,     0,     8,     0,     0,     0,    11,    12,
      98,    94,    95,     0,     0,     0,     0,    99,     0,    15,
     100,   101,     0,     0,     0,    96,    97,     0,     0,   102,
     103,   104,   105,     0,   106,    98,    90,    91,    92,    93,
       0,   266,    99,     0,    15,   100,   101,     0,     0,     0,
       0,     0,    94,    95,   102,   103,   104,   105,   179,   106,
       0,    90,    91,    92,    93,     0,    96,    97,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    94,    95,     0,
       0,     0,     0,    99,     0,    15,   100,   101,     0,     0,
       0,    96,    97,     0,     0,   102,   103,   104,   105,     0,
     106,    98,    90,    91,    92,    93,     0,     0,    99,     0,
      15,   100,   101,     0,     0,     0,     0,   333,    94,    95,
     102,   103,   104,   105,     0,   106,     0,    90,    91,    92,
      93,     0,    96,    97,   197,   198,   199,   200,     0,     0,
       0,     0,    98,    94,    95,     0,     0,     0,     0,    99,
       0,    15,   100,   101,     0,     0,     0,    96,    97,     0,
       0,   102,   103,   104,   105,     0,   106,    98,     0,     0,
       0,     0,     0,     0,    99,     0,    15,   100,   101,   204,
     205,   206,   207,   208,   209,     0,   102,   103,   104,   105,
       0,   302,   194,   195,     0,     0,     0,     0,   196,   197,
     198,   199,   200,     0,   132,   194,   195,    12,     0,     0,
       0,   196,   197,   198,   199,   200,     0,     0,   194,   195,
     514,  -146,  -146,     0,   196,   197,   198,   199,   200,     0,
       0,    80,    81,  -146,     0,     0,     0,     0,     0,     0,
     201,   202,   203,     0,   204,   205,   206,   207,   208,   209,
       0,     0,     0,   201,   202,   203,   515,   204,   205,   206,
     207,   208,   209,     0,     0,     0,   201,   202,   203,   396,
     204,   205,   206,   207,   208,   209,     0,   194,   195,     0,
       0,     0,   432,   196,   197,   198,   199,   200,     0,     0,
     194,   195,     0,     0,     0,     0,   196,   197,   198,   199,
     200,     0,     0,   194,   195,     0,     0,     0,     0,   196,
     197,   198,   199,   200,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   201,   202,   203,     0,   204,
     205,   206,   207,   208,   209,     0,     0,     0,   201,   202,
     203,   441,   204,   205,   206,   207,   208,   209,     0,     0,
       0,   201,   202,   203,   499,   204,   205,   206,   207,   208,
     209,     0,   194,   195,     0,     0,     0,   515,   196,   197,
     198,   199,   200,   152,     3,     0,     0,     0,     0,     4,
      68,     0,     0,     0,     0,     0,     0,     0,     0,     6,
       0,   194,   195,     0,     0,     0,     0,     0,   197,   198,
     199,   200,     7,     0,     8,     9,     0,     0,    11,    12,
     201,   202,   203,     0,   204,   205,   206,   207,   208,   209,
       0,     0,     0,     3,     0,     0,   548,     0,     4,    68,
       0,     0,     0,   141,     3,     0,     0,   292,     6,     4,
      68,   203,     0,   204,   205,   206,   207,   208,   209,     6,
       0,     7,     0,     8,     9,     0,     0,    11,    12,     0,
       0,     0,     7,     0,     8,     9,   194,   195,    11,    12,
       0,     0,   196,   197,   198,   199,   200,     0,     0,     0,
       0,     0,   194,   195,     0,     0,     0,   157,   196,   197,
     198,   199,   200,     0,     0,     0,   142,   194,   195,     0,
       0,     0,     0,   196,   197,   198,   199,   200,     0,     0,
       0,     0,     0,     0,   201,   202,   203,     0,   204,   205,
     206,   207,   208,   209,     0,     0,     0,   505,     0,     0,
     201,   202,   203,     0,   204,   205,   206,   207,   208,   209,
       0,     0,     0,   506,     0,   201,   202,   203,     0,   204,
     205,   206,   207,   208,   209,   194,   195,     0,   507,     0,
       0,   196,   197,   198,   199,   200,     3,     0,     0,   194,
     195,     4,    68,     0,     0,   196,   197,   198,   199,   200,
       0,     6,     0,   194,   195,     0,     0,     0,     0,   196,
     197,   198,   199,   200,     7,   400,     8,     9,     0,     0,
      11,    12,     0,   201,   202,   203,     0,   204,   205,   206,
     207,   208,   209,     0,     0,     0,   541,   201,   202,   203,
       0,   204,   205,   206,   207,   208,   209,     0,     0,     0,
     542,   201,   202,   203,     0,   204,   205,   206,   207,   208,
     209,   194,   195,     0,   547,     0,     0,   196,   197,   198,
     199,   200,     3,     0,     0,   194,   195,     4,    68,     0,
       0,   196,   197,   198,   199,   200,     0,     6,   194,   195,
       0,     0,     0,     0,   196,   197,   198,   199,   200,     0,
       7,     0,     8,     9,     0,     0,    11,    12,     0,   201,
     202,   203,     0,   204,   205,   206,   207,   208,   209,     0,
       0,     0,   556,   201,   202,   203,     0,   204,   205,   206,
     207,   208,   209,     0,     0,   383,   201,   202,   203,     0,
     204,   205,   206,   207,   208,   209,   194,   195,   453,     0,
       0,     0,   196,   197,   198,   199,   200,     0,     0,   194,
     195,     0,     0,     0,     0,   196,   197,   198,   199,   200,
       0,     0,     0,     0,     0,     0,   194,   195,     0,     0,
       0,     0,   196,   197,   198,   199,   200,     0,     0,     0,
       0,     0,     0,     0,   201,   202,   203,     0,   204,   205,
     206,   207,   208,   209,   459,     0,   493,   201,   202,   203,
       0,   204,   205,   206,   207,   208,   209,     0,     0,   504,
       0,     0,     0,     0,   201,   202,   203,   460,   204,   205,
     206,   207,   208,   209,   194,   195,     0,     0,     0,     0,
     196,   197,   198,   199,   200,   194,   195,     0,     0,     0,
       0,   196,   197,   198,   199,   200,   194,   195,     0,     0,
       0,     0,   196,   197,   198,   199,   200,     0,     0,   194,
     195,     0,     0,     0,     0,   196,   197,   198,   199,   200,
       0,     0,   201,   202,   203,   440,   204,   205,   206,   207,
     208,   209,     0,   201,   202,   203,   534,   204,   205,   206,
     207,   208,   209,    12,   201,   202,   203,     0,   204,   205,
     206,   207,   208,   209,   197,   198,   199,   200,   202,   203,
       0,   204,   205,   206,   207,   208,   209,   194,   195,     0,
       0,     0,     0,   196,   197,   198,   199,   200,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     205,   206,   207,   208,   209,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,   203,     0,   204,
     205,   206,   207,   208,   209
};

static const yytype_int16 yycheck[] =
{
       6,    89,     6,    17,    85,    51,    21,   106,   108,     3,
      16,    17,   120,   253,   254,   255,   256,   257,   190,   259,
       3,     3,   157,    51,   368,     0,    21,   100,    94,    95,
     136,   100,   106,    17,    80,   101,   102,   103,   104,   105,
     214,    47,   100,   143,   247,    42,    21,   128,    54,     6,
      54,    56,   320,     3,   173,     6,    62,   176,    64,    45,
     106,     6,    57,    49,    69,    59,   392,     0,    74,    66,
      67,    66,   138,    16,    17,    69,    23,   429,   106,   107,
       3,    65,    15,    16,    17,    67,    69,    30,    21,   224,
     136,   137,   192,   167,   168,    52,   202,    30,   172,   165,
     173,    52,   274,   176,   173,    42,   187,   176,    65,   312,
      67,    68,     6,    46,   222,   173,   162,    67,   176,    62,
      65,    64,   474,   129,    71,   129,     6,    70,   134,    62,
     134,    64,    57,    59,    81,    56,    83,    70,    85,   167,
     168,    67,     6,     7,   172,   191,     6,    68,   194,   195,
     196,   197,   198,   199,   200,   201,   202,   203,   204,   205,
     206,   207,   208,   209,   508,   509,   247,   213,   214,   495,
     438,   345,    52,     6,    39,   275,   264,    42,    37,    42,
       6,   128,   157,    47,   190,    65,    66,    67,    19,   235,
      21,   116,    52,   519,    57,    65,    66,    67,   244,    59,
     143,    66,    67,   302,    56,    65,    21,    67,   155,   297,
     143,   154,   258,    67,   314,   158,    42,    69,     6,    52,
      56,   154,    67,   463,   157,   158,    52,     6,   302,    44,
     304,   312,    65,    69,    67,    66,    67,   283,   284,    65,
     187,    67,   288,     6,   310,   353,    17,   313,    52,   224,
      56,    66,    66,    67,    42,    70,   302,    15,    37,    17,
      18,   275,    68,    42,    52,     6,   312,    40,   274,   275,
     276,    56,   276,    52,   302,   321,   304,    65,    66,    67,
     295,    69,     0,   289,    69,   289,    65,   333,    67,    71,
     296,   224,    63,    64,    65,    71,    37,   397,     6,   345,
     247,    42,    17,    18,    56,    63,    64,    65,   314,    71,
     316,    52,   316,    68,   360,    70,    68,   363,   364,   262,
     295,   367,   368,    67,    65,    66,    67,    67,    37,   262,
      57,   274,   275,    42,   281,   350,    21,    71,     6,   447,
      46,   274,   275,   417,   390,    57,     6,   393,    63,    64,
      65,   357,   358,   357,   358,   421,    65,    66,    67,    44,
      71,   407,   295,     6,    19,   312,    21,     6,     7,    37,
      21,   417,     3,   320,    42,   350,   457,    37,   384,    67,
     384,    66,    42,    67,    52,    70,    65,    66,    67,   417,
      69,   397,    52,    44,   440,   483,   484,    65,    66,    67,
      15,    16,    17,    18,    15,    65,     6,    67,     6,     7,
       3,    66,    67,   459,    69,    66,    69,   350,     6,    70,
      66,    67,   488,    69,    66,    67,   432,    69,   432,   435,
      71,   435,   478,    67,     0,     1,    66,    67,    57,   485,
       6,     7,    69,   386,    56,   491,    12,    13,    63,    64,
      65,   539,    62,   386,   397,    57,    22,     6,     7,   465,
      21,   465,   508,   509,   397,   511,   480,   555,   514,    35,
     516,    37,    38,    39,   480,    41,    42,    66,    67,    45,
      69,    57,    48,    44,    37,    56,    52,     6,     7,    42,
      59,   438,     7,   499,   540,   499,    69,    12,    13,    65,
      67,    67,    68,    69,    72,    66,    69,    22,    67,    70,
     457,     7,    65,    66,    67,   521,    12,    13,    32,    33,
      35,    56,    37,    38,    68,     1,    22,     3,     4,     5,
       6,     7,    68,    46,    10,    11,    12,    13,   481,    35,
      68,    37,    38,    19,    20,    67,    22,    23,   481,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      59,    37,    38,    39,    67,    41,    42,    43,    68,    59,
      68,    67,    67,    67,    50,    68,    52,    53,    54,     6,
       6,    70,    69,    56,    15,    69,    62,    63,    64,    65,
      69,    67,    68,    70,    70,    71,     1,    69,     3,     4,
       5,     6,     7,    68,    28,    10,    11,    12,    13,    24,
      69,    56,    68,    59,    19,    20,    56,    22,    23,    69,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    69,    37,    38,    39,    67,    41,    42,    43,    69,
      69,   281,    70,    68,    67,    50,    59,    52,    53,    54,
      70,    69,    69,    57,    70,    56,    68,    62,    63,    64,
      65,    68,    67,    68,    70,    70,    71,     1,    68,     3,
       4,     5,     6,     7,   347,   322,    10,    11,    12,    13,
      71,   187,   285,   481,   155,    19,    20,   386,    22,    23,
     470,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,   394,    37,    38,    39,   516,    41,    42,    43,
     107,    69,    97,   483,    -1,    -1,    50,    -1,    52,    53,
      54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    -1,    67,    68,    -1,    70,    71,     1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    10,    11,    12,
      13,    -1,    -1,    -1,    -1,    -1,    19,    20,    -1,    22,
      23,    -1,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    -1,    37,    38,    39,    -1,    41,    42,
      43,    -1,    -1,    -1,    -1,    -1,    -1,    50,    -1,    52,
      53,    54,    -1,    -1,    -1,    -1,    -1,    42,    -1,    62,
      63,    64,    65,    -1,    67,    68,    -1,     1,    71,     3,
       4,     5,     6,     7,    59,    -1,    10,    11,    -1,    -1,
      -1,    66,    67,    -1,    -1,    19,    20,    -1,    -1,    23,
      -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,     0,     1,    -1,    -1,    39,    -1,     6,     7,    43,
      -1,    -1,    -1,    12,    13,    -1,    50,    -1,    52,    53,
      54,    -1,    -1,    22,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    -1,    67,    68,    -1,    35,    71,    37,    38,
      39,    -1,    41,    42,     6,     7,    45,    -1,    -1,    48,
      12,    13,    -1,    52,    -1,     3,     4,     5,     6,    -1,
      22,    -1,    -1,    -1,    -1,    13,    65,    -1,    67,    68,
      69,    19,    20,    35,    -1,    37,    38,    -1,    -1,    41,
      42,    -1,    -1,    -1,    -1,    33,    34,    -1,    -1,    -1,
       8,     9,    -1,    -1,    -1,    43,    14,    15,    16,    17,
      18,    -1,    50,    -1,    52,    53,    54,    -1,    70,    -1,
      -1,    -1,    -1,    -1,    62,    63,    64,    65,    -1,    67,
       1,    -1,    -1,    -1,    72,     6,     7,    -1,    -1,    -1,
      -1,    12,    13,    -1,    -1,    -1,    -1,    -1,    56,    57,
      58,    22,    60,    61,    62,    63,    64,    65,     7,    -1,
      -1,    69,    -1,    12,    35,    -1,    37,    38,    39,    -1,
      41,    42,    -1,    22,    45,    -1,    -1,    48,    -1,     1,
      -1,    52,    -1,    -1,     6,     7,    35,    -1,    37,    -1,
      12,    13,    41,    42,    65,    -1,    67,    68,    69,    70,
      22,    -1,     8,     9,    -1,    -1,    -1,    -1,    14,    15,
      16,    17,    18,    35,    -1,    37,    38,    39,    -1,    41,
      42,    -1,    -1,    45,    -1,    -1,    48,    -1,     1,    -1,
      52,    -1,    -1,     6,     7,    -1,    -1,    -1,    -1,    12,
      13,    -1,    -1,    65,    -1,    67,    68,    69,    70,    22,
      56,    57,    58,    -1,    60,    61,    62,    63,    64,    65,
      -1,    -1,    35,    69,    37,    38,    39,    -1,    41,    42,
      -1,    -1,    45,    -1,    -1,    48,    -1,     1,    -1,    52,
      -1,    -1,     6,     7,    -1,    -1,    -1,    -1,    12,    13,
      -1,    -1,    65,    -1,    67,    68,    69,    70,    22,    -1,
      -1,    -1,    15,    16,    17,    18,    -1,    -1,    -1,    -1,
      -1,    35,    -1,    37,    38,    39,    -1,    41,    42,    -1,
      -1,    45,    -1,    -1,    48,    -1,    -1,    -1,    52,    -1,
      -1,     3,     4,     5,     6,    -1,    -1,     3,     4,     5,
       6,    65,    -1,    67,    68,    69,    70,    19,    20,    62,
      63,    64,    65,    19,    20,    -1,     8,    -1,    -1,    -1,
      -1,    33,    34,    15,    16,    17,    18,    33,    34,    -1,
      -1,    43,    -1,    -1,    -1,    -1,    -1,    43,    50,    -1,
      52,    53,    54,    -1,    50,    -1,    52,    53,    54,    -1,
      62,    63,    64,    65,    -1,    67,    62,    63,    64,    65,
      72,    67,     3,     4,     5,     6,    72,    -1,    60,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    -1,    19,    20,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     8,     9,
      -1,    -1,    33,    34,    14,    15,    16,    17,    18,    -1,
      -1,    -1,    43,    -1,    -1,    -1,    -1,    -1,    -1,    50,
      -1,    52,    53,    54,    -1,    -1,     3,     4,     5,     6,
       7,    62,    63,    64,    65,    12,    67,    -1,    -1,    -1,
      -1,    72,    19,    20,    -1,    22,    56,    57,    58,    -1,
      60,    61,    62,    63,    64,    65,    33,    34,    35,    -1,
      37,    -1,    72,    -1,    41,    42,    43,     3,     4,     5,
       6,    -1,     8,    50,    -1,    52,    53,    54,    -1,    -1,
      -1,    -1,    -1,    19,    20,    62,    63,    64,    65,    -1,
      67,    -1,    -1,    -1,    71,    -1,    -1,    33,    34,    -1,
      -1,    -1,    -1,     3,     4,     5,     6,    43,     8,    -1,
      -1,    -1,    -1,    -1,    50,    -1,    52,    53,    54,    19,
      20,    -1,    -1,    -1,    -1,    -1,    62,    63,    64,    65,
      -1,    67,    -1,    33,    34,    71,    -1,    -1,    -1,     3,
       4,     5,     6,    43,     3,     4,     5,     6,    -1,    -1,
      50,    -1,    52,    53,    54,    19,    20,    -1,    -1,    -1,
      19,    20,    62,    63,    64,    65,    -1,    67,    -1,    33,
      34,    71,    -1,    -1,    33,    34,    -1,    -1,    -1,    43,
       3,     4,     5,     6,    43,    -1,    50,    -1,    52,    53,
      54,    50,    -1,    52,    53,    54,    19,    20,    62,    63,
      64,    65,    -1,    67,    -1,    -1,    -1,    71,    67,    -1,
      33,    34,    71,    -1,    -1,    -1,     3,     4,     5,     6,
      43,    -1,    -1,    -1,    -1,    -1,    -1,    50,    -1,    52,
      53,    54,    19,    20,    -1,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    -1,    67,    68,    33,    34,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,
      -1,    -1,    -1,    50,    -1,    52,    53,    54,    -1,    -1,
       3,     4,     5,     6,     7,    62,    63,    64,    65,    12,
      67,    68,    -1,    -1,    -1,    -1,    19,    20,    -1,    22,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,    -1,
      33,    34,    35,    -1,    37,    -1,    -1,    -1,    41,    42,
      43,    19,    20,    -1,    -1,    -1,    -1,    50,    -1,    52,
      53,    54,    -1,    -1,    -1,    33,    34,    -1,    -1,    62,
      63,    64,    65,    -1,    67,    43,     3,     4,     5,     6,
      -1,     8,    50,    -1,    52,    53,    54,    -1,    -1,    -1,
      -1,    -1,    19,    20,    62,    63,    64,    65,    66,    67,
      -1,     3,     4,     5,     6,    -1,    33,    34,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    43,    19,    20,    -1,
      -1,    -1,    -1,    50,    -1,    52,    53,    54,    -1,    -1,
      -1,    33,    34,    -1,    -1,    62,    63,    64,    65,    -1,
      67,    43,     3,     4,     5,     6,    -1,    -1,    50,    -1,
      52,    53,    54,    -1,    -1,    -1,    -1,    59,    19,    20,
      62,    63,    64,    65,    -1,    67,    -1,     3,     4,     5,
       6,    -1,    33,    34,    15,    16,    17,    18,    -1,    -1,
      -1,    -1,    43,    19,    20,    -1,    -1,    -1,    -1,    50,
      -1,    52,    53,    54,    -1,    -1,    -1,    33,    34,    -1,
      -1,    62,    63,    64,    65,    -1,    67,    43,    -1,    -1,
      -1,    -1,    -1,    -1,    50,    -1,    52,    53,    54,    60,
      61,    62,    63,    64,    65,    -1,    62,    63,    64,    65,
      -1,    67,     8,     9,    -1,    -1,    -1,    -1,    14,    15,
      16,    17,    18,    -1,    39,     8,     9,    42,    -1,    -1,
      -1,    14,    15,    16,    17,    18,    -1,    -1,     8,     9,
      36,    56,    57,    -1,    14,    15,    16,    17,    18,    -1,
      -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,
      56,    57,    58,    -1,    60,    61,    62,    63,    64,    65,
      -1,    -1,    -1,    56,    57,    58,    72,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    56,    57,    58,    72,
      60,    61,    62,    63,    64,    65,    -1,     8,     9,    -1,
      -1,    -1,    72,    14,    15,    16,    17,    18,    -1,    -1,
       8,     9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,
      18,    -1,    -1,     8,     9,    -1,    -1,    -1,    -1,    14,
      15,    16,    17,    18,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    56,    57,    58,    -1,    60,
      61,    62,    63,    64,    65,    -1,    -1,    -1,    56,    57,
      58,    72,    60,    61,    62,    63,    64,    65,    -1,    -1,
      -1,    56,    57,    58,    72,    60,    61,    62,    63,    64,
      65,    -1,     8,     9,    -1,    -1,    -1,    72,    14,    15,
      16,    17,    18,     6,     7,    -1,    -1,    -1,    -1,    12,
      13,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    22,
      -1,     8,     9,    -1,    -1,    -1,    -1,    -1,    15,    16,
      17,    18,    35,    -1,    37,    38,    -1,    -1,    41,    42,
      56,    57,    58,    -1,    60,    61,    62,    63,    64,    65,
      -1,    -1,    -1,     7,    -1,    -1,    72,    -1,    12,    13,
      -1,    -1,    -1,     6,     7,    -1,    -1,    70,    22,    12,
      13,    58,    -1,    60,    61,    62,    63,    64,    65,    22,
      -1,    35,    -1,    37,    38,    -1,    -1,    41,    42,    -1,
      -1,    -1,    35,    -1,    37,    38,     8,     9,    41,    42,
      -1,    -1,    14,    15,    16,    17,    18,    -1,    -1,    -1,
      -1,    -1,     8,     9,    -1,    -1,    -1,    71,    14,    15,
      16,    17,    18,    -1,    -1,    -1,    69,     8,     9,    -1,
      -1,    -1,    -1,    14,    15,    16,    17,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    56,    57,    58,    -1,    60,    61,
      62,    63,    64,    65,    -1,    -1,    -1,    69,    -1,    -1,
      56,    57,    58,    -1,    60,    61,    62,    63,    64,    65,
      -1,    -1,    -1,    69,    -1,    56,    57,    58,    -1,    60,
      61,    62,    63,    64,    65,     8,     9,    -1,    69,    -1,
      -1,    14,    15,    16,    17,    18,     7,    -1,    -1,     8,
       9,    12,    13,    -1,    -1,    14,    15,    16,    17,    18,
      -1,    22,    -1,     8,     9,    -1,    -1,    -1,    -1,    14,
      15,    16,    17,    18,    35,    36,    37,    38,    -1,    -1,
      41,    42,    -1,    56,    57,    58,    -1,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    69,    56,    57,    58,
      -1,    60,    61,    62,    63,    64,    65,    -1,    -1,    -1,
      69,    56,    57,    58,    -1,    60,    61,    62,    63,    64,
      65,     8,     9,    -1,    69,    -1,    -1,    14,    15,    16,
      17,    18,     7,    -1,    -1,     8,     9,    12,    13,    -1,
      -1,    14,    15,    16,    17,    18,    -1,    22,     8,     9,
      -1,    -1,    -1,    -1,    14,    15,    16,    17,    18,    -1,
      35,    -1,    37,    38,    -1,    -1,    41,    42,    -1,    56,
      57,    58,    -1,    60,    61,    62,    63,    64,    65,    -1,
      -1,    -1,    69,    56,    57,    58,    -1,    60,    61,    62,
      63,    64,    65,    -1,    -1,    68,    56,    57,    58,    -1,
      60,    61,    62,    63,    64,    65,     8,     9,    68,    -1,
      -1,    -1,    14,    15,    16,    17,    18,    -1,    -1,     8,
       9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,    18,
      -1,    -1,    -1,    -1,    -1,    -1,     8,     9,    -1,    -1,
      -1,    -1,    14,    15,    16,    17,    18,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    56,    57,    58,    -1,    60,    61,
      62,    63,    64,    65,    36,    -1,    68,    56,    57,    58,
      -1,    60,    61,    62,    63,    64,    65,    -1,    -1,    68,
      -1,    -1,    -1,    -1,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,     8,     9,    -1,    -1,    -1,    -1,
      14,    15,    16,    17,    18,     8,     9,    -1,    -1,    -1,
      -1,    14,    15,    16,    17,    18,     8,     9,    -1,    -1,
      -1,    -1,    14,    15,    16,    17,    18,    -1,    -1,     8,
       9,    -1,    -1,    -1,    -1,    14,    15,    16,    17,    18,
      -1,    -1,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    -1,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    42,    56,    57,    58,    -1,    60,    61,
      62,    63,    64,    65,    15,    16,    17,    18,    57,    58,
      -1,    60,    61,    62,    63,    64,    65,     8,     9,    -1,
      -1,    -1,    -1,    14,    15,    16,    17,    18,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      61,    62,    63,    64,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,    60,
      61,    62,    63,    64,    65
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     6,     7,    12,    13,    22,    35,    37,    38,
      39,    41,    42,    45,    48,    52,    65,    67,    68,    69,
      74,    75,    76,    77,    79,    80,    82,    87,    88,    92,
      94,    95,    96,    97,    98,   102,   105,   118,   121,   122,
     125,   128,   150,   170,     3,   102,   126,   127,     6,    37,
     151,    67,    67,     6,    89,    45,    49,    81,   170,    37,
     102,   105,   106,   105,   127,    40,     0,    76,    13,    78,
      94,   113,   114,    68,   105,   119,   137,   138,    95,    95,
      66,    67,    71,    71,     6,    93,     6,   170,   102,    67,
       3,     4,     5,     6,    19,    20,    33,    34,    43,    50,
      53,    54,    62,    63,    64,    65,    67,    96,   132,   133,
     166,   169,   170,   173,   174,    67,    57,   126,    46,    86,
      86,    37,   102,   105,    69,   105,    71,   147,   148,   105,
     115,   114,    39,   126,    56,    68,    57,    13,    65,    72,
     166,     6,    69,    94,   107,   108,   109,   110,     6,     7,
     123,   124,     6,    70,    94,   129,   130,    71,    94,   118,
       6,   173,    67,   169,   169,   171,   171,    67,    15,     7,
      12,    37,    67,    97,    99,   100,   101,   121,   125,    66,
     169,   169,   169,   169,   169,   148,   166,   167,   174,   133,
      65,    66,    67,   111,     8,     9,    14,    15,    16,    17,
      18,    56,    57,    58,    60,    61,    62,    63,    64,    65,
      69,    19,    21,    66,    67,     6,     3,    69,     6,   103,
     104,    83,    86,    71,    91,     6,     7,    84,    85,    84,
      69,     1,     6,     7,    10,    11,    23,    25,    26,    27,
      28,    29,    30,    31,    32,    68,    70,   116,   117,   118,
     147,   149,   150,   156,   157,   158,   159,   160,   161,   163,
     166,   126,    56,    68,    67,   126,     8,    71,   139,   166,
     168,   166,    72,    72,    65,    67,   105,   111,    56,    69,
      69,    56,    62,    57,    57,    56,   145,    59,    59,   105,
     134,   136,    70,   130,    70,    75,   105,    59,    69,   152,
     165,   166,    67,   169,    67,   174,   174,   174,    99,    99,
      72,    69,   116,    69,   106,   111,    72,   166,    69,   111,
     112,    66,    67,   166,   166,   166,   166,   166,   166,   166,
     166,   166,   168,    59,   166,   166,   166,   166,   166,   166,
     166,     6,     7,   166,   165,    67,    69,    56,    68,    84,
      75,    90,    68,    46,    68,    68,    70,    59,    59,     6,
      65,    68,   166,    67,    67,    68,    68,    67,    67,    59,
     166,    70,   117,    68,     7,   149,   149,   149,   149,   149,
     164,   166,   149,    68,   105,   173,   120,     6,    21,    44,
      66,    70,   141,   142,   143,   144,    72,   106,   126,     6,
      36,   110,   166,   166,   124,    70,   166,    59,   126,    56,
      68,   131,    70,   153,   154,   173,    69,    56,    69,   174,
     174,    56,    15,    69,   169,    70,   166,    71,   169,   172,
     111,   126,    72,    69,   108,    72,   166,    69,   112,   166,
      59,    72,    69,   165,    69,   103,    70,    47,    84,   126,
     126,    68,   166,    68,   166,   166,   166,   162,   164,    36,
      59,    28,    24,    69,   126,    69,   138,     6,     7,   166,
      56,   145,    71,   140,   146,   166,   168,    57,    66,   144,
     166,   135,    68,    59,    56,    67,   166,   174,    69,    69,
     169,    67,    69,    68,    70,   141,   126,    69,   126,    72,
     108,   166,    69,    84,    68,    69,    69,    69,   118,    68,
     166,    67,   149,   126,    36,    72,   142,    70,    70,   141,
     166,   127,   136,   153,   173,   166,    69,   166,    70,   145,
     126,    69,   164,   164,    59,   166,   166,   140,   145,    59,
      67,    69,    69,    69,    70,    68,    68,    69,    72,    70,
     155,   173,   166,    68,    57,    56,    69,   173
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,    73,    74,    74,    75,    75,    76,    76,    76,    76,
      76,    76,    76,    77,    77,    78,    78,    79,    79,    79,
      79,    80,    80,    81,    81,    82,    83,    84,    84,    84,
      85,    85,    86,    86,    87,    88,    89,    89,    90,    90,
      91,    92,    92,    92,    93,    94,    95,    95,    95,    95,
      96,    96,    96,    96,    96,    96,    96,    97,    97,    98,
      98,    99,    99,    99,    99,   100,   101,   101,   101,   101,
     101,   102,   103,   103,   104,   104,   104,   105,   105,   105,
     105,   105,   105,   105,   105,   105,   105,   105,   105,   105,
     106,   106,   106,   106,   107,   107,   108,   108,   109,   109,
     109,   110,   110,   110,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   112,   113,   113,
     114,   115,   115,   116,   116,   117,   117,   118,   118,   119,
     120,   119,   121,   121,   122,   122,   123,   123,   124,   124,
     124,   124,   125,   125,   125,   125,   126,   126,   127,   127,
     128,   128,   129,   129,   130,   130,   131,   131,   132,   133,
     133,   134,   135,   134,   136,   136,   136,   136,   136,   137,
     137,   138,   138,   138,   138,   138,   138,   139,   140,   140,
     140,   140,   141,   141,   142,   142,   142,   142,   143,   143,
     144,   144,   144,   145,   145,   146,   147,   147,   148,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   150,   150,
     151,   151,   152,   152,   152,   153,   153,   154,   154,   155,
     155,   156,   156,   156,   156,   156,   157,   158,   159,   160,
     161,   162,   161,   163,   164,   164,   165,   165,   165,   165,
     166,   166,   166,   166,   166,   166,   166,   166,   166,   166,
     166,   166,   166,   166,   166,   166,   166,   166,   166,   167,
     168,   169,   169,   169,   169,   169,   169,   169,   169,   169,
     169,   169,   169,   169,   169,   169,   169,   169,   169,   169,
     169,   169,   169,   169,   169,   169,   169,   169,   169,   169,
     169,   169,   169,   170,   170,   171,   172,   173,   173,   174,
     174
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     2,     0,     1,     1,     1,     1,
       1,     5,     5,     1,     0,     5,     2,     1,     3,     4,
       1,     1,     1,     0,     2,     6,     1,     0,     1,     0,
       1,     6,     5,     4,     0,     1,     1,     2,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     4,
       4,     1,     2,     1,     2,     1,     1,     1,     1,     1,
       1,     6,     1,     3,     0,     1,     4,     2,     3,     2,
       1,     4,     3,     4,     5,     3,     4,     4,     4,     3,
       1,     2,     1,     2,     1,     3,     1,     3,     1,     2,
       3,     3,     2,     1,     1,     2,     2,     3,     3,     3,
       4,     4,     5,     2,     4,     3,     5,     0,     1,     2,
       3,     2,     4,     1,     2,     1,     1,     2,     3,     1,
       0,     5,     5,     2,     1,     2,     1,     3,     1,     1,
       3,     3,     4,     3,     4,     3,     0,     1,     1,     2,
       2,     3,     1,     2,     3,     2,     1,     2,     1,     2,
       1,     1,     0,     4,     2,     2,     3,     4,     0,     2,
       5,     2,     6,     3,     6,     4,     3,     1,     1,     1,
       4,     2,     2,     4,     2,     1,     6,     0,     1,     2,
       3,     2,     2,     0,     1,     1,     3,     2,     1,     2,
       1,     2,     2,     2,     7,     4,     2,     2,     2,     2,
       3,     3,     4,     2,     1,     2,     2,     2,     5,     6,
       0,     1,     2,     4,     6,     0,     1,     4,     6,     1,
       3,     3,     3,     3,     5,     2,     1,     4,     3,     4,
       6,     0,     6,     4,     1,     0,     0,     1,     3,     3,
       3,     3,     3,     5,     4,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     1,     1,
       2,     2,     2,     2,     2,     2,     7,     2,     2,     3,
       4,     5,     5,     7,     5,     4,     4,     4,     3,     3,
       1,     2,     6,     1,     1,     1,     3,     7,     5,     2,
       4,     2,     4,     1,     2,     0,     1,     1,     2,     1,
       2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 3: /* edf: %empty  */
#line 264 "./cgram.y"
                  { ftnend(); }
#line 2271 "y.tab.c"
    break;

  case 6: /* external_def: funtype kr_args compoundstmt  */
#line 271 "./cgram.y"
                                                { fend((yyvsp[-2].symp)); }
#line 2277 "y.tab.c"
    break;

  case 7: /* external_def: declaration  */
#line 272 "./cgram.y"
                                { blevel = 0; symclear(0); }
#line 2283 "y.tab.c"
    break;

  case 12: /* external_def: error  */
#line 277 "./cgram.y"
                         { blevel = 0; }
#line 2289 "y.tab.c"
    break;

  case 13: /* funtype: declarator  */
#line 280 "./cgram.y"
                                                 {
		    (yyval.symp) = fundef(mkty(INT, 0, 0), (yyvsp[0].nodep));
		    cftnsp->sflags |= NORETYP;
		}
#line 2298 "y.tab.c"
    break;

  case 14: /* funtype: declaration_specifiers declarator  */
#line 284 "./cgram.y"
                                                    { (yyval.symp) = fundef((yyvsp[-1].nodep),(yyvsp[0].nodep)); }
#line 2304 "y.tab.c"
    break;

  case 21: /* using_x: CXX_USING tnopt ccopt nested_name_sp ';'  */
#line 297 "./cgram.y"
                                                           { werror("using"); }
#line 2310 "y.tab.c"
    break;

  case 22: /* using_x: CXX_USING CXX_NAMESPACE ccopt nested_name_sp ';'  */
#line 298 "./cgram.y"
                                                                   { werror("using2"); }
#line 2316 "y.tab.c"
    break;

  case 23: /* tnopt: CXX_TYPENAME  */
#line 301 "./cgram.y"
                               { }
#line 2322 "y.tab.c"
    break;

  case 24: /* tnopt: %empty  */
#line 302 "./cgram.y"
                  { }
#line 2328 "y.tab.c"
    break;

  case 25: /* ns_alias: CXX_NAMESPACE C_NAME '=' qual_ns_sp ';'  */
#line 305 "./cgram.y"
                                                          { werror("ns_alias");}
#line 2334 "y.tab.c"
    break;

  case 33: /* ccopt: %empty  */
#line 321 "./cgram.y"
                  { }
#line 2340 "y.tab.c"
    break;

  case 34: /* simple_decl: ')' NOMATCH  */
#line 324 "./cgram.y"
                              { uerror("simple-declaration"); }
#line 2346 "y.tab.c"
    break;

  case 35: /* namespace: CXX_NAMESPACE nsname attr_var nsbeg ns_body '}'  */
#line 327 "./cgram.y"
                                                                  { POPSYM(); }
#line 2352 "y.tab.c"
    break;

  case 37: /* nsname: %empty  */
#line 331 "./cgram.y"
                  { (yyval.strp) = NULL; }
#line 2358 "y.tab.c"
    break;

  case 39: /* ns_body: %empty  */
#line 335 "./cgram.y"
                  { }
#line 2364 "y.tab.c"
    break;

  case 40: /* nsbeg: '{'  */
#line 338 "./cgram.y"
                      { dclns((yyvsp[-1].nodep), (yyvsp[(-1) - (1)].strp)); }
#line 2370 "y.tab.c"
    break;

  case 41: /* extlink: C_CLASS C_STRING eb '{' ext_def_list '}'  */
#line 341 "./cgram.y"
                                                           { elnk = LINK_DEF; }
#line 2376 "y.tab.c"
    break;

  case 42: /* extlink: C_CLASS C_STRING eb '{' '}'  */
#line 342 "./cgram.y"
                                              { elnk = LINK_DEF; }
#line 2382 "y.tab.c"
    break;

  case 43: /* extlink: C_CLASS C_STRING eb declaration  */
#line 343 "./cgram.y"
                                                  { elnk = LINK_DEF; }
#line 2388 "y.tab.c"
    break;

  case 44: /* eb: %empty  */
#line 346 "./cgram.y"
                  {
			NODE *p = (yyvsp[(-1) - (0)].nodep);
			char *s = (yyvsp[0].strp);
			if (p->n_type != EXTERN)
				uerror("'extern' expected");
			if (strcmp(s, "\"C\"") != 0 && strcmp(s, "\"c\""))
				uerror("unknown linkage %s", s);
			nfree(p);
			elnk = LINK_C;
		}
#line 2403 "y.tab.c"
    break;

  case 45: /* declaration_specifiers: merge_attribs  */
#line 363 "./cgram.y"
                                 { (yyval.nodep) = typenode((yyvsp[0].nodep)); }
#line 2409 "y.tab.c"
    break;

  case 46: /* merge_attribs: type_sq  */
#line 366 "./cgram.y"
                           { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2415 "y.tab.c"
    break;

  case 47: /* merge_attribs: type_sq merge_attribs  */
#line 367 "./cgram.y"
                                         { (yyval.nodep) = cmop((yyvsp[0].nodep), (yyvsp[-1].nodep)); }
#line 2421 "y.tab.c"
    break;

  case 48: /* merge_attribs: cf_spec  */
#line 368 "./cgram.y"
                           { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2427 "y.tab.c"
    break;

  case 49: /* merge_attribs: cf_spec merge_attribs  */
#line 369 "./cgram.y"
                                         { (yyval.nodep) = cmop((yyvsp[0].nodep), (yyvsp[-1].nodep)); }
#line 2433 "y.tab.c"
    break;

  case 50: /* type_sq: C_TYPE  */
#line 372 "./cgram.y"
                          { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2439 "y.tab.c"
    break;

  case 51: /* type_sq: C_TYPENAME  */
#line 373 "./cgram.y"
                              { 
			struct symtab *sp = lookup((yyvsp[0].strp), 0);
			if (sp->stype == ENUMTY) {
				sp->stype = strmemb(sp->sap)->stype;
			}
			(yyval.nodep) = mkty(sp->stype, sp->sdf, sp->sap);
			(yyval.nodep)->n_sp = sp;
		}
#line 2452 "y.tab.c"
    break;

  case 52: /* type_sq: struct_dcl  */
#line 381 "./cgram.y"
                              { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2458 "y.tab.c"
    break;

  case 53: /* type_sq: enum_dcl  */
#line 382 "./cgram.y"
                            { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2464 "y.tab.c"
    break;

  case 54: /* type_sq: C_QUALIFIER  */
#line 383 "./cgram.y"
                               { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2470 "y.tab.c"
    break;

  case 55: /* type_sq: attribute_specifier  */
#line 384 "./cgram.y"
                                       { (yyval.nodep) = biop(ATTRIB, (yyvsp[0].nodep), 0); }
#line 2476 "y.tab.c"
    break;

  case 56: /* type_sq: typeof  */
#line 385 "./cgram.y"
                          { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2482 "y.tab.c"
    break;

  case 57: /* cf_spec: C_CLASS  */
#line 388 "./cgram.y"
                           { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2488 "y.tab.c"
    break;

  case 58: /* cf_spec: C_FUNSPEC  */
#line 389 "./cgram.y"
                             { fun_inline = 1;  /* XXX - hack */
			(yyval.nodep) = block(QUALIFIER, NIL, NIL, 0, 0, 0); }
#line 2495 "y.tab.c"
    break;

  case 59: /* typeof: C_TYPEOF '(' e ')'  */
#line 393 "./cgram.y"
                                      { (yyval.nodep) = tyof(eve((yyvsp[-1].nodep))); }
#line 2501 "y.tab.c"
    break;

  case 60: /* typeof: C_TYPEOF '(' cast_type ')'  */
#line 394 "./cgram.y"
                                              { TYMFIX((yyvsp[-1].nodep)); (yyval.nodep) = tyof((yyvsp[-1].nodep)); }
#line 2507 "y.tab.c"
    break;

  case 62: /* new_ma: new_type_sq new_ma  */
#line 398 "./cgram.y"
                                      { (yyval.nodep) = cmop((yyvsp[0].nodep), (yyvsp[-1].nodep)); }
#line 2513 "y.tab.c"
    break;

  case 63: /* new_ma: cf_spec  */
#line 399 "./cgram.y"
                           { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2519 "y.tab.c"
    break;

  case 64: /* new_ma: cf_spec new_ma  */
#line 400 "./cgram.y"
                                  { (yyval.nodep) = cmop((yyvsp[0].nodep), (yyvsp[-1].nodep)); }
#line 2525 "y.tab.c"
    break;

  case 65: /* new_ds: new_ma  */
#line 403 "./cgram.y"
                          { (yyval.nodep) = typenode((yyvsp[0].nodep)); }
#line 2531 "y.tab.c"
    break;

  case 66: /* new_type_sq: C_TYPE  */
#line 406 "./cgram.y"
                          { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2537 "y.tab.c"
    break;

  case 67: /* new_type_sq: C_TYPENAME  */
#line 407 "./cgram.y"
                              { 
			struct symtab *sp = lookup((yyvsp[0].strp), 0);
			if (sp->stype == ENUMTY) {
				sp->stype = strmemb(sp->sap)->stype;
			}
			(yyval.nodep) = mkty(sp->stype, sp->sdf, sp->sap);
			(yyval.nodep)->n_sp = sp;
		}
#line 2550 "y.tab.c"
    break;

  case 68: /* new_type_sq: struct_dcl  */
#line 415 "./cgram.y"
                              { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2556 "y.tab.c"
    break;

  case 69: /* new_type_sq: enum_dcl  */
#line 416 "./cgram.y"
                            { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2562 "y.tab.c"
    break;

  case 70: /* new_type_sq: C_QUALIFIER  */
#line 417 "./cgram.y"
                               { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2568 "y.tab.c"
    break;

  case 71: /* attribute_specifier: C_ATTRIBUTE '(' '(' attribute_list ')' ')'  */
#line 421 "./cgram.y"
                                                              { (yyval.nodep) = (yyvsp[-2].nodep); }
#line 2574 "y.tab.c"
    break;

  case 73: /* attribute_list: attribute ',' attribute_list  */
#line 425 "./cgram.y"
                                                { (yyval.nodep) = cmop((yyvsp[0].nodep), (yyvsp[-2].nodep)); }
#line 2580 "y.tab.c"
    break;

  case 74: /* attribute: %empty  */
#line 428 "./cgram.y"
                   {
#ifdef GCC_COMPAT
			 (yyval.nodep) = voidcon();
#endif
		}
#line 2590 "y.tab.c"
    break;

  case 75: /* attribute: C_NAME  */
#line 433 "./cgram.y"
                          { (yyval.nodep) = bdty(NAME, (yyvsp[0].strp)); }
#line 2596 "y.tab.c"
    break;

  case 76: /* attribute: C_NAME '(' elist ')'  */
#line 434 "./cgram.y"
                                        {
			(yyval.nodep) = bdty((yyvsp[-1].nodep) == NIL ? UCALL : CALL, bdty(NAME, (yyvsp[-3].strp)), (yyvsp[-1].nodep));
		}
#line 2604 "y.tab.c"
    break;

  case 77: /* declarator: '*' declarator  */
#line 442 "./cgram.y"
                                  { (yyval.nodep) = bdty(UMUL, (yyvsp[0].nodep)); }
#line 2610 "y.tab.c"
    break;

  case 78: /* declarator: '*' type_qualifier_list declarator  */
#line 443 "./cgram.y"
                                                      {
			(yyval.nodep) = (yyvsp[-1].nodep);
			(yyval.nodep)->n_left = (yyvsp[0].nodep);
		}
#line 2619 "y.tab.c"
    break;

  case 79: /* declarator: nmrec C_NAME  */
#line 447 "./cgram.y"
                                { (yyval.nodep) = biop(NMLIST, (yyvsp[-1].nodep), bdty(NAME, (yyvsp[0].strp))); }
#line 2625 "y.tab.c"
    break;

  case 80: /* declarator: C_NAME  */
#line 448 "./cgram.y"
                          { (yyval.nodep) = bdty(NAME, (yyvsp[0].strp)); }
#line 2631 "y.tab.c"
    break;

  case 81: /* declarator: '(' attr_spec_list declarator ')'  */
#line 449 "./cgram.y"
                                                     {
			(yyval.nodep) = (yyvsp[-1].nodep);
			(yyval.nodep)->n_ap = attr_add((yyval.nodep)->n_ap, gcc_attr_wrapper((yyvsp[-2].nodep)));
		}
#line 2640 "y.tab.c"
    break;

  case 82: /* declarator: '(' declarator ')'  */
#line 453 "./cgram.y"
                                      { (yyval.nodep) = (yyvsp[-1].nodep); }
#line 2646 "y.tab.c"
    break;

  case 83: /* declarator: declarator '[' e ']'  */
#line 454 "./cgram.y"
                                        { (yyval.nodep) = biop(LB, (yyvsp[-3].nodep), (yyvsp[-1].nodep)); }
#line 2652 "y.tab.c"
    break;

  case 84: /* declarator: declarator '[' C_CLASS e ']'  */
#line 455 "./cgram.y"
                                                {
			if ((yyvsp[-2].nodep)->n_type != STATIC)
				uerror("bad class keyword");
			tfree((yyvsp[-2].nodep)); /* XXX - handle */
			(yyval.nodep) = biop(LB, (yyvsp[-4].nodep), (yyvsp[-1].nodep));
		}
#line 2663 "y.tab.c"
    break;

  case 85: /* declarator: declarator '[' ']'  */
#line 461 "./cgram.y"
                                      { (yyval.nodep) = biop(LB, (yyvsp[-2].nodep), bcon(NOOFFSET)); }
#line 2669 "y.tab.c"
    break;

  case 86: /* declarator: declarator '[' '*' ']'  */
#line 462 "./cgram.y"
                                          { (yyval.nodep) = biop(LB, (yyvsp[-3].nodep), bcon(NOOFFSET)); }
#line 2675 "y.tab.c"
    break;

  case 87: /* declarator: declarator '(' parameter_type_list ')'  */
#line 463 "./cgram.y"
                                                          {
			(yyval.nodep) = bdty(CALL, (yyvsp[-3].nodep), (yyvsp[-1].nodep));
		}
#line 2683 "y.tab.c"
    break;

  case 88: /* declarator: declarator '(' identifier_list ')'  */
#line 466 "./cgram.y"
                                                      {
			(yyval.nodep) = bdty(CALL, (yyvsp[-3].nodep), (yyvsp[-1].nodep));
			oldstyle = 1;
		}
#line 2692 "y.tab.c"
    break;

  case 89: /* declarator: declarator '(' ')'  */
#line 470 "./cgram.y"
                                      { (yyval.nodep) = bdty(UCALL, (yyvsp[-2].nodep)); }
#line 2698 "y.tab.c"
    break;

  case 90: /* type_qualifier_list: C_QUALIFIER  */
#line 474 "./cgram.y"
                               { (yyval.nodep) = (yyvsp[0].nodep); (yyval.nodep)->n_op = UMUL; }
#line 2704 "y.tab.c"
    break;

  case 91: /* type_qualifier_list: type_qualifier_list C_QUALIFIER  */
#line 475 "./cgram.y"
                                                   {
			(yyval.nodep) = (yyvsp[-1].nodep);
			(yyval.nodep)->n_qual |= (yyvsp[0].nodep)->n_qual;
			nfree((yyvsp[0].nodep));
		}
#line 2714 "y.tab.c"
    break;

  case 92: /* type_qualifier_list: attribute_specifier  */
#line 480 "./cgram.y"
                                       {
			(yyval.nodep) = block(UMUL, NIL, NIL, 0, 0, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2722 "y.tab.c"
    break;

  case 93: /* type_qualifier_list: type_qualifier_list attribute_specifier  */
#line 483 "./cgram.y"
                                                           {
			(yyvsp[-1].nodep)->n_ap = attr_add((yyvsp[-1].nodep)->n_ap, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2730 "y.tab.c"
    break;

  case 94: /* identifier_list: C_NAME  */
#line 488 "./cgram.y"
                          { (yyval.nodep) = bdty(NAME, (yyvsp[0].strp)); oldargs((yyval.nodep)); }
#line 2736 "y.tab.c"
    break;

  case 95: /* identifier_list: identifier_list ',' C_NAME  */
#line 489 "./cgram.y"
                                              {
			(yyval.nodep) = cmop((yyvsp[-2].nodep), bdty(NAME, (yyvsp[0].strp)));
			oldargs((yyval.nodep)->n_right);
		}
#line 2745 "y.tab.c"
    break;

  case 96: /* parameter_type_list: parameter_list  */
#line 499 "./cgram.y"
                                  { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2751 "y.tab.c"
    break;

  case 97: /* parameter_type_list: parameter_list ',' C_ELLIPSIS  */
#line 500 "./cgram.y"
                                                 {
			(yyval.nodep) = cmop((yyvsp[-2].nodep), biop(ELLIPSIS, NIL, NIL));
		}
#line 2759 "y.tab.c"
    break;

  case 98: /* parameter_list: parameter_declaration  */
#line 510 "./cgram.y"
                                         { (yyval.nodep) = (yyvsp[0].nodep); }
#line 2765 "y.tab.c"
    break;

  case 99: /* parameter_list: parameter_declaration '&'  */
#line 511 "./cgram.y"
                                             { (yyval.nodep) = (yyvsp[-1].nodep); }
#line 2771 "y.tab.c"
    break;

  case 100: /* parameter_list: parameter_list ',' parameter_declaration  */
#line 512 "./cgram.y"
                                                            {
			(yyval.nodep) = cmop((yyvsp[-2].nodep), (yyvsp[0].nodep));
		}
#line 2779 "y.tab.c"
    break;

  case 101: /* parameter_declaration: declaration_specifiers declarator attr_var  */
#line 521 "./cgram.y"
                                                              {
			if (glval((yyvsp[-2].nodep)) != SNULL && glval((yyvsp[-2].nodep)) != REGISTER)
				uerror("illegal parameter class");
			(yyval.nodep) = block(TYMERGE, (yyvsp[-2].nodep), (yyvsp[-1].nodep), INT, 0, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2789 "y.tab.c"
    break;

  case 102: /* parameter_declaration: declaration_specifiers abstract_declarator  */
#line 526 "./cgram.y"
                                                              { 
			(yyval.nodep) = block(TYMERGE, (yyvsp[-1].nodep), (yyvsp[0].nodep), INT, 0, 0);
		}
#line 2797 "y.tab.c"
    break;

  case 103: /* parameter_declaration: declaration_specifiers  */
#line 529 "./cgram.y"
                                          {
			(yyval.nodep) = block(TYMERGE, (yyvsp[0].nodep), bdty(NAME, NULL), INT, 0, 0);
		}
#line 2805 "y.tab.c"
    break;

  case 104: /* abstract_declarator: '*'  */
#line 535 "./cgram.y"
                       { (yyval.nodep) = bdty(UMUL, bdty(NAME, NULL)); }
#line 2811 "y.tab.c"
    break;

  case 105: /* abstract_declarator: '*' type_qualifier_list  */
#line 536 "./cgram.y"
                                           {
			(yyval.nodep) = (yyvsp[0].nodep);
			(yyval.nodep)->n_left = bdty(NAME, NULL);
		}
#line 2820 "y.tab.c"
    break;

  case 106: /* abstract_declarator: '*' abstract_declarator  */
#line 540 "./cgram.y"
                                           { (yyval.nodep) = bdty(UMUL, (yyvsp[0].nodep)); }
#line 2826 "y.tab.c"
    break;

  case 107: /* abstract_declarator: '*' type_qualifier_list abstract_declarator  */
#line 541 "./cgram.y"
                                                               {
			(yyval.nodep) = (yyvsp[-1].nodep);
			(yyval.nodep)->n_left = (yyvsp[0].nodep);
		}
#line 2835 "y.tab.c"
    break;

  case 108: /* abstract_declarator: '(' abstract_declarator ')'  */
#line 545 "./cgram.y"
                                               { (yyval.nodep) = (yyvsp[-1].nodep); }
#line 2841 "y.tab.c"
    break;

  case 109: /* abstract_declarator: '[' ']' attr_var  */
#line 546 "./cgram.y"
                                    {
			(yyval.nodep) = block(LB, bdty(NAME, NULL), bcon(NOOFFSET),
			    INT, 0, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2850 "y.tab.c"
    break;

  case 110: /* abstract_declarator: '[' e ']' attr_var  */
#line 550 "./cgram.y"
                                      {
			(yyval.nodep) = block(LB, bdty(NAME, NULL), (yyvsp[-2].nodep),
			    INT, 0, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2859 "y.tab.c"
    break;

  case 111: /* abstract_declarator: abstract_declarator '[' ']' attr_var  */
#line 554 "./cgram.y"
                                                        {
			(yyval.nodep) = block(LB, (yyvsp[-3].nodep), bcon(NOOFFSET),
			    INT, 0, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2868 "y.tab.c"
    break;

  case 112: /* abstract_declarator: abstract_declarator '[' e ']' attr_var  */
#line 558 "./cgram.y"
                                                          {
			(yyval.nodep) = block(LB, (yyvsp[-4].nodep), (yyvsp[-2].nodep), INT, 0, gcc_attr_wrapper((yyvsp[0].nodep)));
		}
#line 2876 "y.tab.c"
    break;

  case 113: /* abstract_declarator: '(' ')'  */
#line 561 "./cgram.y"
                           { (yyval.nodep) = bdty(UCALL, bdty(NAME, NULL)); }
#line 2882 "y.tab.c"
    break;

  case 114: /* abstract_declarator: '(' ib2 parameter_type_list ')'  */
#line 562 "./cgram.y"
                                                   {
			(yyval.nodep) = bdty(CALL, bdty(NAME, NULL), (yyvsp[-1].nodep));
		}
#line 2890 "y.tab.c"
    break;

  case 115: /* abstract_declarator: abstract_declarator '(' ')'  */
#line 565 "./cgram.y"
                                               {
			(yyval.nodep) = bdty(UCALL, (yyvsp[-2].nodep));
		}
#line 2898 "y.tab.c"
    break;

  case 116: /* abstract_declarator: abstract_declarator '(' ib2 parameter_type_list ')'  */
#line 568 "./cgram.y"
                                                                       {
			(yyval.nodep) = bdty(CALL, (yyvsp[-4].nodep), (yyvsp[-1].nodep));
		}
#line 2906 "y.tab.c"
    break;

  case 117: /* ib2: %empty  */
#line 573 "./cgram.y"
                  { }
#line 2912 "y.tab.c"
    break;

  case 120: /* arg_declaration: declaration_specifiers arg_param_list ';'  */
#line 583 "./cgram.y"
                                                             {
			nfree((yyvsp[-2].nodep));
		}
#line 2920 "y.tab.c"
    break;

  case 121: /* arg_param_list: declarator attr_var  */
#line 588 "./cgram.y"
                                       {
			olddecl(block(TYMERGE, ccopy((yyvsp[-2].nodep)), (yyvsp[-1].nodep),
			    INT, 0, 0), (yyvsp[0].nodep));
		}
#line 2929 "y.tab.c"
    break;

  case 122: /* arg_param_list: arg_param_list ',' declarator attr_var  */
#line 592 "./cgram.y"
                                                          {
			olddecl(block(TYMERGE, ccopy((yyvsp[-4].nodep)), (yyvsp[-1].nodep),
			    INT, 0, 0), (yyvsp[0].nodep));
		}
#line 2938 "y.tab.c"
    break;

  case 127: /* declaration: declaration_specifiers ';'  */
#line 616 "./cgram.y"
                                              { tfree((yyvsp[-1].nodep)); fun_inline = 0; }
#line 2944 "y.tab.c"
    break;

  case 128: /* declaration: declaration_specifiers init_declarator_list ';'  */
#line 617 "./cgram.y"
                                                                   {
			tfree((yyvsp[-2].nodep));
			fun_inline = 0;
		}
#line 2953 "y.tab.c"
    break;

  case 129: /* init_declarator_list: init_declarator  */
#line 628 "./cgram.y"
                                   { symclear(blevel); }
#line 2959 "y.tab.c"
    break;

  case 130: /* @1: %empty  */
#line 629 "./cgram.y"
                                                     { (yyval.nodep) = (yyvsp[-3].nodep); }
#line 2965 "y.tab.c"
    break;

  case 131: /* init_declarator_list: init_declarator_list ',' attr_var @1 init_declarator  */
#line 629 "./cgram.y"
                                                                                                {
			uawarn((yyvsp[-2].nodep), "init_declarator");
			symclear(blevel);
		}
#line 2974 "y.tab.c"
    break;

  case 132: /* enum_dcl: enum_head '{' moe_list optcomma '}'  */
#line 635 "./cgram.y"
                                                       { (yyval.nodep) = enumdcl((yyvsp[-4].symp)); }
#line 2980 "y.tab.c"
    break;

  case 133: /* enum_dcl: C_ENUM C_NAME  */
#line 636 "./cgram.y"
                                 {  (yyval.nodep) = enumref((yyvsp[0].strp)); }
#line 2986 "y.tab.c"
    break;

  case 134: /* enum_head: C_ENUM  */
#line 639 "./cgram.y"
                          { (yyval.symp) = enumhd(NULL); }
#line 2992 "y.tab.c"
    break;

  case 135: /* enum_head: C_ENUM C_NAME  */
#line 640 "./cgram.y"
                                 {  (yyval.symp) = enumhd((yyvsp[0].strp)); }
#line 2998 "y.tab.c"
    break;

  case 138: /* moe: C_NAME  */
#line 647 "./cgram.y"
                          {  moedef((yyvsp[0].strp)); }
#line 3004 "y.tab.c"
    break;

  case 139: /* moe: C_TYPENAME  */
#line 648 "./cgram.y"
                              {  moedef((yyvsp[0].strp)); }
#line 3010 "y.tab.c"
    break;

  case 140: /* moe: C_NAME '=' e  */
#line 649 "./cgram.y"
                                { enummer = con_e((yyvsp[0].nodep)); moedef((yyvsp[-2].strp)); }
#line 3016 "y.tab.c"
    break;

  case 141: /* moe: C_TYPENAME '=' e  */
#line 650 "./cgram.y"
                                    { enummer = con_e((yyvsp[0].nodep)); moedef((yyvsp[-2].strp)); }
#line 3022 "y.tab.c"
    break;

  case 142: /* struct_dcl: str_head '{' struct_dcl_list '}'  */
#line 653 "./cgram.y"
                                                    {
			NODE *p;

			(yyval.nodep) = dclstruct((yyvsp[-3].rp));
			if (pragma_allpacked) {
				p = bdty(CALL, bdty(NAME, "packed"),
				    bcon(pragma_allpacked));
				(yyval.nodep)->n_ap = attr_add((yyval.nodep)->n_ap,gcc_attr_wrapper(p)); }
		}
#line 3036 "y.tab.c"
    break;

  case 143: /* struct_dcl: C_STRUCT attr_var C_NAME  */
#line 662 "./cgram.y"
                                            { 
			(yyval.nodep) = rstruct((yyvsp[0].strp),(yyvsp[-2].intval));
			uawarn((yyvsp[-1].nodep), "struct_dcl");
		}
#line 3045 "y.tab.c"
    break;

  case 144: /* struct_dcl: C_STRUCT attr_var nmrec C_NAME  */
#line 666 "./cgram.y"
                                                  { 
			(yyval.nodep) = cxxrstruct((yyvsp[-3].intval),(yyvsp[-2].nodep),(yyvsp[-1].nodep),(yyvsp[0].strp));
			uawarn((yyvsp[-2].nodep), "struct_dcl");
		}
#line 3054 "y.tab.c"
    break;

  case 145: /* struct_dcl: str_head '{' '}'  */
#line 670 "./cgram.y"
                                    { (yyval.nodep) = dclstruct((yyvsp[-2].rp)); }
#line 3060 "y.tab.c"
    break;

  case 146: /* attr_var: %empty  */
#line 673 "./cgram.y"
                   {	
			NODE *q, *p;

			p = pragma_aligned ? bdty(CALL, bdty(NAME, "aligned"),
			    bcon(pragma_aligned)) : NIL;
			if (pragma_packed) {
				q = bdty(NAME, "packed");
				p = (p == NIL ? q : cmop(p, q));
			}
			pragma_aligned = pragma_packed = 0;
			(yyval.nodep) = p;
		}
#line 3077 "y.tab.c"
    break;

  case 149: /* attr_spec_list: attr_spec_list attribute_specifier  */
#line 689 "./cgram.y"
                                                      { (yyval.nodep) = cmop((yyvsp[-1].nodep), (yyvsp[0].nodep)); }
#line 3083 "y.tab.c"
    break;

  case 150: /* str_head: C_STRUCT attr_var  */
#line 692 "./cgram.y"
                                     {  (yyval.rp) = bstruct(NULL, (yyvsp[-1].intval), (yyvsp[0].nodep));  }
#line 3089 "y.tab.c"
    break;

  case 151: /* str_head: C_STRUCT attr_var C_NAME  */
#line 693 "./cgram.y"
                                            {  (yyval.rp) = bstruct((yyvsp[0].strp), (yyvsp[-2].intval), (yyvsp[-1].nodep));  }
#line 3095 "y.tab.c"
    break;

  case 154: /* struct_declaration: declaration_specifiers struct_declarator_list optsemi  */
#line 701 "./cgram.y"
                                                                         {
			tfree((yyvsp[-2].nodep));
		}
#line 3103 "y.tab.c"
    break;

  case 155: /* struct_declaration: C_NAME ':'  */
#line 704 "./cgram.y"
                              { /* cxxaccess($1); */ }
#line 3109 "y.tab.c"
    break;

  case 156: /* optsemi: ';'  */
#line 707 "./cgram.y"
                       { }
#line 3115 "y.tab.c"
    break;

  case 157: /* optsemi: optsemi ';'  */
#line 708 "./cgram.y"
                               { werror("extra ; in struct"); }
#line 3121 "y.tab.c"
    break;

  case 158: /* specifier_qualifier_list: merge_specifiers  */
#line 712 "./cgram.y"
                                    { (yyval.nodep) = typenode((yyvsp[0].nodep)); }
#line 3127 "y.tab.c"
    break;

  case 159: /* merge_specifiers: type_sq merge_specifiers  */
#line 715 "./cgram.y"
                                            { (yyval.nodep) = cmop((yyvsp[0].nodep), (yyvsp[-1].nodep)); }
#line 3133 "y.tab.c"
    break;

  case 160: /* merge_specifiers: type_sq  */
#line 716 "./cgram.y"
                           { (yyval.nodep) = (yyvsp[0].nodep); }
#line 3139 "y.tab.c"
    break;

  case 161: /* struct_declarator_list: struct_declarator  */
#line 720 "./cgram.y"
                                     { symclear(blevel); }
#line 3145 "y.tab.c"
    break;

  case 162: /* @2: %empty  */
#line 721 "./cgram.y"
                                              { (yyval.nodep)=(yyvsp[-2].nodep); }
#line 3151 "y.tab.c"
    break;

  case 163: /* struct_declarator_list: struct_declarator_list ',' @2 struct_declarator  */
#line 722 "./cgram.y"
                                          { symclear(blevel); }
#line 3157 "y.tab.c"
    break;

  case 164: /* struct_declarator: declarator attr_var  */
#line 725 "./cgram.y"
                                       {
			NODE *p;

			(yyvsp[-1].nodep) = aryfix((yyvsp[-1].nodep));
			p = tymerge((yyvsp[-2].nodep), tymfix((yyvsp[-1].nodep)));
			if ((yyvsp[0].nodep))
				p->n_ap = attr_add(p->n_ap, gcc_attr_wrapper((yyvsp[0].nodep)));
			soumemb(p, (char *)(yyvsp[-1].nodep)->n_sp, glval((yyvsp[-2].nodep)));
			tfree(p);
		}
#line 3172 "y.tab.c"
    break;

  case 165: /* struct_declarator: ':' e  */
#line 735 "./cgram.y"
                         {
			int ie = con_e((yyvsp[0].nodep));
			if (fldchk(ie))
				ie = 1;
			falloc(NULL, ie, (yyvsp[-2].nodep));
		}
#line 3183 "y.tab.c"
    break;

  case 166: /* struct_declarator: declarator ':' e  */
#line 741 "./cgram.y"
                                    {
			int ie = con_e((yyvsp[0].nodep));
			if (fldchk(ie))
				ie = 1;
			if ((yyvsp[-2].nodep)->n_op == NAME) {
				/* XXX - tymfix() may alter $1 */
				tymerge((yyvsp[-3].nodep), tymfix((yyvsp[-2].nodep)));
				soumemb((yyvsp[-2].nodep), (char *)(yyvsp[-2].nodep)->n_sp, FIELD | ie);
				nfree((yyvsp[-2].nodep));
			} else
				uerror("illegal declarator");
		}
#line 3200 "y.tab.c"
    break;

  case 167: /* struct_declarator: declarator ':' e attr_spec_list  */
#line 753 "./cgram.y"
                                                   {
			int ie = con_e((yyvsp[-1].nodep));
			if (fldchk(ie))
				ie = 1;
			if ((yyvsp[-3].nodep)->n_op == NAME) {
				/* XXX - tymfix() may alter $1 */
				tymerge((yyvsp[-4].nodep), tymfix((yyvsp[-3].nodep)));
				if ((yyvsp[0].nodep))
					(yyvsp[-3].nodep)->n_ap = attr_add((yyvsp[-3].nodep)->n_ap,
					    gcc_attr_wrapper((yyvsp[0].nodep)));
				soumemb((yyvsp[-3].nodep), (char *)(yyvsp[-3].nodep)->n_sp, FIELD | ie);
				nfree((yyvsp[-3].nodep));
			} else
				uerror("illegal declarator");
		}
#line 3220 "y.tab.c"
    break;

  case 168: /* struct_declarator: %empty  */
#line 768 "./cgram.y"
                                       {
			NODE *p = (yyvsp[0].nodep);
			char *c = permalloc(10);

			if (p->n_type != STRTY && p->n_type != UNIONTY)
				uerror("bad unnamed member type");
			snprintf(c, 10, "*%dFAKE", getlab());
			soumemb(p, c, 0);
		}
#line 3234 "y.tab.c"
    break;

  case 169: /* xnfdeclarator: declarator attr_var  */
#line 780 "./cgram.y"
                                       {
			(yyval.symp) = xnf = init_declarator((yyvsp[-2].nodep), (yyvsp[-1].nodep), 1, (yyvsp[0].nodep));
		}
#line 3242 "y.tab.c"
    break;

  case 170: /* xnfdeclarator: declarator C_ASM '(' string ')'  */
#line 783 "./cgram.y"
                                                   {
			pragma_renamed = newstring((yyvsp[-1].strp), strlen((yyvsp[-1].strp)));
			(yyval.symp) = xnf = init_declarator((yyvsp[-5].nodep), (yyvsp[-4].nodep), 1, NULL);
		}
#line 3251 "y.tab.c"
    break;

  case 171: /* init_declarator: declarator attr_var  */
#line 793 "./cgram.y"
                                       { init_declarator((yyvsp[-2].nodep), (yyvsp[-1].nodep), 0, (yyvsp[0].nodep));}
#line 3257 "y.tab.c"
    break;

  case 172: /* init_declarator: declarator C_ASM '(' string ')' attr_var  */
#line 794 "./cgram.y"
                                                            {
#ifdef GCC_COMPAT
			pragma_renamed = newstring((yyvsp[-2].strp), strlen((yyvsp[-2].strp)));
			init_declarator((yyvsp[-6].nodep), (yyvsp[-5].nodep), 0, (yyvsp[0].nodep));
#else
			werror("gcc extension");
			init_declarator((yyvsp[-6].nodep), (yyvsp[-5].nodep), 0, (yyvsp[0].nodep));
#endif
		}
#line 3271 "y.tab.c"
    break;

  case 173: /* init_declarator: xnfdeclarator '=' e  */
#line 803 "./cgram.y"
                                       { 
			if ((yyvsp[-2].symp)->sclass == STATIC || (yyvsp[-2].symp)->sclass == EXTDEF)
				statinit++;
			simpleinit((yyvsp[-2].symp), eve((yyvsp[0].nodep)));
			if ((yyvsp[-2].symp)->sclass == STATIC || (yyvsp[-2].symp)->sclass == EXTDEF)
				statinit--;
			xnf = NULL;
		}
#line 3284 "y.tab.c"
    break;

  case 174: /* init_declarator: xnfdeclarator '=' begbr init_list optcomma '}'  */
#line 811 "./cgram.y"
                                                                  {
			endinit(0);
			xnf = NULL;
		}
#line 3293 "y.tab.c"
    break;

  case 175: /* init_declarator: xnfdeclarator '=' begbr '}'  */
#line 815 "./cgram.y"
                                               { endinit(0); xnf = NULL; }
#line 3299 "y.tab.c"
    break;

  case 176: /* init_declarator: xnfdeclarator '=' addrlbl  */
#line 816 "./cgram.y"
                                             { simpleinit((yyvsp[-2].symp), (yyvsp[0].nodep)); xnf = NULL; }
#line 3305 "y.tab.c"
    break;

  case 177: /* begbr: '{'  */
#line 819 "./cgram.y"
                       { beginit((yyvsp[(-1) - (1)].symp)); }
#line 3311 "y.tab.c"
    break;

  case 178: /* initializer: e  */
#line 822 "./cgram.y"
                               {  (yyval.nodep) = eve((yyvsp[0].nodep)); }
#line 3317 "y.tab.c"
    break;

  case 179: /* initializer: addrlbl  */
#line 823 "./cgram.y"
                           {  (yyval.nodep) = (yyvsp[0].nodep); }
#line 3323 "y.tab.c"
    break;

  case 180: /* initializer: ibrace init_list optcomma '}'  */
#line 824 "./cgram.y"
                                                 { (yyval.nodep) = NULL; }
#line 3329 "y.tab.c"
    break;

  case 181: /* initializer: ibrace '}'  */
#line 825 "./cgram.y"
                              { asginit(bcon(0)); (yyval.nodep) = NULL; }
#line 3335 "y.tab.c"
    break;

  case 182: /* init_list: designation initializer  */
#line 828 "./cgram.y"
                                           { dainit((yyvsp[-1].nodep), (yyvsp[0].nodep)); }
#line 3341 "y.tab.c"
    break;

  case 183: /* init_list: init_list ',' designation initializer  */
#line 829 "./cgram.y"
                                                          { dainit((yyvsp[-1].nodep), (yyvsp[0].nodep)); }
#line 3347 "y.tab.c"
    break;

  case 184: /* designation: designator_list '='  */
#line 832 "./cgram.y"
                                       { desinit((yyvsp[-1].nodep)); (yyval.nodep) = NIL; }
#line 3353 "y.tab.c"
    break;

  case 185: /* designation: GCC_DESIG  */
#line 833 "./cgram.y"
                             { desinit(bdty(NAME, (yyvsp[0].strp))); (yyval.nodep) = NIL; }
#line 3359 "y.tab.c"
    break;

  case 186: /* designation: '[' e C_ELLIPSIS e ']' '='  */
#line 834 "./cgram.y"
                                              { (yyval.nodep) = biop(CM, (yyvsp[-4].nodep), (yyvsp[-2].nodep)); }
#line 3365 "y.tab.c"
    break;

  case 187: /* designation: %empty  */
#line 835 "./cgram.y"
                   { (yyval.nodep) = NIL; }
#line 3371 "y.tab.c"
    break;

  case 188: /* designator_list: designator  */
#line 838 "./cgram.y"
                              { (yyval.nodep) = (yyvsp[0].nodep); }
#line 3377 "y.tab.c"
    break;

  case 189: /* designator_list: designator_list designator  */
#line 839 "./cgram.y"
                                              { (yyval.nodep) = (yyvsp[0].nodep); (yyval.nodep)->n_left = (yyvsp[-1].nodep); }
#line 3383 "y.tab.c"
    break;

  case 190: /* designator: '[' e ']'  */
#line 842 "./cgram.y"
                             {
			int ie = con_e((yyvsp[-1].nodep));
			if (ie < 0) {
				uerror("designator must be non-negative");
				ie = 0;
			}
			(yyval.nodep) = biop(LB, NIL, bcon(ie));
		}
#line 3396 "y.tab.c"
    break;

  case 191: /* designator: C_STROP C_TYPENAME  */
#line 850 "./cgram.y"
                                      {
			if ((yyvsp[-1].intval) != DOT)
				uerror("invalid designator");
			(yyval.nodep) = bdty(NAME, (yyvsp[0].strp));
		}
#line 3406 "y.tab.c"
    break;

  case 192: /* designator: C_STROP C_NAME  */
#line 855 "./cgram.y"
                                  {
			if ((yyvsp[-1].intval) != DOT)
				uerror("invalid designator");
			(yyval.nodep) = bdty(NAME, (yyvsp[0].strp));
		}
#line 3416 "y.tab.c"
    break;

  case 195: /* ibrace: '{'  */
#line 866 "./cgram.y"
                       {  ilbrace(); }
#line 3422 "y.tab.c"
    break;

  case 196: /* compoundstmt: begin block_item_list '}'  */
#line 871 "./cgram.y"
                                             { flend(); }
#line 3428 "y.tab.c"
    break;

  case 197: /* compoundstmt: begin '}'  */
#line 872 "./cgram.y"
                             { flend(); }
#line 3434 "y.tab.c"
    break;

  case 198: /* begin: '{'  */
#line 875 "./cgram.y"
                      {
			struct savbc *bc = tmpalloc(sizeof(struct savbc));
			if (blevel == 1) {
#ifdef STABS
				if (gflag)
					stabs_line(lineno);
#endif
				dclargs();
			}
#ifdef STABS
			if (gflag && blevel > 1)
				stabs_lbrac(blevel+1);
#endif
			++blevel;
			oldstyle = 0;
			bc->contlab = autooff;
			bc->next = savctx;
			savctx = bc;
			if (!isinlining && sspflag && blevel == 2)
				sspstart();
		}
#line 3460 "y.tab.c"
    break;

  case 199: /* statement: e ';'  */
#line 898 "./cgram.y"
                         { /* fwalk($1, eprint, 0); */ ecomp(eve((yyvsp[-1].nodep))); symclear(blevel); }
#line 3466 "y.tab.c"
    break;

  case 201: /* statement: ifprefix statement  */
#line 900 "./cgram.y"
                                      { plabel((yyvsp[-1].intval)); reached = 1; }
#line 3472 "y.tab.c"
    break;

  case 202: /* statement: ifelprefix statement  */
#line 901 "./cgram.y"
                                        {
			if ((yyvsp[-1].intval) != NOLAB) {
				plabel( (yyvsp[-1].intval));
				reached = 1;
			}
		}
#line 3483 "y.tab.c"
    break;

  case 203: /* statement: whprefix statement  */
#line 907 "./cgram.y"
                                      {
			branch(contlab);
			plabel( brklab );
			if( (flostat&FBRK) || !(flostat&FLOOP))
				reached = 1;
			else
				reached = 0;
			resetbc(0);
		}
#line 3497 "y.tab.c"
    break;

  case 204: /* statement: doprefix statement C_WHILE '(' e ')' ';'  */
#line 916 "./cgram.y"
                                                            {
			plabel(contlab);
			if (flostat & FCONT)
				reached = 1;
			if (reached)
				cbranch(buildtree(NE, eve((yyvsp[-2].nodep)), bcon(0)),
				    bcon((yyvsp[-6].intval)));
			else
				tfree(eve((yyvsp[-2].nodep)));
			plabel( brklab);
			reached = 1;
			resetbc(0);
		}
#line 3515 "y.tab.c"
    break;

  case 205: /* statement: forprefix .e ')' statement  */
#line 930 "./cgram.y"
                        {  plabel( contlab );
			    if( flostat&FCONT ) reached = 1;
			    if( (yyvsp[-2].nodep) ) ecomp( (yyvsp[-2].nodep) );
			    branch((yyvsp[-3].intval));
			    plabel( brklab );
			    if( (flostat&FBRK) || !(flostat&FLOOP) ) reached = 1;
			    else reached = 0;
			    resetbc(0);
			    symclear(blevel); /* if declaration inside for() */
			    }
#line 3530 "y.tab.c"
    break;

  case 206: /* statement: switchpart statement  */
#line 941 "./cgram.y"
                        { if( reached ) branch( brklab );
			    plabel( (yyvsp[-1].intval) );
			    swend();
			    plabel( brklab);
			    if( (flostat&FBRK) || !(flostat&FDEF) ) reached = 1;
			    resetbc(FCONT);
			    }
#line 3542 "y.tab.c"
    break;

  case 207: /* statement: C_BREAK ';'  */
#line 948 "./cgram.y"
                                {
			if (brklab == NOLAB)
				uerror("illegal break");
			else if (reached)
				branch(brklab);
			flostat |= FBRK;
			reached = 0;
		}
#line 3555 "y.tab.c"
    break;

  case 208: /* statement: C_CONTINUE ';'  */
#line 956 "./cgram.y"
                                   {
			if (contlab == NOLAB)
				uerror("illegal continue");
			else
				branch(contlab);
			flostat |= FCONT;
			goto rch;
		}
#line 3568 "y.tab.c"
    break;

  case 209: /* statement: C_RETURN ';'  */
#line 964 "./cgram.y"
                                 {
			branch(retlab);
			if (cftnsp->stype != VOID && 
			    (cftnsp->sflags & NORETYP) == 0 &&
			    cftnsp->stype != VOID+FTN)
				uerror("return value required");
			rch:
			if (!reached)
				warner(Wunreachable_code);
			reached = 0;
		}
#line 3584 "y.tab.c"
    break;

  case 210: /* statement: C_RETURN e ';'  */
#line 975 "./cgram.y"
                                   {
			NODE *p, *q;

			p = nametree(cftnsp);
			p->n_type = DECREF(p->n_type);
			q = eve((yyvsp[-1].nodep));
#ifndef NO_COMPLEX
			if (ANYCX(q) || ANYCX(p))
				q = cxret(q, p);
#endif
			p = buildtree(RETURN, p, q);
			if (p->n_type == VOID) {
				ecomp(p->n_right);
			} else {
				if (cftnod == NIL)
					cftnod = tempnode(0, p->n_type,
					    p->n_df, p->n_ap);
				ecomp(buildtree(ASSIGN,
				    ccopy(cftnod), p->n_right));
			}
			tfree(p->n_left);
			nfree(p);
			branch(retlab);
			reached = 0;
		}
#line 3614 "y.tab.c"
    break;

  case 211: /* statement: C_GOTO C_NAME ';'  */
#line 1000 "./cgram.y"
                                     { gotolabel((yyvsp[-1].strp)); goto rch; }
#line 3620 "y.tab.c"
    break;

  case 212: /* statement: C_GOTO '*' e ';'  */
#line 1001 "./cgram.y"
                                    { ecomp(biop(GOTO, eve((yyvsp[-1].nodep)), NIL)); }
#line 3626 "y.tab.c"
    break;

  case 218: /* asmstatement: C_ASM mvol '(' string ')'  */
#line 1009 "./cgram.y"
                                             { send_passt(IP_ASM, mkpstr((yyvsp[-1].strp))); }
#line 3632 "y.tab.c"
    break;

  case 219: /* asmstatement: C_ASM mvol '(' string xasm ')'  */
#line 1010 "./cgram.y"
                                                  { mkxasm((yyvsp[-2].strp), (yyvsp[-1].nodep)); }
#line 3638 "y.tab.c"
    break;

  case 221: /* mvol: C_QUALIFIER  */
#line 1014 "./cgram.y"
                               { nfree((yyvsp[0].nodep)); }
#line 3644 "y.tab.c"
    break;

  case 222: /* xasm: ':' oplist  */
#line 1017 "./cgram.y"
                              { (yyval.nodep) = xcmop((yyvsp[0].nodep), NIL, NIL); }
#line 3650 "y.tab.c"
    break;

  case 223: /* xasm: ':' oplist ':' oplist  */
#line 1018 "./cgram.y"
                                         { (yyval.nodep) = xcmop((yyvsp[-2].nodep), (yyvsp[0].nodep), NIL); }
#line 3656 "y.tab.c"
    break;

  case 224: /* xasm: ':' oplist ':' oplist ':' cnstr  */
#line 1019 "./cgram.y"
                                                   { (yyval.nodep) = xcmop((yyvsp[-4].nodep), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3662 "y.tab.c"
    break;

  case 225: /* oplist: %empty  */
#line 1022 "./cgram.y"
                                 { (yyval.nodep) = NIL; }
#line 3668 "y.tab.c"
    break;

  case 226: /* oplist: oper  */
#line 1023 "./cgram.y"
                        { (yyval.nodep) = (yyvsp[0].nodep); }
#line 3674 "y.tab.c"
    break;

  case 227: /* oper: string '(' e ')'  */
#line 1026 "./cgram.y"
                                    { (yyval.nodep) = xasmop((yyvsp[-3].strp), eve((yyvsp[-1].nodep))); }
#line 3680 "y.tab.c"
    break;

  case 228: /* oper: oper ',' string '(' e ')'  */
#line 1027 "./cgram.y"
                                             {
			(yyval.nodep) = cmop((yyvsp[-5].nodep), xasmop((yyvsp[-3].strp), eve((yyvsp[-1].nodep))));
		}
#line 3688 "y.tab.c"
    break;

  case 229: /* cnstr: string  */
#line 1032 "./cgram.y"
                          { (yyval.nodep) = xasmop((yyvsp[0].strp), bcon(0)); }
#line 3694 "y.tab.c"
    break;

  case 230: /* cnstr: cnstr ',' string  */
#line 1033 "./cgram.y"
                                    { (yyval.nodep) = cmop((yyvsp[-2].nodep), xasmop((yyvsp[0].strp), bcon(0))); }
#line 3700 "y.tab.c"
    break;

  case 231: /* label: C_NAME ':' attr_var  */
#line 1036 "./cgram.y"
                                       { deflabel((yyvsp[-2].strp), (yyvsp[0].nodep)); reached = 1; }
#line 3706 "y.tab.c"
    break;

  case 232: /* label: C_TYPENAME ':' attr_var  */
#line 1037 "./cgram.y"
                                           { deflabel((yyvsp[-2].strp), (yyvsp[0].nodep)); reached = 1; }
#line 3712 "y.tab.c"
    break;

  case 233: /* label: C_CASE e ':'  */
#line 1038 "./cgram.y"
                                { addcase(eve((yyvsp[-1].nodep))); reached = 1; }
#line 3718 "y.tab.c"
    break;

  case 234: /* label: C_CASE e C_ELLIPSIS e ':'  */
#line 1039 "./cgram.y"
                                             {
#ifdef GCC_COMPAT
			gcccase(eve((yyvsp[-3].nodep)), eve((yyvsp[-1].nodep))); reached = 1;
#endif
		}
#line 3728 "y.tab.c"
    break;

  case 235: /* label: C_DEFAULT ':'  */
#line 1044 "./cgram.y"
                                 { reached = 1; adddef(); flostat |= FDEF; }
#line 3734 "y.tab.c"
    break;

  case 236: /* doprefix: C_DO  */
#line 1047 "./cgram.y"
                     {
			savebc();
			brklab = getlab();
			contlab = getlab();
			plabel(  (yyval.intval) = getlab());
			reached = 1;
		}
#line 3746 "y.tab.c"
    break;

  case 237: /* ifprefix: C_IF '(' e ')'  */
#line 1055 "./cgram.y"
                               {
			cbranch(buildtree(NOT, eve((yyvsp[-1].nodep)), NIL), bcon((yyval.intval) = getlab()));
			reached = 1;
		}
#line 3755 "y.tab.c"
    break;

  case 238: /* ifelprefix: ifprefix statement C_ELSE  */
#line 1060 "./cgram.y"
                                            {
			if (reached)
				branch((yyval.intval) = getlab());
			else
				(yyval.intval) = NOLAB;
			plabel( (yyvsp[-2].intval));
			reached = 1;
		}
#line 3768 "y.tab.c"
    break;

  case 239: /* whprefix: C_WHILE '(' e ')'  */
#line 1070 "./cgram.y"
                                       {
			savebc();
			(yyvsp[-1].nodep) = eve((yyvsp[-1].nodep));
			if ((yyvsp[-1].nodep)->n_op == ICON && glval((yyvsp[-1].nodep)) != 0)
				flostat = FLOOP;
			plabel( contlab = getlab());
			reached = 1;
			brklab = getlab();
			if (flostat == FLOOP)
				tfree((yyvsp[-1].nodep));
			else
				cbranch(buildtree(NOT, (yyvsp[-1].nodep), NIL), bcon(brklab));
		}
#line 3786 "y.tab.c"
    break;

  case 240: /* forprefix: C_FOR '(' .e ';' .e ';'  */
#line 1084 "./cgram.y"
                                              {
			if ((yyvsp[-3].nodep))
				ecomp((yyvsp[-3].nodep));
			savebc();
			contlab = getlab();
			brklab = getlab();
			plabel( (yyval.intval) = getlab());
			reached = 1;
			if ((yyvsp[-1].nodep))
				cbranch(buildtree(NOT, (yyvsp[-1].nodep), NIL), bcon(brklab));
			else
				flostat |= FLOOP;
		}
#line 3804 "y.tab.c"
    break;

  case 241: /* $@3: %empty  */
#line 1097 "./cgram.y"
                             { ++blevel; }
#line 3810 "y.tab.c"
    break;

  case 242: /* forprefix: C_FOR '(' $@3 declaration .e ';'  */
#line 1097 "./cgram.y"
                                                              {
			blevel--;
			savebc();
			contlab = getlab();
			brklab = getlab();
			plabel( (yyval.intval) = getlab());
			reached = 1;
			if ((yyvsp[-1].nodep))
				cbranch(buildtree(NOT, (yyvsp[-1].nodep), NIL), bcon(brklab));
			else
				flostat |= FLOOP;
		}
#line 3827 "y.tab.c"
    break;

  case 243: /* switchpart: C_SWITCH '(' e ')'  */
#line 1111 "./cgram.y"
                                        {
			NODE *p;
			int num;
			TWORD t;

			savebc();
			brklab = getlab();
			(yyvsp[-1].nodep) = eve((yyvsp[-1].nodep));
			if (!ISINTEGER((yyvsp[-1].nodep)->n_type)) {
				uerror("switch expression must have integer "
				       "type");
				t = INT;
			} else {
				(yyvsp[-1].nodep) = intprom((yyvsp[-1].nodep));
				t = (yyvsp[-1].nodep)->n_type;
			}
			p = tempnode(0, t, 0, 0);
			num = regno(p);
			ecomp(buildtree(ASSIGN, p, (yyvsp[-1].nodep)));
			branch( (yyval.intval) = getlab());
			swstart(num, t);
			reached = 0;
		}
#line 3855 "y.tab.c"
    break;

  case 244: /* .e: e  */
#line 1136 "./cgram.y"
                     { (yyval.nodep) = eve((yyvsp[0].nodep)); }
#line 3861 "y.tab.c"
    break;

  case 245: /* .e: %empty  */
#line 1137 "./cgram.y"
                        { (yyval.nodep)=0; }
#line 3867 "y.tab.c"
    break;

  case 246: /* elist: %empty  */
#line 1140 "./cgram.y"
                   { (yyval.nodep) = NIL; }
#line 3873 "y.tab.c"
    break;

  case 248: /* elist: elist ',' e  */
#line 1142 "./cgram.y"
                                 { (yyval.nodep) = biop(CM, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3879 "y.tab.c"
    break;

  case 249: /* elist: elist ',' cast_type  */
#line 1143 "./cgram.y"
                                         { /* hack for stdarg */
			TYMFIX((yyvsp[0].nodep));
			(yyvsp[0].nodep)->n_op = TYPE;
			(yyval.nodep) = biop(CM, (yyvsp[-2].nodep), (yyvsp[0].nodep));
		}
#line 3889 "y.tab.c"
    break;

  case 250: /* e: e ',' e  */
#line 1153 "./cgram.y"
                           { (yyval.nodep) = biop(COMOP, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3895 "y.tab.c"
    break;

  case 251: /* e: e '=' e  */
#line 1154 "./cgram.y"
                           { (yyval.nodep) = biop(ASSIGN, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3901 "y.tab.c"
    break;

  case 252: /* e: e C_ASOP e  */
#line 1155 "./cgram.y"
                              { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3907 "y.tab.c"
    break;

  case 253: /* e: e '?' e ':' e  */
#line 1156 "./cgram.y"
                                 {
			(yyval.nodep)=biop(QUEST, (yyvsp[-4].nodep), biop(COLON, (yyvsp[-2].nodep), (yyvsp[0].nodep)));
		}
#line 3915 "y.tab.c"
    break;

  case 254: /* e: e '?' ':' e  */
#line 1159 "./cgram.y"
                               { 
			NODE *p = tempnode(0, (yyvsp[-3].nodep)->n_type, (yyvsp[-3].nodep)->n_df, (yyvsp[-3].nodep)->n_ap);
			(yyval.nodep) = biop(COLON, ccopy(p), (yyvsp[0].nodep));
			(yyval.nodep)=biop(QUEST, biop(ASSIGN, p, (yyvsp[-3].nodep)), (yyval.nodep));
		}
#line 3925 "y.tab.c"
    break;

  case 255: /* e: e C_OROR e  */
#line 1164 "./cgram.y"
                              { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3931 "y.tab.c"
    break;

  case 256: /* e: e C_ANDAND e  */
#line 1165 "./cgram.y"
                                { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3937 "y.tab.c"
    break;

  case 257: /* e: e '|' e  */
#line 1166 "./cgram.y"
                           { (yyval.nodep) = biop(OR, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3943 "y.tab.c"
    break;

  case 258: /* e: e '^' e  */
#line 1167 "./cgram.y"
                           { (yyval.nodep) = biop(ER, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3949 "y.tab.c"
    break;

  case 259: /* e: e '&' e  */
#line 1168 "./cgram.y"
                           { (yyval.nodep) = biop(AND, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3955 "y.tab.c"
    break;

  case 260: /* e: e C_EQUOP e  */
#line 1169 "./cgram.y"
                                { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3961 "y.tab.c"
    break;

  case 261: /* e: e C_RELOP e  */
#line 1170 "./cgram.y"
                               { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3967 "y.tab.c"
    break;

  case 262: /* e: e C_SHIFTOP e  */
#line 1171 "./cgram.y"
                                 { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3973 "y.tab.c"
    break;

  case 263: /* e: e '+' e  */
#line 1172 "./cgram.y"
                           { (yyval.nodep) = biop(PLUS, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3979 "y.tab.c"
    break;

  case 264: /* e: e '-' e  */
#line 1173 "./cgram.y"
                           { (yyval.nodep) = biop(MINUS, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3985 "y.tab.c"
    break;

  case 265: /* e: e C_DIVOP e  */
#line 1174 "./cgram.y"
                               { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3991 "y.tab.c"
    break;

  case 266: /* e: e '*' e  */
#line 1175 "./cgram.y"
                           { (yyval.nodep) = biop(MUL, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 3997 "y.tab.c"
    break;

  case 267: /* e: e '=' addrlbl  */
#line 1176 "./cgram.y"
                                 { (yyval.nodep) = biop(ASSIGN, (yyvsp[-2].nodep), (yyvsp[0].nodep)); }
#line 4003 "y.tab.c"
    break;

  case 269: /* xbegin: begin  */
#line 1180 "./cgram.y"
                         {
			(yyval.intval) = getlab(); getlab(); getlab();
			branch((yyval.intval)); plabel(((yyval.intval))+1); }
#line 4011 "y.tab.c"
    break;

  case 270: /* addrlbl: C_ANDAND C_NAME  */
#line 1185 "./cgram.y"
                                  {
#ifdef GCC_COMPAT
			struct symtab *s = lookup((yyvsp[0].strp), SLBLNAME);
			if (s->soffset == 0)
				s->soffset = -getlab();
			(yyval.nodep) = buildtree(ADDROF, nametree(s), NIL);
#else
			uerror("gcc extension");
#endif
		}
#line 4026 "y.tab.c"
    break;

  case 271: /* term: term C_INCOP  */
#line 1197 "./cgram.y"
                                {  (yyval.nodep) = biop((yyvsp[0].intval), (yyvsp[-1].nodep), bcon(1)); }
#line 4032 "y.tab.c"
    break;

  case 272: /* term: '*' term  */
#line 1198 "./cgram.y"
                            { (yyval.nodep) = biop(UMUL, (yyvsp[0].nodep), NIL); }
#line 4038 "y.tab.c"
    break;

  case 273: /* term: '&' term  */
#line 1199 "./cgram.y"
                            { (yyval.nodep) = biop(ADDROF, (yyvsp[0].nodep), NIL); }
#line 4044 "y.tab.c"
    break;

  case 274: /* term: '-' term  */
#line 1200 "./cgram.y"
                            { (yyval.nodep) = biop(UMINUS, (yyvsp[0].nodep), NIL ); }
#line 4050 "y.tab.c"
    break;

  case 275: /* term: '+' term  */
#line 1201 "./cgram.y"
                            { (yyval.nodep) = biop(PLUS, (yyvsp[0].nodep), bcon(0)); }
#line 4056 "y.tab.c"
    break;

  case 276: /* term: CXX_CASTS C_RELOP cast_type C_RELOP '(' e ')'  */
#line 1202 "./cgram.y"
                                                                 {
			tfree((yyvsp[-1].nodep));
			tfree((yyvsp[-4].nodep));
			(yyval.nodep) = bcon(0);
			werror("CXX_CASTS unhandled");
		}
#line 4067 "y.tab.c"
    break;

  case 277: /* term: C_UNOP term  */
#line 1208 "./cgram.y"
                               { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[0].nodep), NIL); }
#line 4073 "y.tab.c"
    break;

  case 278: /* term: C_INCOP term  */
#line 1209 "./cgram.y"
                                {
			(yyval.nodep) = biop((yyvsp[-1].intval) == INCR ? PLUSEQ : MINUSEQ, (yyvsp[0].nodep), bcon(1));
		}
#line 4081 "y.tab.c"
    break;

  case 279: /* term: C_SIZEOF xa term  */
#line 1212 "./cgram.y"
                                    { (yyval.nodep) = biop(SZOF, (yyvsp[0].nodep), bcon(0)); inattr = (yyvsp[-1].intval); }
#line 4087 "y.tab.c"
    break;

  case 280: /* term: '(' cast_type ')' term  */
#line 1213 "./cgram.y"
                                                         {
			TYMFIX((yyvsp[-2].nodep));
			(yyval.nodep) = biop(CAST, (yyvsp[-2].nodep), (yyvsp[0].nodep));
		}
#line 4096 "y.tab.c"
    break;

  case 281: /* term: C_SIZEOF xa '(' cast_type ')'  */
#line 1217 "./cgram.y"
                                                                 {
			(yyval.nodep) = biop(SZOF, (yyvsp[-1].nodep), bcon(1));
			inattr = (yyvsp[-3].intval);
		}
#line 4105 "y.tab.c"
    break;

  case 282: /* term: C_ALIGNOF xa '(' cast_type ')'  */
#line 1221 "./cgram.y"
                                                  {
			int al;
			TYMFIX((yyvsp[-1].nodep));
			al = talign((yyvsp[-1].nodep)->n_type, (yyvsp[-1].nodep)->n_ap);
			(yyval.nodep) = bcon(al/SZCHAR);
			inattr = (yyvsp[-3].intval);
			tfree((yyvsp[-1].nodep));
		}
#line 4118 "y.tab.c"
    break;

  case 283: /* term: '(' cast_type ')' clbrace init_list optcomma '}'  */
#line 1229 "./cgram.y"
                                                                   {
			endinit(0);
			(yyval.nodep) = bdty(NAME, (yyvsp[-3].symp));
			(yyval.nodep)->n_op = CLOP;
		}
#line 4128 "y.tab.c"
    break;

  case 284: /* term: '(' cast_type ')' clbrace '}'  */
#line 1234 "./cgram.y"
                                                {
			endinit(0);
			(yyval.nodep) = bdty(NAME, (yyvsp[-1].symp));
			(yyval.nodep)->n_op = CLOP;
		}
#line 4138 "y.tab.c"
    break;

  case 285: /* term: term '[' e ']'  */
#line 1239 "./cgram.y"
                                  {
			if ((yyvsp[-3].nodep)->n_op == NEWKW) {
				(yyvsp[-3].nodep)->n_left = biop(LB, (yyvsp[-3].nodep)->n_left, (yyvsp[-1].nodep));
			} else
				(yyval.nodep) = biop(LB, (yyvsp[-3].nodep), (yyvsp[-1].nodep));
		}
#line 4149 "y.tab.c"
    break;

  case 286: /* term: C_NAME '(' elist ')'  */
#line 1245 "./cgram.y"
                                         {
#if 0
			if ((yyvsp[-1].nodep)) tfree((yyvsp[-1].nodep));
			(yyval.nodep) = bcon(0);
#else
			(yyval.nodep) = biop((yyvsp[-1].nodep) ? CALL : UCALL, bdty(NAME, (yyvsp[-3].strp)), (yyvsp[-1].nodep));
#endif
		}
#line 4162 "y.tab.c"
    break;

  case 287: /* term: term '(' elist ')'  */
#line 1253 "./cgram.y"
                                       { (yyval.nodep) = biop((yyvsp[-1].nodep) ? CALL : UCALL, (yyvsp[-3].nodep), (yyvsp[-1].nodep)); }
#line 4168 "y.tab.c"
    break;

  case 288: /* term: term C_STROP C_NAME  */
#line 1254 "./cgram.y"
                                       { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), bdty(NAME, (yyvsp[0].strp))); }
#line 4174 "y.tab.c"
    break;

  case 289: /* term: term C_STROP C_TYPENAME  */
#line 1255 "./cgram.y"
                                           { (yyval.nodep) = biop((yyvsp[-1].intval), (yyvsp[-2].nodep), bdty(NAME, (yyvsp[0].strp)));}
#line 4180 "y.tab.c"
    break;

  case 290: /* term: C_NAME  */
#line 1256 "./cgram.y"
                                                      { (yyval.nodep) = bdty(NAME, (yyvsp[0].strp)); }
#line 4186 "y.tab.c"
    break;

  case 291: /* term: nmrec C_NAME  */
#line 1257 "./cgram.y"
                                               {
			(yyval.nodep) = biop(NMLIST, (yyvsp[-1].nodep), bdty(NAME, (yyvsp[0].strp)));
		}
#line 4194 "y.tab.c"
    break;

  case 292: /* term: PCC_OFFSETOF '(' cast_type ',' term ')'  */
#line 1260 "./cgram.y"
                                                            {
			TYMFIX((yyvsp[-3].nodep));
			(yyvsp[-3].nodep)->n_type = INCREF((yyvsp[-3].nodep)->n_type);
			(yyvsp[-3].nodep) = biop(CAST, (yyvsp[-3].nodep), bcon(0));
			if ((yyvsp[-1].nodep)->n_op == NAME) {
				(yyval.nodep) = biop(STREF, (yyvsp[-3].nodep), (yyvsp[-1].nodep));
			} else {
				NODE *p = (yyvsp[-1].nodep);
				while (p->n_left->n_op != NAME)
					p = p->n_left;
				p->n_left = biop(STREF, (yyvsp[-3].nodep), p->n_left);
				(yyval.nodep) = (yyvsp[-1].nodep);
			}
			(yyval.nodep) = biop(ADDROF, (yyval.nodep), NIL);
			(yyvsp[-3].nodep) = block(NAME, NIL, NIL, ENUNSIGN(INTPTR), 0, 0);
			(yyval.nodep) = biop(CAST, (yyvsp[-3].nodep), (yyval.nodep));
		}
#line 4216 "y.tab.c"
    break;

  case 293: /* term: C_ICON  */
#line 1277 "./cgram.y"
                          { (yyval.nodep) = (yyvsp[0].nodep); }
#line 4222 "y.tab.c"
    break;

  case 294: /* term: C_FCON  */
#line 1278 "./cgram.y"
                          { (yyval.nodep) = (yyvsp[0].nodep); }
#line 4228 "y.tab.c"
    break;

  case 295: /* term: string  */
#line 1279 "./cgram.y"
                          { (yyval.nodep) = bdty(STRING, (yyvsp[0].strp), widestr); }
#line 4234 "y.tab.c"
    break;

  case 296: /* term: '(' e ')'  */
#line 1280 "./cgram.y"
                                { (yyval.nodep)=(yyvsp[-1].nodep); }
#line 4240 "y.tab.c"
    break;

  case 297: /* term: '(' xbegin block_item_list e ';' '}' ')'  */
#line 1281 "./cgram.y"
                                                            {
			/* XXX - check recursive ({ }) statements */
			branch(((yyvsp[-5].intval))+2);
			plabel((yyvsp[-5].intval));
			(yyval.nodep) = buildtree(COMOP,
			    biop(GOTO, bcon(((yyvsp[-5].intval))+1), NIL), eve((yyvsp[-3].nodep)));
			flend();
		}
#line 4253 "y.tab.c"
    break;

  case 298: /* term: '(' xbegin block_item_list '}' ')'  */
#line 1289 "./cgram.y"
                                                      { 
			/* XXX - check recursive ({ }) statements */
			branch(((yyvsp[-3].intval))+2);
			plabel((yyvsp[-3].intval));
			(yyval.nodep) = buildtree(COMOP,
			    biop(GOTO, bcon(((yyvsp[-3].intval))+1), NIL), voidcon());
			flend();
		}
#line 4266 "y.tab.c"
    break;

  case 299: /* term: CXX_NEW new_ds  */
#line 1297 "./cgram.y"
                                  { (yyval.nodep) = biop(NEWKW, (yyvsp[0].nodep), bcon(0)); }
#line 4272 "y.tab.c"
    break;

  case 300: /* term: CXX_NEW '(' cast_type ')'  */
#line 1298 "./cgram.y"
                                             { (yyval.nodep) = 0; uerror("new cast"); }
#line 4278 "y.tab.c"
    break;

  case 301: /* term: CXX_DELETE term  */
#line 1299 "./cgram.y"
                                             {
			(yyval.nodep) = biop(DELETE, (yyvsp[0].nodep), bcon(NM_DEL));
		}
#line 4286 "y.tab.c"
    break;

  case 302: /* term: CXX_DELETE '[' ']' term  */
#line 1302 "./cgram.y"
                                                     {
			(yyval.nodep) = biop(DELETE, (yyvsp[0].nodep), bcon(NM_DLA));
		}
#line 4294 "y.tab.c"
    break;

  case 303: /* nmrec: CXX_MORENM  */
#line 1307 "./cgram.y"
                             { (yyval.nodep) = bdty(NAME, (yyvsp[0].strp)); }
#line 4300 "y.tab.c"
    break;

  case 304: /* nmrec: CXX_MORENM nmrec  */
#line 1308 "./cgram.y"
                                   { (yyval.nodep) = biop(NMLIST, (yyvsp[0].nodep), bdty(NAME, (yyvsp[-1].strp))); }
#line 4306 "y.tab.c"
    break;

  case 305: /* xa: %empty  */
#line 1311 "./cgram.y"
                  { (yyval.intval) = inattr; inattr = 0; }
#line 4312 "y.tab.c"
    break;

  case 306: /* clbrace: '{'  */
#line 1314 "./cgram.y"
                        { NODE *q = (yyvsp[(-1) - (1)].nodep); TYMFIX(q); (yyval.symp) = clbrace(q); }
#line 4318 "y.tab.c"
    break;

  case 307: /* string: C_STRING  */
#line 1317 "./cgram.y"
                            { widestr = 0; (yyval.strp) = stradd("", (yyvsp[0].strp)); }
#line 4324 "y.tab.c"
    break;

  case 308: /* string: string C_STRING  */
#line 1318 "./cgram.y"
                                   { (yyval.strp) = stradd((yyvsp[-1].strp), (yyvsp[0].strp)); }
#line 4330 "y.tab.c"
    break;

  case 309: /* cast_type: specifier_qualifier_list  */
#line 1321 "./cgram.y"
                                            {
			(yyval.nodep) = biop(TYMERGE, (yyvsp[0].nodep), bdty(NAME, NULL));
		}
#line 4338 "y.tab.c"
    break;

  case 310: /* cast_type: specifier_qualifier_list abstract_declarator  */
#line 1324 "./cgram.y"
                                                                {
			(yyval.nodep) = biop(TYMERGE, (yyvsp[-1].nodep), aryfix((yyvsp[0].nodep)));
		}
#line 4346 "y.tab.c"
    break;


#line 4350 "y.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1329 "./cgram.y"


NODE *
mkty(TWORD t, union dimfun *d, struct attr *sue)
{
	return block(TYPE, NIL, NIL, t, d, sue);
}

NODE *
bdty(int op, ...)
{
	va_list ap;
	int val;
	register NODE *q;

	va_start(ap, op);
	q = biop(op, NIL, NIL);

	switch (op) {
	case UMUL:
	case UCALL:
		q->n_left = va_arg(ap, NODE *);
		q->n_rval = 0;
		break;

	case CALL:
		q->n_left = va_arg(ap, NODE *);
		q->n_right = va_arg(ap, NODE *);
		break;

	case LB:
		q->n_left = va_arg(ap, NODE *);
		if ((val = va_arg(ap, int)) <= 0) {
			uerror("array size must be positive");
			val = 1;
		}
		q->n_right = bcon(val);
		break;

	case NAME:
		q->n_sp = va_arg(ap, struct symtab *); /* XXX survive tymerge */
		break;

	case STRING:
		q->n_name = va_arg(ap, char *);
		val = va_arg(ap, int);
		slval(q, val);
		break;

	default:
		cerror("bad bdty");
	}
	va_end(ap);

	return q;
}

static void
flend(void)
{
	if (!isinlining && sspflag && blevel == 2)
		sspend();
#ifdef STABS
	if (gflag && blevel > 2)
		stabs_rbrac(blevel);
#endif
	--blevel;
	if( blevel == 1 )
		blevel = 0;
	symclear(blevel); /* Clean ut the symbol table */
	if (autooff > maxautooff)
		maxautooff = autooff;
	autooff = savctx->contlab;
	savctx = savctx->next;
}

static void
savebc(void)
{
	struct savbc *bc = tmpalloc(sizeof(struct savbc));

	bc->brklab = brklab;
	bc->contlab = contlab;
	bc->flostat = flostat;
	bc->next = savbc;
	savbc = bc;
	flostat = 0;
}

static void
resetbc(int mask)
{
	flostat = savbc->flostat | (flostat&mask);
	contlab = savbc->contlab;
	brklab = savbc->brklab;
	savbc = savbc->next;
}

struct swdef {
	struct swdef *next;	/* Next in list */
	int deflbl;		/* Label for "default" */
	struct swents *ents;	/* Linked sorted list of case entries */
	int nents;		/* # of entries in list */
	int num;		/* Node value will end up in */
	TWORD type;		/* Type of switch expression */
} *swpole;

/*
 * add case to switch
 */
static void
addcase(NODE *p)
{
	struct swents **put, *w, *sw = tmpalloc(sizeof(struct swents));
	CONSZ val;

	p = optim(rmpconv(p));  /* change enum to ints */
	if (p->n_op != ICON || p->n_sp != NULL) {
		uerror( "non-constant case expression");
		return;
	}
	if (swpole == NULL) {
		uerror("case not in switch");
		return;
	}

	if (DEUNSIGN(swpole->type) != DEUNSIGN(p->n_type)) {
		val = glval(p);
		p = makety(p, swpole->type, 0, 0, 0);
		if (p->n_op != ICON)
			cerror("could not cast case value to type of switch "
			       "expression");
		if (glval(p) != val)
			werror("case expression truncated");
	}
	sw->sval = glval(p);
	tfree(p);
	put = &swpole->ents;
	if (ISUNSIGNED(swpole->type)) {
		for (w = swpole->ents;
		     w != NULL && (U_CONSZ)w->sval < (U_CONSZ)sw->sval;
		     w = w->next)
			put = &w->next;
	} else {
		for (w = swpole->ents; w != NULL && w->sval < sw->sval;
		     w = w->next)
			put = &w->next;
	}
	if (w != NULL && w->sval == sw->sval) {
		uerror("duplicate case in switch");
		return;
	}
	plabel(sw->slab = getlab());
	*put = sw;
	sw->next = w;
	swpole->nents++;
}

#ifdef GCC_COMPAT
void
gcccase(NODE *ln, NODE *hn)
{
	CONSZ i, l, h;

	l = icons(optim(ln));
	h = icons(optim(hn));

	if (h < l)
		i = l, l = h, h = i;

	for (i = l; i <= h; i++)
		addcase(xbcon(i, NULL, hn->n_type));
}
#endif

/*
 * add default case to switch
 */
static void
adddef(void)
{
	if (swpole == NULL)
		uerror("default not inside switch");
	else if (swpole->deflbl != 0)
		uerror("duplicate default in switch");
	else
		plabel( swpole->deflbl = getlab());
}

static void
swstart(int num, TWORD type)
{
	struct swdef *sw = tmpalloc(sizeof(struct swdef));

	sw->deflbl = sw->nents = 0;
	sw->ents = NULL;
	sw->next = swpole;
	sw->num = num;
	sw->type = type;
	swpole = sw;
}

/*
 * end a switch block
 */
static void
swend(void)
{
	struct swents *sw, **swp;
	int i;

	sw = tmpalloc(sizeof(struct swents));
	swp = tmpalloc(sizeof(struct swents *) * (swpole->nents+1));

	sw->slab = swpole->deflbl;
	swp[0] = sw;

	for (i = 1; i <= swpole->nents; i++) {
		swp[i] = swpole->ents;
		swpole->ents = swpole->ents->next;
	}
	genswitch(swpole->num, swpole->type, swp, swpole->nents);

	swpole = swpole->next;
}

/*
 * num: tempnode the value of the switch expression is in
 * type: type of the switch expression
 *
 * p points to an array of structures, each consisting
 * of a constant value and a label.
 * The first is >=0 if there is a default label;
 * its value is the label number
 * The entries p[1] to p[n] are the nontrivial cases
 * n is the number of case statements (length of list)
 */
static void
genswitch(int num, TWORD type, struct swents **p, int n)
{
	NODE *r, *q;
	int i;

	if (mygenswitch(num, type, p, n))
		return;

	/* simple switch code */
	for (i = 1; i <= n; ++i) {
		/* already in 1 */
		r = tempnode(num, type, 0, 0);
		q = xbcon(p[i]->sval, NULL, type);
		r = buildtree(NE, r, clocal(q));
		cbranch(buildtree(NOT, r, NIL), bcon(p[i]->slab));
	}
	if (p[0]->slab > 0)
		branch(p[0]->slab);
}

/*
 * Declare a variable or prototype.
 */
static struct symtab *
init_declarator(NODE *tn, NODE *p, int assign, NODE *a)
{
	int class = (int)glval(tn);
	struct symtab *sp;

	p = aryfix(p);
	if (p->n_op == NMLIST) {
		tymerge(tn, p->n_right);
	} else
		p = tymerge(tn, p);
	if (a) {
		struct attr *ap = gcc_attr_wrapper(a);
		p->n_ap = attr_add(p->n_ap, ap);
	}

	sp = cxxdeclvar(p);

	if (fun_inline && ISFTN(p->n_type))
		sp->sflags |= SINLINE;

	if (!ISFTN(p->n_type)) {
		if (assign) {
			defid(p, class);
			sp = p->n_sp;
			sp->sflags |= SASG;
			if (sp->sflags & SDYNARRAY)
				uerror("can't initialize dynamic arrays");
			lcommdel(sp);
		} else
			nidcl(p, class);
	} else {
		extern NODE *parlink;
		if (assign)
			uerror("cannot initialise function");
		defid(p, uclass(class));
		sp = p->n_sp;
		if (parlink) {
			/* dynamic sized arrays in prototypes */
			tfree(parlink); /* Free delayed tree */
			parlink = NIL;
		}
	}
	tfree(p);
	return sp;
}

/*
 * Declare old-stype function arguments.
 */
static void
oldargs(NODE *p)
{
	blevel++;
	p->n_op = TYPE;
	p->n_type = FARG;
	p->n_sp = lookup((char *)p->n_sp, 0);/* XXX */
	defid(p, PARAM);
	blevel--;
}

/*
 * Set NAME nodes to a null name and index of LB nodes to NOOFFSET
 * unless clr is one, in that case preserve variable name.
 */
static NODE *
namekill(NODE *p, int clr)
{
	NODE *q;
	int o = p->n_op;

	switch (coptype(o)) {
	case LTYPE:
		if (o == NAME) {
			if (clr)
				p->n_sp = NULL;
			else
				p->n_sp = lookup((char *)p->n_sp, 0);/* XXX */
		}
		break;

	case UTYPE:
		p->n_left = namekill(p->n_left, clr);
		break;

        case BITYPE:
                p->n_left = namekill(p->n_left, clr);
		if (o == LB) {
			if (clr) {
				tfree(p->n_right);
				p->n_right = bcon(NOOFFSET);
			} else
				p->n_right = eve(p->n_right);
		} else if (o == CALL)
			p->n_right = namekill(p->n_right, 1);
		else
			p->n_right = namekill(p->n_right, clr);
		if (o == TYMERGE) {
			q = tymerge(p->n_left, p->n_right);
			q->n_ap = attr_add(q->n_ap, p->n_ap);
			tfree(p->n_left);
			nfree(p);
			p = q;
		}
		break;
	}
	return p;
}

/*
 * Declare function arguments.
 */
static NODE *
funargs(NODE *p)
{
	extern NODE *arrstk[10];

	if (p->n_op == ELLIPSIS)
		return p;

	p = namekill(p, 0);
	if (ISFTN(p->n_type))
		p->n_type = INCREF(p->n_type);
	if (ISARY(p->n_type)) {
		p->n_type += (PTR-ARY);
		if (p->n_df->ddim == -1)
			tfree(arrstk[0]), arrstk[0] = NIL;
		p->n_df++;
	}
	if (p->n_type == VOID && p->n_sp->sname == NULL)
		return p; /* sanitycheck later */
	else if (p->n_sp->sname == NULL)
		; /* uerror("argument missing"); */
	else
		defid(p, PARAM);
	return p;
}

static NODE *
listfw(NODE *p, NODE * (*f)(NODE *))
{
        if (p->n_op == CM) {
                p->n_left = listfw(p->n_left, f);
                p->n_right = (*f)(p->n_right);
        } else
                p = (*f)(p);
	return p;
}


/*
 * Declare a function.
 */
static struct symtab *
fundef(NODE *tp, NODE *p)
{
	extern int prolab;
	struct symtab *s, *nsthis;
	NODE *q, *typ;
	int class = (int)glval(tp), oclass, ctval;
	char *c;

	/*
	 * We discard all names except for those needed for
	 * parameter declaration. While doing that, also change
	 * non-constant array sizes to unknown.
	 */
	ctval = tvaloff;
	for (q = p; coptype(q->n_op) != LTYPE &&
	    q->n_left->n_op != NAME &&
	    q->n_left->n_op != NMLIST; q = q->n_left) {
		if (q->n_op == CALL)
			q->n_right = namekill(q->n_right, 1);
	}
	if (q->n_op != CALL && q->n_op != UCALL) {
		uerror("invalid function definition");
		p = bdty(UCALL, p);
	} else if (q->n_op == CALL) {
		blevel = 1;
		argoff = ARGINIT;
		if (oldstyle == 0)
			q->n_right = listfw(q->n_right, funargs);
		ftnarg(q);
		blevel = 0;
	}

	if (p->n_op == CALL && p->n_left->n_op == NMLIST) {
		NODE *r = p->n_left;
		p->n_left = r->n_right;
		r->n_right = typ = tymerge(tp, p);
		p = r;
	} else
		typ = tymerge(tp, p);

#ifdef GCC_COMPAT
	/* gcc seems to discard __builtin_ when declaring functions */
	if (strncmp("__builtin_", (char *)typ->n_sp, 10) == 0)
		typ->n_sp = (struct symtab *)((char *)typ->n_sp + 10);
#endif

	s = cxxftnfind(p, SNORMAL);
	nsthis = nscur;
	nscur = s->sdown; /* XXX fun in fun? */

	oclass = s->sclass;
	if (class == STATIC && oclass == EXTERN)
		werror("%s was first declared extern, then static", s->sname);

	if (fun_inline) {
		/* special syntax for inline functions */
		if (! strcmp(s->sname,"main")) 
			uerror("cannot inline main()");

		s->sflags |= SINLINE;
		inline_start(s);
		if (class == EXTERN)
			class = EXTDEF;
	} else if (class == EXTERN)
		class = SNULL; /* same result */

	cftnsp = s;
#if 0
	defid(p, class);
#endif

#ifdef GCC_COMPAT
	if (attr_find(p->n_ap, GCC_ATYP_ALW_INL)) {
		/* Temporary turn on temps to make always_inline work */
		alwinl = 1;
		if (xtemps == 0) alwinl |= 2;
		xtemps = 1;
	}
#endif
	prolab = getlab();
	if ((c = cftnsp->soname) == NULL)
		c = addname(exname(cftnsp->sname));
	send_passt(IP_PROLOG, -1, c, cftnsp->stype,
	    cftnsp->sclass == EXTDEF, prolab, ctval);
	blevel++;
#ifdef STABS
	if (gflag)
		stabs_func(s);
#endif
	tfree(tp);
	tfree(p);
	return nsthis;
}

static void
fend(struct symtab *ns)
{
	if (blevel)
		cerror("function level error");
	ftnend();
	fun_inline = 0;
	if (alwinl & 2) xtemps = 0;
	alwinl = 0;
	cftnsp = NULL;
	nscur = ns;
}

NODE *
structref(NODE *p, int f, char *name)
{
	NODE *r;

	if (f == DOT)
		p = buildtree(ADDROF, p, NIL);
	r = biop(NAME, NIL, NIL);
	r->n_name = name;
	r = buildtree(STREF, p, r);
	return r;
}

static void
olddecl(NODE *p, NODE *a)
{
	struct symtab *s;

	p = namekill(p, 0);
	s = p->n_sp;
	if (s->slevel != 1 || s->stype == UNDEF)
		uerror("parameter '%s' not defined", s->sname);
	else if (s->stype != FARG)
		uerror("parameter '%s' redefined", s->sname);

	s->stype = p->n_type;
	s->sdf = p->n_df;
	s->sap = p->n_ap;
	if (ISARY(s->stype)) {
		s->stype += (PTR-ARY);
		s->sdf++;
	} else if (s->stype == FLOAT)
		s->stype = DOUBLE;
	if (a)
		attr_add(s->sap, gcc_attr_wrapper(a));
	nfree(p);
}

void
branch(int lbl)
{
	int r = reached++;
	ecomp(biop(GOTO, bcon(lbl), NIL));
	reached = r;
}

/*
 * Create a printable string based on an encoded string.
 */
static char *
mkpstr(char *str)
{
	char *s, *os;
	int v, l = (int)strlen(str)+3; /* \t + \n + \0 */

	os = s = inlalloc(l);
	*s++ = '\t';
	for (; *str; ) {
		if (*str++ == '\\')
			v = esccon(&str);
		else
			v = str[-1];
		*s++ = v;
	}
	*s++ = '\n';
	*s = 0;
	return os;
}

/*
 * Estimate the max length a string will have in its internal 
 * representation based on number of \ characters.
 */
static int
maxstlen(char *str)
{
	int i;

	for (i = 0; *str; str++, i++)
		if (*str == '\\' || *str < 32 || *str > 0176)
			i += 3;
	return i;
}

static char *
voct(char *d, unsigned int v)
{
	v &= (1 << SZCHAR) - 1;
	*d++ = '\\';
	*d++ = v/64 + '0'; v &= 077;
	*d++ = v/8 + '0'; v &= 7;
	*d++ = v + '0';
	return d;
}
	

/*
 * Convert a string to internal format.  The resulting string may be no
 * more than len characters long.
 */
static void
fixstr(char *d, char *s, int len)
{
	unsigned int v;

	while (*s) {
		if (len <= 0)
			cerror("fixstr");
		if (*s == '\\') {
			s++;
			v = esccon(&s);
			d = voct(d, v);
			len -= 4;
		} else if (*s < ' ' || *s > 0176) {
			d = voct(d, *s++);
			len -= 4;
		} else
			*d++ = *s++, len--;
	}
	*d = 0;
}

/*
 * Add "raw" string new to cleaned string old.
 */
static char *
stradd(char *old, char *new)
{
	char *rv;
	int len;

	if (*new == 'L' && new[1] == '\"')
		widestr = 1, new++;
	if (*new == '\"') {
		new++;			 /* remove first " */
		new[strlen(new) - 1] = 0;/* remove last " */
	}
	len = (int)strlen(old) + maxstlen(new) + 1;
	rv = tmpalloc(len);
	strlcpy(rv, old, len);
	fixstr(rv + strlen(old), new, maxstlen(new) + 1);
	return rv;
}

/*
 * Fake a symtab entry for compound literals.
 */
static struct symtab *
clbrace(NODE *p)
{
	struct symtab *sp;

	sp = getsymtab(simname("cl"), STEMP);
	sp->stype = p->n_type;
	sp->squal = p->n_qual;
	sp->sdf = p->n_df;
	sp->sap = p->n_ap;
	tfree(p);
	if (blevel == 0 && xnf != NULL) {
		sp->sclass = STATIC;
		sp->slevel = 2;
		sp->soffset = getlab();
	} else {
		sp->sclass = blevel ? AUTO : STATIC;
		if (!ISARY(sp->stype) || sp->sdf->ddim != NOOFFSET) {
			sp->soffset = NOOFFSET;
			oalloc(sp, &autooff);
		}
	}
	beginit(sp);
	return sp;
}

char *
simname(char *s)
{
	int len = (int)strlen(s) + 10 + 1;
	char *w = tmpalloc(len);

	snprintf(w, len, "%s%d", s, getlab());
	return w;
}

NODE *
biop(int op, NODE *l, NODE *r)
{
	return block(op, l, r, INT, 0, 0);
}

static NODE *
cmop(NODE *l, NODE *r)
{
	return biop(CM, l, r);
}

static NODE *
voidcon(void)
{
	return block(ICON, NIL, NIL, STRTY, 0, 0);
}

/* Support for extended assembler a' la' gcc style follows below */

static NODE *
xmrg(NODE *out, NODE *in)
{
	NODE *p = in;

	if (p->n_op == XARG) {
		in = cmop(out, p);
	} else {
		while (p->n_left->n_op == CM)
			p = p->n_left;
		p->n_left = cmop(out, p->n_left);
	}
	return in;
}

/*
 * Put together in and out node lists in one list, and balance it with
 * the constraints on the right side of a CM node.
 */
static NODE *
xcmop(NODE *out, NODE *in, NODE *str)
{
	NODE *p, *q;

	if (out) {
		/* D out-list sanity check */
		for (p = out; p->n_op == CM; p = p->n_left) {
			q = p->n_right;
			if (q->n_name[0] != '=' && q->n_name[0] != '+')
				uerror("output missing =");
		}
		if (p->n_name[0] != '=' && p->n_name[0] != '+')
			uerror("output missing =");
		if (in == NIL)
			p = out;
		else
			p = xmrg(out, in);
	} else if (in) {
		p = in;
	} else
		p = voidcon();

	if (str == NIL)
		str = voidcon();
	return cmop(p, str);
}

/*
 * Generate a XARG node based on a string and an expression.
 */
static NODE *
xasmop(char *str, NODE *p)
{

	p = biop(XARG, p, NIL);
	p->n_name = isinlining ? newstring(str, strlen(str)) : str;
	return p;
}

/*
 * Generate a XASM node based on a string and an expression.
 */
static void
mkxasm(char *str, NODE *p)
{
	NODE *q;

	q = biop(XASM, p->n_left, p->n_right);
	q->n_name = isinlining ? newstring(str, strlen(str)) : str;
	nfree(p);
	ecomp(q);
}

static struct attr *
gcc_attr_wrapper(NODE *p)
{
#ifdef GCC_COMPAT
	return gcc_attr_parse(p);
#else
	uerror("gcc attribute used");
	return NULL;
#endif
}


#ifdef GCC_COMPAT
static NODE *
tyof(NODE *p)
{
	static struct symtab spp;
	NODE *q = block(TYPE, NIL, NIL, p->n_type, p->n_df, p->n_ap);
	q->n_qual = p->n_qual;
	q->n_sp = &spp; /* for typenode */
	tfree(p);
	return q;
}
#else
static NODE *
tyof(NODE *p)
{
	uerror("typeof gcc extension");
	return bcon(0);
}
#endif

/*
 * Rewrite ++/-- to (t=p, p++, t) ops on types that do not act act as usual.
 */
static NODE *
rewincop(NODE *p1, NODE *p2, int op)
{
	NODE *t, *r;

	t = cstknode(p1->n_type, 0, 0);
	r = buildtree(ASSIGN, ccopy(t), ccopy(p1));
	r = buildtree(COMOP, r, buildtree(op, p1, eve(p2)));
	return buildtree(COMOP, r, t);
}

/*
 * Traverse an unhandled expression tree bottom-up and call buildtree()
 * or equivalent as needed.
 */
NODE *
eve(NODE *p)
{
	struct symtab *sp;
	NODE *r, *p1, *p2;
	int x;

	p1 = p->n_left;
	p2 = p->n_right;
	switch (p->n_op) {
	case NMLIST:
	case NAME:
		sp = cxxlookup(p, SNORMAL|SNOCREAT);
		if (sp->sflags & SINLINE)
			inline_ref(sp);
		r = nametree(sp);
		if (sp->sflags & SDYNARRAY)
			r = buildtree(UMUL, r, NIL);
#ifdef GCC_COMPAT
		if (attr_find(sp->sap, GCC_ATYP_DEPRECATED))
			warner(Wdeprecated_declarations, sp->sname);
#endif
		break;

	case DOT:
	case STREF:
		r = cxxstructref(eve(p1), p->n_op, (char *)p2->n_sp);
		nfree(p2);
		break;

	case CAST:
		p1 = buildtree(CAST, p1, eve(p2));
		nfree(p1->n_left);
		r = p1->n_right;
		nfree(p1);
		break;


	case SZOF:
		x = xinline; xinline = 0; /* XXX hack */
		if (glval(p2) == 0)
			p1 = eve(p1);
		else
			TYMFIX(p1);
		nfree(p2);
		r = doszof(p1);
		xinline = x;
		break;

	case LB:
		p1 = eve(p->n_left);
		r = buildtree(UMUL, buildtree(PLUS, p1, eve(p2)), NIL);
		break;

	case COMPL:
#ifndef NO_COMPLEX
		p1 = eve(p1);
		if (ANYCX(p1))
			r = cxconj(p1);
		else
			r = buildtree(COMPL, p1, NIL);
		break;
#endif
	case UMINUS:
	case NOT:
	case UMUL:
		r = buildtree(p->n_op, eve(p->n_left), NIL);
		break;

	case ADDROF:
		r = eve(p1);
		if (ISFTN(p->n_type)/* || ISARY(p->n_type) */){
#ifdef notdef
			werror( "& before array or function: ignored" );
#endif
		} else
			r = buildtree(ADDROF, r, NIL);
		break;

	case CALL:
	case UCALL:
		if (p1->n_op == NAME || p1->n_op == NMLIST) {
			sp = cxxlookup(p1, SNORMAL);
#ifndef NO_C_BUILTINS
			if (sp->sflags & SBUILTIN) {
				nfree(p1);
				r = builtin_check(sp, p2);
				break;
			}
#endif
			if (sp->stype == UNDEF) {
				p1->n_type = FTN|INT;
				p1->n_sp = sp;
				p1->n_ap = NULL;
				defid(p1, EXTERN);
			}
			nfree(p1);
#ifdef GCC_COMPAT
			if (attr_find(sp->sap, GCC_ATYP_DEPRECATED))
				warner(Wdeprecated_declarations, sp->sname);
#endif
			p2 = p->n_op == CALL ? eve(p2) : NIL;
			r = doacall(sp, nametree(sp), p2, 0);
		} else if (p1->n_op == DOT || p1->n_op == STREF) {
			/*
			 * function as member of a struct.
			 * - check args for correct overloaded function
			 * - add hidden arg0 as pointer to this struct
			 */

			p2 = p->n_op == CALL ? eve(p2) : NIL;
			p1->n_left = eve(p1->n_left); /* eval rest */
			r = cxxmatchftn(p1, p2);
			if (p1->n_op == DOT)
				p1->n_left = buildtree(ADDROF, p1->n_left, NIL);
			nfree(p1->n_right);
			p1 = nfree(p1);
			p2 = cxxaddhidden(p2, p1);
			r = doacall(NULL, r, p2, 1);
		} else {
			p2 = p->n_op == CALL ? eve(p2) : NIL;
			r = doacall(NULL, eve(p1), p2, 0);
		}
		break;

#ifndef NO_COMPLEX
	case XREAL:
	case XIMAG:
		p1 = eve(p1);
		r = cxelem(p->n_op, p1);
		break;
#endif

	case MUL:
	case DIV:
	case PLUS:
	case MINUS:
	case ASSIGN:
	case EQ:
	case NE:
#ifndef NO_COMPLEX
		p1 = eve(p1);
		p2 = eve(p2);
		if (ANYCX(p1) || ANYCX(p2)) {
			r = cxop(p->n_op, p1, p2);
		} else if (ISITY(p1->n_type) || ISITY(p2->n_type)) {
			r = imop(p->n_op, p1, p2);
		} else
			r = buildtree(p->n_op, p1, p2);
		break;
#endif
	case MOD:
	case CM:
	case GT:
	case GE:
	case LT:
	case LE:
	case RS:
	case LS:
	case RSEQ:
	case LSEQ:
	case AND:
	case OR:
	case ER:
	case OROR:
	case ANDAND:
	case EREQ:
	case OREQ:
	case ANDEQ:
	case QUEST:
	case COLON:
		p1 = eve(p1);
eve2:		r = buildtree(p->n_op, p1, eve(p2));
		break;

	case INCR:
	case DECR:
		p1 = eve(p1);
		if (p1->n_type >= FLOAT && p1->n_type <= LDOUBLE) {
			/* ++/-- on floats isn't ((d+=1)-1) */
			/* rewrite to (t=d,d++,t) */
			/* XXX - side effects */
			r = rewincop(p1, p2, p->n_op);
			break;
		}
		if (p1->n_type != BOOL)
			goto eve2;
		/* Hey, fun.  ++ will always be 1, and -- will toggle result */
		if (p->n_op == INCR) {
			/* (t=d,d=1,t) */
			r = rewincop(p1, p2, ASSIGN);
		} else {
			/* (t=d,d^=1,t) */
			r = rewincop(p1, p2, EREQ);
		}
		break;

	case MODEQ:
	case MINUSEQ:
	case PLUSEQ:
	case MULEQ:
	case DIVEQ:
		p1 = eve(p1);
		p2 = eve(p2);
#ifndef NO_COMPLEX
		if (ANYCX(p1) || ANYCX(p2)) {
			r = cxop(UNASG p->n_op, ccopy(p1), p2);
			r = cxop(ASSIGN, p1, r);
			break;
		} else if (ISITY(p1->n_type) || ISITY(p2->n_type)) {
			r = imop(UNASG p->n_op, ccopy(p1), p2);
			r = cxop(ASSIGN, p1, r);
			break;
		}
		/* FALLTHROUGH */
#endif
		if (p1->n_type == BOOL) {
			r = buildtree(UNASG p->n_op, ccopy(p1), p2);
			r = buildtree(ASSIGN, p1, r);
		} else {
			r = buildtree(p->n_op, p1, p2);
		}
		break;

	case STRING:
		r = strend(glval(p), p->n_name);
		break;

	case COMOP:
		if (p1->n_op == GOTO) {
			/* inside ({ }), eve already called */
			r = buildtree(p->n_op, p1, p2);
		} else {
			p1 = eve(p1);
			r = buildtree(p->n_op, p1, eve(p2));
		}
		break;

	case TYPE:
	case ICON:
	case FCON:
	case TEMP:
		return p;

	case CLOP:
		r = nametree(p->n_sp);
		break;

	case DELETE:
		p1 = eve(p1);
		r = cxx_delete(p1, glval(p2));
		nfree(p2);
		break;

	case NEWKW:
		r = cxx_new(p1);
		nfree(p2);
		break;

	default:
#ifdef PCC_DEBUG
		fwalk(p, eprint, 0);
#endif
		cerror("eve");
		r = NIL;
	}
	nfree(p);
	return r;
}

int
con_e(NODE *p)
{
#ifdef WORD_ADDRESSED
	return (int)icons(optim(eve(p)));
#else
	return (int)icons(optim(rmpconv(eve(p))));
#endif
}

void
uawarn(NODE *p, char *s)
{
	if (p == 0)
		return;
	if (attrwarn)
		werror("unhandled %s attribute", s);
	tfree(p);
}

static void
dainit(NODE *d, NODE *a)
{
	if (d == NULL) {
		asginit(a);
	} else if (d->n_op == CM) {
		int is = con_e(d->n_left);
		int ie = con_e(d->n_right);
		int i;

		nfree(d);
		if (ie < is)
			uerror("negative initializer range");
		desinit(biop(LB, NIL, bcon(is)));
		for (i = is; i < ie; i++)
			asginit(ccopy(a));
		asginit(a);
	} else {
		cerror("dainit");
	}
}

/*
 * Traverse down and tymerge() where appropriate.
 */
static NODE *
tymfix(NODE *p)
{
	NODE *q;
	int o = coptype(p->n_op);

	switch (o) {
	case LTYPE:
		break;
	case UTYPE:
		p->n_left = tymfix(p->n_left);
		break;
	case BITYPE:
		p->n_left = tymfix(p->n_left);
		p->n_right = tymfix(p->n_right);
		if (p->n_op == TYMERGE) {
			q = tymerge(p->n_left, p->n_right);
			q->n_ap = attr_add(q->n_ap, p->n_ap);
			tfree(p->n_left);
			nfree(p);
			p = q;
		}
		break;
	}
	return p;
}

static NODE *
aryfix(NODE *p)
{
	NODE *q;

	for (q = p; q->n_op != NAME; q = q->n_left) {
		if (q->n_op == LB) {
			q->n_right = optim(rmpconv(eve(q->n_right)));
			if ((blevel == 0 || rpole != NULL) &&
			    !nncon(q->n_right))
				uerror("array size not constant"); 
			/*
			 * Checks according to 6.7.5.2 clause 1:
			 * "...the expression shall have an integer type."
			 * "If the expression is a constant expression,	 
			 * it shall have a value greater than zero."
			 */
			if (!ISINTEGER(q->n_right->n_type))
				werror("array size is not an integer");
			else if (q->n_right->n_op == ICON &&
			    glval(q->n_right) < 0 &&
			    glval(q->n_right) != NOOFFSET) {
					uerror("array size cannot be negative");
					slval(q->n_right, 1);
			}
		} else if (q->n_op == CALL)
			q->n_right = namekill(q->n_right, 1);
	}
	return p;
}
