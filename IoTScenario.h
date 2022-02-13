#include "IoTItem.h"
#include <map>

class ExprAST {
public:
  virtual ~ExprAST();
  virtual IoTValue* exec();
  virtual int setValue(IoTValue *val);  // ret 0 - установка значения не поддерживается наследником
};

class IoTScenario {
  //===----------------------------------------------------------------------===//
  // Lexer (Лексический анализатор)
  //===----------------------------------------------------------------------===//

  std::string IdentifierStr;  // Заполняется, если tok_identifier
  float NumVal;              // Заполняется, если tok_number
  int LastChar = ' ';

  /// gettok - Возвращает следующий токен из стандартного потока ввода.
  int gettok();

  //===----------------------------------------------------------------------===//
  // Parser (Парсер или Синтаксический Анализатор)
  //===----------------------------------------------------------------------===//

  /// CurTok/getNextToken - Предоставляет простой буфер токенов. CurTok - это текущий
  /// токен, просматриваемый парсером. getNextToken получает следующий токен от
  /// лексического анализатора и обновляет CurTok.
  int CurTok;
  int getNextToken();

  /// BinopPrecedence - Содержит приоритеты для бинарных операторов
  std::map<char, int> BinopPrecedence;

  /// GetTokPrecedence - Возвращает приоритет текущего бинарного оператора.
  int GetTokPrecedence();

  /// Error* - Это небольшие вспомогательные функции для обработки ошибок.
  ExprAST *Error(const char *Str);

  /// identifierexpr
  ///   ::= identifier
  ///   ::= identifier '(' expression* ')'
  ExprAST *ParseIdentifierExpr();

  /// numberexpr ::= number
  ExprAST *ParseNumberExpr();

  /// parenexpr ::= '(' expression ')'
  ExprAST *ParseParenExpr();

  /// bracketsexpr ::= '{' expression '}'
  ExprAST *ParseBracketsExpr();

  /// quotesexpr ::= '"' expression '"'
  ExprAST *ParseQuotesExpr();

  /// ifexpr ::= 'if' expression 'then' expression 'else' expression
  ExprAST *ParseIfExpr();

  /// primary
  ///   ::= identifierexpr
  ///   ::= numberexpr
  ///   ::= parenexpr
  ExprAST *ParsePrimary();

  /// binoprhs
  ///   ::= ('+' primary)*
  ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS);

  /// expression
  ///   ::= primary binoprhs
  ///
  ExprAST *ParseExpression();

  std::vector<ExprAST*> ScenarioElements;  // корневые элементы дерава   
  std::ifstream *myfile;

  void clearScenarioElements();

public:
  void loadScenario(std::string fileName);
  void ExecScenario();

  IoTScenario();
  ~IoTScenario();
};