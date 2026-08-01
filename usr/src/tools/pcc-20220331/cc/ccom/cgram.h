/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

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
    C_ENUM = 289,                  /* C_ENUM  */
    C_ELLIPSIS = 290,              /* C_ELLIPSIS  */
    C_QUALIFIER = 291,             /* C_QUALIFIER  */
    C_FUNSPEC = 292,               /* C_FUNSPEC  */
    C_ASM = 293,                   /* C_ASM  */
    NOMATCH = 294,                 /* NOMATCH  */
    C_TYPEOF = 295,                /* C_TYPEOF  */
    C_ATTRIBUTE = 296,             /* C_ATTRIBUTE  */
    PCC_OFFSETOF = 297,            /* PCC_OFFSETOF  */
    GCC_DESIG = 298,               /* GCC_DESIG  */
    C_STATICASSERT = 299,          /* C_STATICASSERT  */
    C_ALIGNAS = 300,               /* C_ALIGNAS  */
    C_ALIGNOF = 301,               /* C_ALIGNOF  */
    C_GENERIC = 302,               /* C_GENERIC  */
    C_ATOMIC = 303                 /* C_ATOMIC  */
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
#define C_ENUM 289
#define C_ELLIPSIS 290
#define C_QUALIFIER 291
#define C_FUNSPEC 292
#define C_ASM 293
#define NOMATCH 294
#define C_TYPEOF 295
#define C_ATTRIBUTE 296
#define PCC_OFFSETOF 297
#define GCC_DESIG 298
#define C_STATICASSERT 299
#define C_ALIGNAS 300
#define C_ALIGNOF 301
#define C_GENERIC 302
#define C_ATOMIC 303

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 251 "./cgram.y"

	TWORD type;
	int intval;
	P1ND *nodep;
	struct symtab *symp;
	struct rstack *rp;
	char *strp;
	struct bks *bkp;
	struct flt flt;
	struct lexint li;
	struct genlist *g;

#line 176 "y.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
