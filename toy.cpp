#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

#include "IoTItem.h"


IoTItem* tmpItem = new IoTItem("");  //заглушка для интеграции и создания полной картины структуры

std::ifstream myfile("d:\\IoTScenario\\scenario.txt");

//===----------------------------------------------------------------------===//
// Lexer (Лексический анализатор)
//===----------------------------------------------------------------------===//

// Лексический анализатор возвращает токены [0-255], если это неизвестны, 
// иначе одну из известных единиц кода
enum Token {
  tok_eof = -1,

  // операнды (первичные выражения: идентификаторы, числа)
  tok_identifier = -4, tok_number = -5, tok_string = -3,

  // управление
  tok_if = -6, tok_then = -7, tok_else = -8
};

static std::string IdentifierStr;  // Заполняется, если tok_identifier
static float NumVal;              // Заполняется, если tok_number

/// gettok - Возвращает следующий токен из стандартного потока ввода.
static int gettok() {
  static int LastChar = ' ';

  // Пропускаем пробелы.
  while (isspace(LastChar))
    LastChar = myfile.get();

  if (isalpha(LastChar)) { // идентификатор: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = myfile.get())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "if") return tok_if;
    if (IdentifierStr == "then") return tok_then;
    if (IdentifierStr == "else") return tok_else;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') {   // Число: [0-9.]+
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = myfile.get();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }

  if (LastChar == '#') {
    // Комментарий до конца строки
    do LastChar = myfile.get();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    
    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == '"') { // "строка"    
    IdentifierStr = "";
    LastChar = myfile.get();
    while (LastChar != '"') {
      IdentifierStr += LastChar;
      LastChar = myfile.get();
    }
    LastChar = myfile.get();
      
    return tok_string;
  }
  
  // Проверка конца файла.
  if (LastChar == EOF)
    return tok_eof;

  // В противном случае просто возвращаем символ как значение ASCII
  int ThisChar = LastChar;
  LastChar = myfile.get();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (Абстрактное Синтаксическое Дерево или Дерево Парсинга)
//===----------------------------------------------------------------------===//

/// ExprAST - Базовый класс для всех узлов выражений.
class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual IoTValue* exec() {return nullptr;}
};

/// NumberExprAST - Класс узла выражения для числовых литералов (Например, "1.0").
class NumberExprAST : public ExprAST {
  IoTValue Val;
public:
  NumberExprAST(float val) { Val.valD = val;}

  IoTValue* exec(){
    fprintf(stderr, "Call from  NumberExprAST: %f\n", Val.valD);
    return &Val;
  }
};

/// StringExprAST - Класс узла выражения для строковых литералов (Например, "Example with spaces and quotes").
class StringExprAST : public ExprAST {
  IoTValue Val;
public:
  StringExprAST(std::string &val) { Val.isDecimal = false; Val.valS = val;}

  IoTValue* exec(){
    fprintf(stderr, "Call from  StringExprAST: %s\n", Val.valS.c_str());
    return &Val;
  }
};

/// VariableExprAST - Класс узла выражения для переменных (например, "a").
class VariableExprAST : public ExprAST {
  std::string Name;
  IoTValue* Value;  // ссылка на объект значения модуля (прямой доступ к идентификатору указанному в сценарии), если получилось найти модуль по ID

public:
  VariableExprAST(const std::string &name, IoTItem* item) : Name(name), Value(item->getValue()) {}

  IoTValue* exec() {
    if (Value->isDecimal) 
      fprintf(stderr, "Call from  VariableExprAST: %s = %d\n", Name.c_str(), Value->valD);
    else fprintf(stderr, "Call from  VariableExprAST: %s = %s\n", Name.c_str(), Value->valS.c_str());
    return Value;
  }
};

/// BinaryExprAST - Класс узла выражения для бинарных операторов.
class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
  IoTValue val;

public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs) 
    : Op(op), LHS(lhs), RHS(rhs) {}

  IoTValue* exec(){
    fprintf(stderr, "Call from  BinaryExprAST: %c\n", Op);
    IoTValue* lhs = LHS->exec();
    IoTValue* rhs = RHS->exec();
    if (lhs != nullptr && rhs !=nullptr) {
      if (lhs->isDecimal && rhs->isDecimal) {
        switch (Op) {
          case '>':
            val.valD = lhs->valD > rhs->valD;
          break;
          case '<':
            val.valD = lhs->valD < rhs->valD;
          break;

          case '+':
            val.valD = lhs->valD + rhs->valD;
          break;
          case '-':
            val.valD = lhs->valD - rhs->valD;
          break;
          case '*':
            val.valD = lhs->valD * rhs->valD;
          break;
          case '=':
            val.valD = lhs->valD == rhs->valD;
          break;

          default:
            break;
        }
        return &val;
      }

      if (!lhs->isDecimal && !rhs->isDecimal) {
        switch (Op) {
          case '=':
            val.valD = lhs->valS == rhs->valS;
          break;

          default:
            break;
        }
        return &val;
      }
    }
    return nullptr;
  }
};

/// CallExprAST - Класс узла выражения для вызова функции.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}

  IoTValue* exec() {
    fprintf(stderr, "Call from  CallExprAST\n");
  }
};

/// CmdBlockExprAST - Класс узла блока команд {..}.
class CmdBlockExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CmdBlockExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}

  IoTValue* exec() {
    fprintf(stderr, "Call from  CallExprAST\n");
  }
};

/// IfExprAST - Класс узла выражения для if/then/else.
class IfExprAST : public ExprAST {
  ExprAST *Cond, *Then, *Else;
  IoTValue* cond_ret;
public:
  IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else)
    : Cond(cond), Then(then), Else(_else) {}

  IoTValue* exec() {
    IoTValue *then_ret, *else_ret;
    cond_ret = Cond->exec();
    if (cond_ret != nullptr && cond_ret->isDecimal && cond_ret->valD) then_ret = Then->exec();
    else if (Else) else_ret = Else->exec();

    fprintf(stderr, "Call from  IfExprAST: Cond result = %d, then result = %f, else result = %f\n", cond_ret->valD, then_ret->valD, else_ret->valD);
    return cond_ret;
  }
};

//===----------------------------------------------------------------------===//
// Parser (Парсер или Синтаксический Анализатор)
//===----------------------------------------------------------------------===//


/// CurTok/getNextToken - Предоставляет простой буфер токенов. CurTok - это текущий
/// токен, просматриваемый парсером. getNextToken получает следующий токен от
/// лексического анализатора и обновляет CurTok.
static int CurTok;
static int getNextToken() {
  return CurTok = gettok();
}

/// BinopPrecedence - Содержит приоритеты для бинарных операторов
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Возвращает приоритет текущего бинарного оператора.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;
  
  // Удостоверимся, что это объявленный бинарный оператор.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

/// Error* - Это небольшие вспомогательные функции для обработки ошибок.
ExprAST *Error(const char *Str) { fprintf(stderr, "Error: %s\n", Str);return 0;}

static ExprAST *ParseExpression();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  
  getNextToken();  // получаем идентификатор.
  
  if (CurTok != '(') // Обычная переменная.
    return new VariableExprAST(IdName, tmpItem);
  
  // Вызов функции.
  getNextToken();  // получаем (
  std::vector<ExprAST*> Args;
  if (CurTok != ')') {
    while (1) {
      ExprAST *Arg = ParseExpression();
      if (!Arg) return 0;
      Args.push_back(Arg);

      if (CurTok == ')') break;

      if (CurTok != ',')
        return Error("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Получаем ')'.
  getNextToken();
  
  return new CallExprAST(IdName, Args);
}

/// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
  ExprAST *Result = new NumberExprAST(NumVal);
  getNextToken(); // получаем число
  return Result;
}

/// parenexpr ::= '(' expression ')'
static ExprAST *ParseParenExpr() {
  getNextToken();  // получаем (.
  ExprAST *V = ParseExpression();
  if (!V) return 0;
  
  if (CurTok != ')')
    return Error("expected ')'");
  getNextToken();  // получаем ).
  return V;
}

/// quotesexpr ::= '"' expression '"'
static ExprAST *ParseQuotesExpr() {
  std::string StringCont = IdentifierStr;
  ExprAST *Result = new StringExprAST(StringCont);
  getNextToken(); // получаем число
  return Result;
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
static ExprAST *ParseIfExpr() {
  getNextToken();  // Получаем if.
  
  // условие.
  ExprAST *Cond = ParseExpression();
  if (!Cond) return 0;
  
  if (CurTok != tok_then)
    return Error("expected then");
  getNextToken();  // Получаем then
  
  ExprAST *Then = ParseExpression();
  if (Then == 0) return 0;
  
  if (CurTok != tok_else)
    return Error("expected else");
  
  getNextToken();
  
  ExprAST *Else = ParseExpression();
  if (!Else) return 0;
  
  return new IfExprAST(Cond, Then, Else);
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static ExprAST *ParsePrimary() {
  switch (CurTok) {
  default: return Error("unknown token when expecting an expression");
  case tok_identifier: return ParseIdentifierExpr();
  case tok_number:     return ParseNumberExpr();
  case '(':            return ParseParenExpr();
  case tok_string:     return ParseQuotesExpr();
  case tok_if:         return ParseIfExpr();
  }
}

/// binoprhs
///   ::= ('+' primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
  // Если это бинарный оператор, получаем его приоритет
  while (1) {
    int TokPrec = GetTokPrecedence();
    
    // Если этот бинарный оператор связывает выражения по крайней мере так же, 
    // как текущий, то используем его
    if (TokPrec < ExprPrec)
      return LHS;
    
    // Отлично, мы знаем, что это бинарный оператор.
    int BinOp = CurTok;
    getNextToken();  // eat binop
    
    // Разобрать первичное выражение после бинарного оператора
    ExprAST *RHS = ParsePrimary();
    if (!RHS) return 0;
    
    // Если BinOp связан с RHS меньшим приоритетом, чем оператор после RHS, 
    // то берём часть вместе с RHS как LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec+1, RHS);
      if (RHS == 0) return 0;
    }
    
    // Собираем LHS/RHS.
    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}


/// expression
///   ::= primary binoprhs
///
static ExprAST *ParseExpression() {
  ExprAST *LHS = ParsePrimary();
  if (!LHS) return 0;
  
  return ParseBinOpRHS(0, LHS);
}



//===----------------------------------------------------------------------===//
// Top-Level parsing (Парсинг верхнего уровня)
//===----------------------------------------------------------------------===//

static void HandleIf() {
  if (ParseIfExpr()) {
    
  } else {
    // Пропускаем токен для восстановления после ошибки.
    getNextToken();
  }
}



//===----------------------------------------------------------------------===//
// Main driver code (Код основной программы)
//===----------------------------------------------------------------------===//

std::vector<ExprAST*> ScenarioElements;  // корневые элементы дерава   

void loadScenario() {
  if (myfile.is_open()) {
    getNextToken();
    while ( myfile ) {
      switch (CurTok) {
        case tok_eof:    return;
        //case ';':        getNextToken(); break;  // игнорируем верхнеуровневые точки с запятой.
        case tok_if:     ScenarioElements.push_back(ParseExpression()); break;
        default:         getNextToken(); break;
      }
    }
    myfile.close();
  }
}


int main() {
  // Задаём стандартные бинарные операторы.
  // 1 - наименьший приоритет.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['>'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  
  BinopPrecedence['='] = 60;  // highest.
  
  loadScenario();

  for (unsigned int i = 0; i < ScenarioElements.size(); i++) {
    if (ScenarioElements[i]) ScenarioElements[i]->exec();
  }

  return 0;
}