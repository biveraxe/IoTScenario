#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>

#include "IoTItem.h"


IoTItem* tmpItem = new IoTItem("");  //заглушка для интеграции и создания полной картины структуры

// Лексический анализатор возвращает токены [0-255], если это неизвестны, 
// иначе одну из известных единиц кода
enum Token {
  tok_eof = -1,

  // операнды (первичные выражения: идентификаторы, числа)
  tok_identifier = -4, tok_number = -5, tok_string = -3,

  // двухсимвольные операторы бинарных операций
  tok_equal = -2, tok_notequal = -9, tok_lesseq = -10, tok_greateq = -11,  

  // управление
  tok_if = -6, tok_then = -7, tok_else = -8
};

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (Абстрактное Синтаксическое Дерево или Дерево Парсинга)
//===----------------------------------------------------------------------===//

/// ExprAST - Базовый класс для всех узлов выражений.
class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual IoTValue* exec() {return nullptr;}
  virtual int setValue(IoTValue *val) {return 0;}  // 0 - установка значения не поддерживается наследником
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
  IoTItem* Item;  // ссылка на объект модуля (прямой доступ к идентификатору указанному в сценарии), если получилось найти модуль по ID

public:
  VariableExprAST(const std::string &name, IoTItem* item) : Name(name), Item(item) {}

  int setValue(IoTValue *val) {
    Item->value = *val;  // устанавливаем значение в связанном Item модуля напрямую
    return 1;
  }

  IoTValue* exec() {
    if (Item->value.isDecimal) 
      fprintf(stderr, "Call from  VariableExprAST: %s = %f\n", Name.c_str(), Item->value.valD);
    else fprintf(stderr, "Call from  VariableExprAST: %s = %s\n", Name.c_str(), Item->value.valS.c_str());
    return &(Item->value);
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

  ~BinaryExprAST() {
    if (LHS) delete LHS;
    if (RHS) delete RHS;
    fprintf(stderr, "Call from  BinaryExprAST delete\n");
  }

  IoTValue* exec(){
    std::string printStr = "";
    if (Op == tok_equal) printStr = printStr = "==";
      else printStr = printStr + Op;
    fprintf(stderr, "Call from  BinaryExprAST: %s\n", printStr.c_str());
    
    IoTValue* rhs = RHS->exec();  // получаем значение правого операнда для возможного использования в операции присваивания

    if (Op == '=' && LHS->setValue(rhs)) {  // если установка значения не поддерживается, т.е. слева не переменная, то работаем по другим комбинациям далее
      return rhs;                           // иначе возвращаем присвоенное значение справа
    }

    IoTValue* lhs = LHS->exec();  // если присваивания не произошло, значит операция иная и необходимо значение левого операнда
    
    if (lhs != nullptr && rhs !=nullptr) {
      if (lhs->isDecimal && rhs->isDecimal) {
        switch (Op) {
          case '>':
            val.valD = lhs->valD > rhs->valD;
          break;
          case '<':
            val.valD = lhs->valD < rhs->valD;
          break;
          case tok_lesseq:
            val.valD = lhs->valD <= rhs->valD;
          break;
          case tok_greateq:
            val.valD = lhs->valD >= rhs->valD;
          break;
          case tok_equal:
            val.valD = lhs->valD == rhs->valD;
          break;
          case tok_notequal:
            val.valD = lhs->valD != rhs->valD;
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

          default:
            break;
        }
        return &val;
      }

      if (!lhs->isDecimal && !rhs->isDecimal) {
        switch (Op) {
          case tok_equal:
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

/// CallExprAST - Класс узла выражения для вызова команды.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::string Cmd;
  std::vector<ExprAST*> Args;
  IoTItem *Item;  // ссылка на объект модуля (прямой доступ к идентификатору указанному в сценарии), если получилось найти модуль по ID
  IoTValue ret;  // хранение возвращаемого значения, т.к. возврат по ссылке осуществляется

public:
  CallExprAST(const std::string &callee, std::string &cmd, std::vector<ExprAST*> &args, IoTItem *item)
    : Callee(callee), Cmd(cmd), Args(args), Item(item) {}

  IoTValue* exec() {
    fprintf(stderr, "Call from  CallExprAST ID = %s, Param = %s\n", Callee.c_str(), Cmd.c_str());
    if (Item) {
      std::vector<IoTValue> ArgsAsIoTValue;
      for (unsigned int i = 0; i < Args.size(); i++) {
        IoTValue *tmp = Args[i]->exec();
        if (tmp != nullptr) ArgsAsIoTValue.push_back(*tmp);
          else ArgsAsIoTValue.push_back({0, "", true});
      }
      ret = Item->execute(Cmd, ArgsAsIoTValue);  // вызываем команду из модуля напрямую с передачей всех аргументов
    } else ret = {0, "", true};
    return &ret;
  }

  ~CallExprAST() {
    for (unsigned int i = 0; i < Args.size(); i++) {
      if (Args[i]) delete Args[i];
    }
    Args.clear();
    fprintf(stderr, "Call from  CallExprAST delete\n");
  }
};

/// IfExprAST - Класс узла выражения для if/then/else.
class IfExprAST : public ExprAST {
  ExprAST *Cond, *Then, *Else;

public:
  IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else)
    : Cond(cond), Then(then), Else(_else) {}

  IoTValue* exec() {
    IoTValue *res_ret = nullptr;
    IoTValue *cond_ret = nullptr;
    cond_ret = Cond->exec();
    if (cond_ret != nullptr && cond_ret->isDecimal && cond_ret->valD) res_ret = Then->exec();
    else if (Else) res_ret = Else->exec();

    if (!res_ret) fprintf(stderr, "Call from  IfExprAST: Cond result = %f, no body result\n", cond_ret->valD);
    else if (res_ret->isDecimal) fprintf(stderr, "Call from  IfExprAST: Cond result = %f, result = %f\n", cond_ret->valD, res_ret->valD);
    else fprintf(stderr, "Call from  IfExprAST: Cond result = %f, result = %s\n", cond_ret->valD, res_ret->valS.c_str());
    return cond_ret;
  }

  ~IfExprAST() {
    //for (unsigned int i = 0; i < Args.size(); i++) {
    //  if (Args[i]) delete Args[i];
    //}
    //Args.clear();

    if (Cond) delete Cond;
    if (Then) delete Then;
    if (Else) delete Else;
    fprintf(stderr, "Call from  IfExprAST delete\n");
  }
};

/// BracketsExprAST - Класс узла выражения для if/then/else.
class BracketsExprAST : public ExprAST {
  std::vector<ExprAST*> BracketsList;

public:
  BracketsExprAST(std::vector<ExprAST*> &bracketsList)
    : BracketsList(bracketsList) {}

  IoTValue* exec() {
    fprintf(stderr, "Call from  BracketsExprAST OperCount = %d \n", BracketsList.size());
    
    IoTValue* lastExecValue;
    for (unsigned int i = 0; i < BracketsList.size(); i++) {
      lastExecValue = BracketsList[i]->exec();
    }

    return lastExecValue;
  }

  ~BracketsExprAST() {
    for (unsigned int i = 0; i < BracketsList.size(); i++) {
      if (BracketsList[i]) delete BracketsList[i];
    }
    BracketsList.clear();

    fprintf(stderr, "Call from  BracketsExprAST delete\n");
  }
};


class IoTScenario {

  //===----------------------------------------------------------------------===//
  // Lexer (Лексический анализатор)
  //===----------------------------------------------------------------------===//

  std::string IdentifierStr;  // Заполняется, если tok_identifier
  float NumVal;              // Заполняется, если tok_number
  int LastChar = ' ';

  /// gettok - Возвращает следующий токен из стандартного потока ввода.
  int gettok() {

    // Пропускаем пробелы.
    while (isspace(LastChar))
      LastChar = myfile->get();

    if (isalpha(LastChar)) { // идентификатор: [a-zA-Z][a-zA-Z0-9]*
      IdentifierStr = LastChar;
      while (isalnum((LastChar = myfile->get())))
        IdentifierStr += LastChar;

      if (IdentifierStr == "if") return tok_if;
      if (IdentifierStr == "then") return tok_then;
      if (IdentifierStr == "else") return tok_else;
      return tok_identifier;
    }

    if (isdigit(LastChar)) {   // Число: [0-9.]+
      std::string NumStr;
      do {
        NumStr += LastChar;
        LastChar = myfile->get();
      } while (isdigit(LastChar) || LastChar == '.');

      NumVal = strtod(NumStr.c_str(), 0);
      return tok_number;
    }

    if (LastChar == '#') {
      // Комментарий до конца строки
      do LastChar = myfile->get();
      while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
      
      if (LastChar != EOF)
        return gettok();
    }

    if (LastChar == '"') { // "строка"    
      IdentifierStr = "";
      LastChar = myfile->get();
      while (LastChar != '"') {
        IdentifierStr += LastChar;
        LastChar = myfile->get();
      }
      LastChar = myfile->get();
        
      return tok_string;
    }
    
    // Проверка конца файла.
    if (LastChar == EOF)
      return tok_eof;

    if (LastChar == '=') {
      LastChar = myfile->get();  
      if (LastChar == '=') {
        LastChar = myfile->get();
        return tok_equal;
      } else return '=';
    }

    if (LastChar == '!') {
      LastChar = myfile->get();  
      if (LastChar == '=') {
        LastChar = myfile->get();
        return tok_notequal;
      } else return '!';
    }

    if (LastChar == '<') {
      LastChar = myfile->get();  
      if (LastChar == '=') {
        LastChar = myfile->get();
        return tok_lesseq;
      } else return '<';
    }

    if (LastChar == '>') {
      LastChar = myfile->get();  
      if (LastChar == '=') {
        LastChar = myfile->get();
        return tok_greateq;
      } else return '>';
    }

    // В противном случае просто возвращаем символ как значение ASCII
    int ThisChar = LastChar;
    LastChar = myfile->get();
    return ThisChar;
  }


  //===----------------------------------------------------------------------===//
  // Parser (Парсер или Синтаксический Анализатор)
  //===----------------------------------------------------------------------===//


  /// CurTok/getNextToken - Предоставляет простой буфер токенов. CurTok - это текущий
  /// токен, просматриваемый парсером. getNextToken получает следующий токен от
  /// лексического анализатора и обновляет CurTok.
  int CurTok;
  int getNextToken() {
    return CurTok = gettok();
  }

  /// BinopPrecedence - Содержит приоритеты для бинарных операторов
  std::map<char, int> BinopPrecedence;

  /// GetTokPrecedence - Возвращает приоритет текущего бинарного оператора.
  int GetTokPrecedence() {
    if (!(isascii(CurTok) || BinopPrecedence.count(CurTok)))
      return -1;
    
    // Удостоверимся, что это объявленный бинарный оператор.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
  }

  /// Error* - Это небольшие вспомогательные функции для обработки ошибок.
  ExprAST *Error(const char *Str) { fprintf(stderr, "Error: %s\n", Str);return 0;}


  /// identifierexpr
  ///   ::= identifier
  ///   ::= identifier '(' expression* ')'
  ExprAST *ParseIdentifierExpr() {
    std::string IdName = IdentifierStr;
    std::string Cmd = "";

    getNextToken();  // получаем идентификатор.
    
    if (CurTok == '.') {
      getNextToken();
      Cmd = IdentifierStr;
      getNextToken();
    }

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
    
    return new CallExprAST(IdName, Cmd, Args, tmpItem);
  }

  /// numberexpr ::= number
  ExprAST *ParseNumberExpr() {
    ExprAST *Result = new NumberExprAST(NumVal);
    getNextToken(); // получаем число
    return Result;
  }

  /// parenexpr ::= '(' expression ')'
  ExprAST *ParseParenExpr() {
    getNextToken();  // получаем (.
    ExprAST *V = ParseExpression();
    if (!V) return 0;
    
    if (CurTok != ')')
      return Error("expected ')'");
    getNextToken();  // получаем ).
    return V;
  }

  /// bracketsexpr ::= '(' expression ')'
  ExprAST *ParseBracketsExpr() {
    getNextToken();  // получаем {.
    std::vector<ExprAST*> bracketsList;
    if (CurTok != '}') {
      while (1) {
        ExprAST *Expr = ParseExpression();
        if (!Expr) return 0;
        bracketsList.push_back(Expr);

        if (CurTok != ';')
          return Error("Expected '}' or ';' in operation list");
        getNextToken();

        if (CurTok == '}') break;
      }
    }
 
    getNextToken();  // получаем }.
    return new BracketsExprAST(bracketsList);
  }

  /// quotesexpr ::= '"' expression '"'
  ExprAST *ParseQuotesExpr() {
    std::string StringCont = IdentifierStr;
    ExprAST *Result = new StringExprAST(StringCont);
    getNextToken(); // получаем число
    return Result;
  }

  /// ifexpr ::= 'if' expression 'then' expression 'else' expression
  ExprAST *ParseIfExpr() {
    getNextToken();  // Получаем if.
    
    // условие.
    ExprAST *Cond = ParseExpression();
    if (!Cond) return 0;
    
    if (CurTok != tok_then)
      return Error("expected then");
    getNextToken();  // Получаем then
    
    ExprAST *Then = ParseExpression();
    if (Then == 0) return 0;
    
    //if (CurTok != tok_else)
    //  return Error("expected else");
    ExprAST *Else = nullptr;
    if (CurTok == tok_else) {
      getNextToken();
      Else = ParseExpression();
    }

    return new IfExprAST(Cond, Then, Else);
  }

  /// primary
  ///   ::= identifierexpr
  ///   ::= numberexpr
  ///   ::= parenexpr
  ExprAST *ParsePrimary() {
    switch (CurTok) {
    default: return Error("unknown token when expecting an expression");
    case tok_identifier: return ParseIdentifierExpr();
    case tok_number:     return ParseNumberExpr();
    case '(':            return ParseParenExpr();
    case '{':            return ParseBracketsExpr();
    case tok_string:     return ParseQuotesExpr();
    case tok_if:         return ParseIfExpr();
    }
  }

  /// binoprhs
  ///   ::= ('+' primary)*
  ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
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
  ExprAST *ParseExpression(std::vector<ExprAST*> *bracketsList = nullptr) {
    ExprAST *LHS = ParsePrimary();
    if (!LHS) return 0;
    //if (bracketsList) bracketsList->push_back(LHS);
    return ParseBinOpRHS(0, LHS);
  }

  std::vector<ExprAST*> ScenarioElements;  // корневые элементы дерава   
  std::ifstream *myfile;

  void clearScenarioElements() {  // удаляем все корневые элементы дерева AST
    for (unsigned int i = 0; i < ScenarioElements.size(); i++) {
      if (ScenarioElements[i]) delete ScenarioElements[i];
    }
    ScenarioElements.clear();
  }

public:
  void loadScenario(std::string fileName) {  // посимвольно считываем и сразу интерпретируем сценарий в дерево AST
    clearScenarioElements();  // удаляем все корневые элементы перед загрузкой новых.
    LastChar = ' ';

    myfile = new std::ifstream(fileName);
    if (myfile->is_open()) {
      getNextToken();
      while ( myfile ) {
        switch (CurTok) {
          case tok_eof:    return;
          //case ';':        getNextToken(); break;  // игнорируем верхнеуровневые точки с запятой.
          case tok_if:     ScenarioElements.push_back(ParseExpression()); break;
          default:         getNextToken(); break;
        }
      }
      myfile->close();
      delete myfile;
    }
  }

  void ExecScenario() {  // запускаем поочереди все корневые элементы выражений в сценарии, ожидаемо - это IFы
    for (unsigned int i = 0; i < ScenarioElements.size(); i++) {
      if (ScenarioElements[i]) ScenarioElements[i]->exec();
    }
  }

  IoTScenario() {
    // Задаём стандартные бинарные операторы.
    // 1 - наименьший приоритет.
    BinopPrecedence['='] = 1;  
    BinopPrecedence[tok_equal] = 3;  // ==
    BinopPrecedence[tok_notequal] = 4;  // !=
    BinopPrecedence[tok_lesseq] = 5;  // <=
    BinopPrecedence[tok_greateq] = 6;  // >=
    BinopPrecedence['<'] = 10;
    BinopPrecedence['>'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;  // highest.
  }
  
  ~IoTScenario() {}
};


//===----------------------------------------------------------------------===//
// Main driver code (Код основной программы)
//===----------------------------------------------------------------------===//

IoTScenario iotScen;

int main() {
  
  iotScen.loadScenario("d:\\IoTScenario\\scenario.txt");
  iotScen.ExecScenario();

// имитируем обновление сценария после изменения
//  iotScen.loadScenario("d:\\IoTScenario\\scenario.txt");
//  iotScen.ExecScenario();

// имитируем обновление сценария после изменения
//  iotScen.loadScenario("d:\\IoTScenario\\scenario.txt");
//  iotScen.ExecScenario();

  return 0;
}