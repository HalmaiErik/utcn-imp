// This file is part of the IMP project.

#include <sstream>

#include "parser.h"
#include "lexer.h"
#include "ast.h"



// -----------------------------------------------------------------------------
static std::string FormatMessage(const Location &loc, const std::string &msg)
{
  std::ostringstream os;
  os << "[" << loc.Name << ":" << loc.Line << ":" << loc.Column << "] " << msg;
  return os.str();
}

// -----------------------------------------------------------------------------
ParserError::ParserError(const Location &loc, const std::string &msg)
  : std::runtime_error(FormatMessage(loc, msg))
{
}


// -----------------------------------------------------------------------------
Parser::Parser(Lexer &lexer)
  : lexer_(lexer)
{
}

// -----------------------------------------------------------------------------
std::shared_ptr<Module> Parser::ParseModule()
{
  std::vector<TopLevelStmt> body;
  while (auto tk = Current()) {
    if (tk.Is(Token::Kind::FUNC)) {
      // Parse a function prototype or declaration.
      std::string name(Expect(Token::Kind::IDENT).GetIdent());
      Expect(Token::Kind::LPAREN);

      std::vector<std::pair<std::string, std::string>> args;
      while (!lexer_.Next().Is(Token::Kind::RPAREN)) {
        std::string arg(Current().GetIdent());
        Expect(Token::Kind::COLON);
        std::string type(Expect(Token::Kind::IDENT).GetIdent());
        args.emplace_back(arg, type);

        if (!lexer_.Next().Is(Token::Kind::COMMA)) {
          break;
        }
      }
      Check(Token::Kind::RPAREN);

      Expect(Token::Kind::COLON);
      std::string type(Expect(Token::Kind::IDENT).GetIdent());

      if (lexer_.Next().Is(Token::Kind::EQUAL)) {
        std::string primitive(Expect(Token::Kind::STRING).GetString());
        lexer_.Next();
        body.push_back(std::make_shared<ProtoDecl>(
            name,
            std::move(args),
            type,
            primitive
        ));
      } else {
        auto block = ParseBlockStmt();
        body.push_back(std::make_shared<FuncDecl>(
            name,
            std::move(args),
            type,
            block
        ));
      }
    } else {
      // Parse a top-level statement.
      body.push_back(ParseStmt());
    }
  }
  return std::make_unique<Module>(std::move(body));
}

// -----------------------------------------------------------------------------
std::shared_ptr<Stmt> Parser::ParseStmt()
{
  auto tk = Current();
  switch (tk.GetKind()) {
    case Token::Kind::RETURN: return ParseReturnStmt();
    case Token::Kind::WHILE: return ParseWhileStmt();
		case Token::Kind::IF: return ParseIfStmt();
		case Token::Kind::LET: return ParseLetStmt();
    case Token::Kind::LBRACE: return ParseBlockStmt();
    default: {
			auto expr = std::make_shared<ExprStmt>(ParseExpr());
			Check(Token::Kind::SEMI);
			lexer_.Next();
			return expr;
		}
  }
}

// -----------------------------------------------------------------------------
std::shared_ptr<BlockStmt> Parser::ParseBlockStmt()
{
  Check(Token::Kind::LBRACE);
	lexer_.Next();
  std::vector<std::shared_ptr<Stmt>> body;
  while (!Current().Is(Token::Kind::RBRACE)) {
    body.push_back(ParseStmt());
  }
  Check(Token::Kind::RBRACE);
  lexer_.Next();
  return std::make_shared<BlockStmt>(std::move(body));
}

// -----------------------------------------------------------------------------
std::shared_ptr<ReturnStmt> Parser::ParseReturnStmt()
{
  Check(Token::Kind::RETURN);
  lexer_.Next();
  auto expr = ParseExpr();
	Check(Token::Kind::SEMI);
	lexer_.Next();
  return std::make_shared<ReturnStmt>(expr);
}

// -----------------------------------------------------------------------------
std::shared_ptr<WhileStmt> Parser::ParseWhileStmt()
{
  Check(Token::Kind::WHILE);
  Expect(Token::Kind::LPAREN);
  lexer_.Next();
  auto cond = ParseExpr();
  Check(Token::Kind::RPAREN);
  lexer_.Next();
  auto stmt = ParseStmt();
  return std::make_shared<WhileStmt>(cond, stmt);
}

// -----------------------------------------------------------------------------
std::shared_ptr<IfStmt> Parser::ParseIfStmt()
{
  Check(Token::Kind::IF);
  Expect(Token::Kind::LPAREN);
  lexer_.Next();
  auto cond = ParseExpr();
  Check(Token::Kind::RPAREN);
  lexer_.Next();
  auto ifStmt = ParseStmt();

	// Optional else branch
	std::shared_ptr<Stmt> elseStmt = nullptr;
	const auto &tk = Current();
	if (tk.GetKind() == Token::Kind::ELSE) {
		lexer_.Next();
		elseStmt = ParseStmt();
	} 
	
  return std::make_shared<IfStmt>(cond, ifStmt, elseStmt);
}

// -----------------------------------------------------------------------------
std::shared_ptr<LetStmt> Parser::ParseLetStmt()
{
  Check(Token::Kind::LET);

  std::pair<std::string, std::string> varDec;
  std::string name(Current().GetIdent());
  std::string type(Expect(Token::Kind::IDENT).GetIdent());
  varDec.emplace_back(arg, type);
	
	Expect(Token::Kind::EQUAL);
	lexer_.Next();
	auto varVal = ParseExpr();
	
	return std::make_shared<LetStmt>(varDec, varVal);
}


// -----------------------------------------------------------------------------
std::shared_ptr<Expr> Parser::ParseTermExpr()
{
  auto tk = Current();
  switch (tk.GetKind()) {
    case Token::Kind::IDENT: {
      std::string ident(tk.GetIdent());
      lexer_.Next();
      return std::static_pointer_cast<Expr>(
          std::make_shared<RefExpr>(ident)
      );
    }
		case Token::Kind::INT: {
			uint64_t val = tk.GetInt();
			lexer_.Next();
			return std::static_pointer_cast<Expr>(
					std::make_shared<IntExpr>(val)
			);
		}
    default: {
      std::ostringstream os;
      os << "unexpected " << tk << ", expecting term";
      Error(tk.GetLocation(), os.str());
    }
  }
}

// -----------------------------------------------------------------------------
std::shared_ptr<Expr> Parser::ParseCallExpr()
{
  std::shared_ptr<Expr> callee = ParseTermExpr();
  while (Current().Is(Token::Kind::LPAREN)) {
    std::vector<std::shared_ptr<Expr>> args;
    while (!lexer_.Next().Is(Token::Kind::RPAREN)) {
      args.push_back(ParseExpr());
      if (!Current().Is(Token::Kind::COMMA)) {
        break;
      }
    }
    Check(Token::Kind::RPAREN);
    lexer_.Next();
    callee = std::make_shared<CallExpr>(callee, std::move(args));
  }
  return callee;
}

// -----------------------------------------------------------------------------
std::shared_ptr<Expr> Parser::ParseMulExpr()
{
	std::shared_ptr<Expr> term = ParseCallExpr();

	while (Current().Is(Token::Kind::MUL)) {
    lexer_.Next();
    auto rhs = ParseCallExpr();
    term = std::make_shared<BinaryExpr>(BinaryExpr::Kind::MUL, term, rhs);
  }

	return term;
}

// -----------------------------------------------------------------------------
std::shared_ptr<Expr> Parser::ParseAddSubExpr()
{
  std::shared_ptr<Expr> term = ParseMulExpr();

	// Addition & Subtraction
  while (Current().Is(Token::Kind::PLUS) || Current().Is(Token::Kind::SUB)) {
		const bool isAddition = Current().Is(Token::Kind::PLUS);

    lexer_.Next();
    auto rhs = ParseMulExpr();

		if (isAddition) {
    	term = std::make_shared<BinaryExpr>(BinaryExpr::Kind::ADD, term, rhs);
		}
		else {
			term = std::make_shared<BinaryExpr>(BinaryExpr::Kind::SUB, term, rhs);
		}
  }

  return term;
}

// -----------------------------------------------------------------------------
std::shared_ptr<Expr> Parser::ParseEqualityExpr()
{
	std::shared_ptr<Expr> term = ParseAddSubExpr();
	
	while(Current().Is(Token::Kind::EQUALS)) {
		lexer_.Next();
		auto rhs = ParseAddSubExpr();
		term = std::make_shared<BinaryExpr>(BinaryExpr::Kind::EQUALS, term, rhs);
	}

	return term;
}

// -----------------------------------------------------------------------------
const Token &Parser::Expect(Token::Kind kind)
{
  lexer_.Next();
  return Check(kind);
}

// -----------------------------------------------------------------------------
const Token &Parser::Check(Token::Kind kind)
{
  const auto &tk = Current();
  if (kind != tk.GetKind()) {
    std::ostringstream os;
    os << "unexpected " << tk << ", expecting " << kind;
    Error(tk.GetLocation(), os.str());
  }
  return tk;
}

// -----------------------------------------------------------------------------
void Parser::Error(const Location &loc, const std::string &msg)
{
  throw ParserError(loc, msg);
}
