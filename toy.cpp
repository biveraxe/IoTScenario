#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>


//===----------------------------------------------------------------------===//
// Lexer (Лексический анализатор)
//===----------------------------------------------------------------------===//

// Лексический анализатор возвращает токены [0-255], если это неизвестны, 
// иначе одну из известных единиц кода
enum Token {
  tok_eof = -1,

  // операнды (первичные выражения: идентификаторы, числа)
  tok_identifier = -4, tok_number = -5,

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
    LastChar = getchar();

  if (isalpha(LastChar)) { // идентификатор: [a-zA-Z][a-zA-Z0-9]*
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
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
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);
    return tok_number;
  }

  if (LastChar == '#') {
    // Комментарий до конца строки
    do LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
    
    if (LastChar != EOF)
      return gettok();
  }
  
  // Проверка конца файла.
  if (LastChar == EOF)
    return tok_eof;

  // В противном случае просто возвращаем символ как значение ASCII
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (Абстрактное Синтаксическое Дерево или Дерево Парсинга)
//===----------------------------------------------------------------------===//

/// ExprAST - Базовый класс для всех узлов выражений.
class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual float exec() {return 0;}
  virtual std::string getValStr() {return "";}
};

/// NumberExprAST - Класс узла выражения для числовых литералов (Например, "1.0").
class NumberExprAST : public ExprAST {
  float Val;
public:
  NumberExprAST(float val) : Val(val) {}

  float exec(){
    fprintf(stderr, "Call from  NumberExprAST: %f\n", Val);
    return Val;
  }
};

/// VariableExprAST - Класс узла выражения для переменных (например, "a").
class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(const std::string &name) : Name(name) {}

  float exec(){
    fprintf(stderr, "Call from  VariableExprAST: %s\n", Name.c_str());
  }
};

/// BinaryExprAST - Класс узла выражения для бинарных операторов.
class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs) 
    : Op(op), LHS(lhs), RHS(rhs) {}

  float exec(){
    fprintf(stderr, "Call from  BinaryExprAST: %c\n", Op);

    switch (Op) {
      case '>':
        return LHS->exec() > RHS->exec();
      break;
      case '<':
        return LHS->exec() < RHS->exec();
      break;

      case '+':
        return LHS->exec() + RHS->exec();
      break;
      case '-':
        return LHS->exec() - RHS->exec();
      break;
      case '*':
        return LHS->exec() * RHS->exec();
      break;

      default:
        break;
    }

  }
};

/// CallExprAST - Класс узла выражения для вызова функции.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}

  float exec(){
    fprintf(stderr, "Call from  CallExprAST\n");
  }
};

/// PrototypeAST - Этот класс представляет "прототип" для функции,
/// который хранит её имя и имена аргументов (и, таким образом, 
/// неявно хранится число аргументов).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
public:
  PrototypeAST(const std::string &name, const std::vector<std::string> &args)
    : Name(name), Args(args) {}

  float exec(){
    fprintf(stderr, "Call from  PrototypeAST: %s\n", Name.c_str());
  }
  
};

/// FunctionAST - Представляет определение самой функции
class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;
public:
  FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}
  
  float exec() {
    //Body;
    fprintf(stderr, "Call from  FunctionAST:");
    Proto->exec();
    Body->exec();
  }
};

/// IfExprAST - Класс узла выражения для if/then/else.
class IfExprAST : public ExprAST {
  ExprAST *Cond, *Then, *Else;
public:
  IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else)
    : Cond(cond), Then(then), Else(_else) {}

  float exec(){
    int cond_ret;
    float then_ret, else_ret;
    if (cond_ret = Cond->exec()) then_ret = Then->exec();
    else if (Else) else_ret = Else->exec();

    fprintf(stderr, "Call from  IfExprAST: Cond result = %d, then result = %f, else result = %f\n", cond_ret, then_ret, else_ret);
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
PrototypeAST *ErrorP(const char *Str) { Error(Str); return 0; }
FunctionAST *ErrorF(const char *Str) { Error(Str); return 0; }

static ExprAST *ParseExpression();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  
  getNextToken();  // получаем идентификатор.
  
  if (CurTok != '(') // Обычная переменная.
    return new VariableExprAST(IdName);
  
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

/// prototype
///   ::= id '(' id* ')'
static PrototypeAST *ParsePrototype() {
  if (CurTok != tok_identifier)
    return ErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();
  
  if (CurTok != '(')
    return ErrorP("Expected '(' in prototype");

  // Считываем список наименований аргументов.
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return ErrorP("Expected ')' in prototype");
  
  // Все отлично.
  getNextToken();  // получаем ')'.
  
  return new PrototypeAST(FnName, ArgNames);
}


/// toplevelexpr ::= expression
static FunctionAST *ParseTopLevelExpr() {
  if (ExprAST *E = ParseExpression()) {
    // Создаём анонимный прототип.
    PrototypeAST *Proto = new PrototypeAST("", std::vector<std::string>());
    return new FunctionAST(Proto, E);
  }
  return 0;
}


//===----------------------------------------------------------------------===//
// Top-Level parsing (Парсинг верхнего уровня)
//===----------------------------------------------------------------------===//

ExprAST *root;

static void HandleIf() {
  if (root = ParseIfExpr()) {
    fprintf(stderr, "Parsed an IF\n");
    root->exec();
  } else {
    // Пропускаем токен для восстановления после ошибки.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Рассчитываем верхнеуровневое выражение в анонимной функции.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Пропускаем токен для восстановления после ошибки.
    getNextToken();
  }
}


static void MainLoop() {
  while (1) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:    return;
    case ';':        getNextToken(); break;  // игнорируем верхнеуровневые точки с запятой.
    case tok_if:     HandleIf(); break;
    default:         HandleTopLevelExpression(); break;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main driver code (Код основной программы)
//===----------------------------------------------------------------------===//

int main() {
  // Задаём стандартные бинарные операторы.
  // 1 - наименьший приоритет.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['>'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;  // highest.

  fprintf(stderr, "ready> ");
  getNextToken();

  // Теперь запускаем основной "цикл интерпретатора".
  MainLoop();

  return 0;
}