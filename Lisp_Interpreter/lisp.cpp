#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using std::string;
using std::unordered_map;
using std::vector;

class LispException : public std::exception
{
public:
    LispException(const string &msg) : message_(msg) {}

    virtual const char *what() const throw()
    {
        return message_.c_str();
    }

private:
    string message_;
};

enum class TokenKind {
        BeginParenthesis,
        EndParenthesis,
        Nil,
        Num,
        String,
        Symbol,
        Quote,
        True,
};

struct LispToken {
        TokenKind kind;
        string value;
};

enum class ExprKind {
        Function,
        Num,
        Nil,
        Pair,
        Primitive,
        String,
        Symbol,
        True,
};

class LispExprInterface {
public:
        virtual ExprKind Type() const = 0;
        virtual string ToString() const = 0;
};

class NumExpr : public LispExprInterface {
public:
        explicit NumExpr(float val): val_(val) {}

        virtual ExprKind Type() const { return ExprKind::Num; }

        virtual string ToString() const
        {
                std::ostringstream ss;
                ss << val_;

                return (ss.str());
        }

        const float& Val() const { return val_; }
private:
        const float val_;
};

class StrExpr : public LispExprInterface {
public:
        explicit StrExpr(const string& s): s_(s) {}

        virtual ExprKind Type() const { return ExprKind::String; }

        virtual string ToString() const
        {
                std::ostringstream ss;
                ss << '"' << s_ << '"';

                return (ss.str());
        }

private:
        const string s_;
};

class NilExpr : public LispExprInterface {
public:
        explicit NilExpr(NilExpr const&)        = delete;
        void operator=(NilExpr const&)          = delete;

        static NilExpr& GetInstance()
        {
                static NilExpr instance;

                return (instance);
        }

        virtual ExprKind Type() const { return ExprKind::Nil; }

        virtual string ToString() const { return "()"; }
private:
        explicit NilExpr() {}
};

class TrueExpr : public LispExprInterface {
public:
        explicit TrueExpr(TrueExpr const&)      = delete;
        void operator=(TrueExpr const&)         = delete;

        static TrueExpr& GetInstance()
        {
                static TrueExpr instance;

                return (instance);
        }

        virtual ExprKind Type() const { return ExprKind::True; }

        virtual string ToString() const { return "T"; }
private:
        explicit TrueExpr() {}
};

const NilExpr *kNil = &NilExpr::GetInstance();
const TrueExpr *kTrue = &TrueExpr::GetInstance();

class SymbolExpr : public LispExprInterface {
public:
        explicit SymbolExpr(SymbolExpr const&)  = delete;
        void operator=(SymbolExpr const&)       = delete;

        static const SymbolExpr *GetInstance(const string& sym)
        {
                static unordered_map<string, const SymbolExpr *> m_;
                const SymbolExpr *symp;
                string s;

                // A symbol is not case sensitive.
                for (auto c : sym)
                        s += toupper(c);

                auto search = m_.find(s);

                if (search != m_.end())
                        return (search->second);

                symp = new SymbolExpr(s);
                m_[symp->s_] = symp;

                return (symp);
        }

        virtual ExprKind Type() const { return ExprKind::Symbol; }

        virtual string ToString() const { return s_; }

        bool IsEqual(const string& s) const { return (s == s_); }
private:
        explicit SymbolExpr(const string& s) : s_(s) {}

        const string s_;
};

class PairExpr : public LispExprInterface {
public:
        explicit PairExpr(const LispExprInterface *first,
                          const LispExprInterface *second) :
                first_(first), second_(second)
        {}

        const LispExprInterface *First() const { return first_; }
        const LispExprInterface *Second() const { return second_; }

        virtual ExprKind Type() const { return ExprKind::Pair; }

        virtual string ToString() const {
                std::ostringstream ss;
                const PairExpr *p = this;

                ss << '(';
                for (;;) {
                        ss << p->first_->ToString();
                        if (p->second_->Type() == ExprKind::Pair) {
                                ss << ' ';
                                p = static_cast<const PairExpr *>(p->second_);
                        } else {
                                if (p->second_->Type() != ExprKind::Nil)
                                        ss << " . " << p->second_->ToString();
                                break;
                        }
                }

                ss << ')';

                return (ss.str());
        }
private:
        const LispExprInterface *first_;
        const LispExprInterface *second_;
};

typedef const LispExprInterface *(PrimFun) (vector<const LispExprInterface *>&);

class PrimExpr : public LispExprInterface {
public:
        explicit PrimExpr(const PrimFun* fn, const string& name) :
                fn_(fn), name_(name) {}

        virtual ExprKind Type() const { return ExprKind::Primitive; }

        virtual string ToString() const { return name_; }

        const PrimFun* Impl() const { return (fn_); }
private:
        const PrimFun* fn_;
        const string name_;
};

class Env {
public:
        explicit Env() : next_(NULL) {}
        explicit Env(Env *next) : next_(next) {}

        void define(const SymbolExpr *s, const LispExprInterface *e)
        {
                table_[s] = e;
        }

        const LispExprInterface *lookup(const SymbolExpr *s)
        {

                for (auto env = this; env; env = env->next_) {
                        auto search = env->table_.find(s);
                        if (search != env->table_.end())
                                return (search->second);
                }

                return (NULL);
        }
private:
        unordered_map<const SymbolExpr *, const LispExprInterface *> table_;
        Env *next_;
};

class FunExpr : public LispExprInterface {
public:
        explicit FunExpr(vector<const SymbolExpr *>& params,
                         const LispExprInterface *body,
                         const SymbolExpr *name) :
                params_(params), body_(body), name_(name)
        {}

        explicit FunExpr(vector<const SymbolExpr *>& params,
                         const LispExprInterface *body) :
                params_(params), body_(body), name_(NULL)
        {}

        virtual ExprKind Type() const { return ExprKind::Function; }

        virtual string ToString() const
        {
                std::ostringstream ss;
                ss << "<procedure";
                if (name_ != NULL) {
                        ss << ':' << name_->ToString();
                }
                ss << '>';

                return (ss.str());
        }

        const vector<const SymbolExpr *>& params() const { return params_; }
        const LispExprInterface *body() const { return body_; }
        const SymbolExpr *name() const { return name_; }
private:
        const vector<const SymbolExpr *> params_;
        const LispExprInterface *body_;
        const SymbolExpr *name_;
};

const void
AssertArgsNum(const string& name,
              vector<const LispExprInterface *>& args,
              size_t n)
{
        if (args.size() == n)
                return;
        throw LispException(name + ": Wrong number of arguments.");
}

const NumExpr *NumCast(const LispExprInterface *e)
{

        if (e->Type() != ExprKind::Num)
                throw LispException("Not a number: " + e->ToString());

        return (static_cast<const NumExpr *>(e));
}

const SymbolExpr *SymbolCast(const LispExprInterface *e)
{

        if (e->Type() != ExprKind::Symbol)
                throw LispException("Not a symbol: " + e->ToString());

        return (static_cast<const SymbolExpr *>(e));
}

const LispExprInterface *PrimPlus(vector<const LispExprInterface *>& args)
{
        float acc = 0;

        for (auto e: args)
                acc += NumCast(e)->Val();

        return (new NumExpr(acc));        
}

const LispExprInterface *PrimMinus(vector<const LispExprInterface *>& args)
{
        float acc;
        
        if (args.size() == 0)
                throw LispException("-: Expects at least one argument.");

        acc = NumCast(args[0])->Val();        
        if (args.size() == 1)
                return (new NumExpr(-acc));

        for (size_t i = 1; i < args.size(); i++)
                acc -= NumCast(args[i])->Val();

        return (new NumExpr(acc));
}

const LispExprInterface *PrimMul(vector<const LispExprInterface *>& args)
{
        float acc = 1;

        for (auto e: args)
                acc *= NumCast(e)->Val();

        return (new NumExpr(acc));
}

inline const float Div(const float& n, const float& d)
{

        if (d == 0.0)
                throw LispException("/: Division by zero.");
        return (n / d);        
}

const LispExprInterface *PrimDiv(vector<const LispExprInterface *>& args)
{
        float acc;
        
        if (args.size() == 0)
                throw LispException("/: Expects at least one argument.");

        acc = NumCast(args[0])->Val();
        if (args.size() == 1)
                return (new NumExpr(Div(1, acc)));

        for (size_t i = 1; i < args.size(); i++)
                acc = Div(acc, NumCast(args[i])->Val());

        return (new NumExpr(acc));
}

const LispExprInterface *PrimCons(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("CONS", args, 2);
        return (new PairExpr(args[0], args[1]));
}

const PairExpr *PairCast(const LispExprInterface *e)
{

        if (e->Type() != ExprKind::Pair)
                throw LispException("Not a pair: " + e->ToString());

        return (static_cast<const PairExpr *>(e));
}

const LispExprInterface *PrimCar(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("CAR", args, 1);
        return (PairCast(args[0])->First());
}

const LispExprInterface *PrimCdr(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("CDR", args, 1);
        return (PairCast(args[0])->Second());
}

const LispExprInterface *
IsType(const string& name,
       vector<const LispExprInterface *>& args,
       ExprKind kind)
{
        AssertArgsNum(name, args, 1);
        if (args[0]->Type() == kind)
                return (kTrue);
        return (kNil);
}

const LispExprInterface *PrimIsNum(vector<const LispExprInterface *>& args)
{

        return (IsType("NUMBER?", args, ExprKind::Num));
}

const LispExprInterface *PrimIsSym(vector<const LispExprInterface *>& args)
{

        return (IsType("SYMBOL?", args, ExprKind::Symbol));
}

const LispExprInterface *PrimIsList(vector<const LispExprInterface *>& args)
{

        return (IsType("LIST?", args, ExprKind::Pair));
}

const LispExprInterface *PrimIsNil(vector<const LispExprInterface *>& args)
{

        return (IsType("NIL?", args, ExprKind::Nil));
}

const LispExprInterface *PrimIsAnd(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("AND?", args, 2);
        if (args[0]->Type() != ExprKind::Nil &&
            args[1]->Type() != ExprKind::Nil)
                return (kTrue);
        return (kNil);
}

const LispExprInterface *PrimIsOr(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("OR?", args, 2);
        if (args[0]->Type() == ExprKind::Nil &&
            args[1]->Type() == ExprKind::Nil)
                return (kNil);
        return (kTrue);
}

const LispExprInterface *PrimIsEq(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("EQ?", args, 2);

        if (args[0]->Type() == args[1]->Type() &&
            ((args[0]->Type() == ExprKind::Num &&
              NumCast(args[0])->Val() == NumCast(args[1])->Val()) ||
             args[0] == args[1]))
                return (kTrue);
        return (kNil);
}

const LispExprInterface *PrimIsEqNum(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("=", args, 2);

        if (NumCast(args[0])->Val() == NumCast(args[1])->Val())
                return (kTrue);
        return (kNil);
}

const LispExprInterface *PrimIsLtNum(vector<const LispExprInterface *>& args)
{
        AssertArgsNum("<", args, 2);

        if (NumCast(args[0])->Val() < NumCast(args[1])->Val())
                return (kTrue);
        return (kNil);
}

const LispExprInterface *PrimIsGtNum(vector<const LispExprInterface *>& args)
{
        AssertArgsNum(">", args, 2);

        if (NumCast(args[0])->Val() > NumCast(args[1])->Val())
                return (kTrue);
        return (kNil);
}

class SimpleLispInterpreter {
public:
        explicit SimpleLispInterpreter()
        {
                InitGlobalEnv();
        }

        void Process(const string &inputStr)
        {
                vector<const LispExprInterface *> exprs;
                auto tokens = GenerateTokens(inputStr);
                size_t cursor = 0;

                Parse(tokens, exprs, cursor);

                for (auto in: exprs) {
                        auto out = Evaluate(in, global_env_);
                        std::cout << out->ToString() << std::endl;
                }
        }

private:
        Env global_env_;

        void DefPrim(const string& name, const PrimFun *fn, ...)
        {
                auto e = new PrimExpr(fn, "<primitive:" + name + '>');
                global_env_.define(SymbolExpr::GetInstance(name), e);
        }

        void InitGlobalEnv(void)
        {
                DefPrim("+", PrimPlus);
                DefPrim("-", PrimMinus);
                DefPrim("*", PrimMul);
                DefPrim("/", PrimDiv);
                DefPrim("CONS", PrimCons);
                DefPrim("CAR", PrimCar);
                DefPrim("CDR", PrimCdr);
                DefPrim("NUMBER?", PrimIsNum);
                DefPrim("SYMBOL?", PrimIsSym);
                DefPrim("LIST?", PrimIsList);
                DefPrim("NIL?", PrimIsNil);
                DefPrim("AND?", PrimIsAnd);
                DefPrim("OR?", PrimIsOr);
                DefPrim("EQ?", PrimIsEq);
                DefPrim("=", PrimIsEqNum);
                DefPrim("<", PrimIsLtNum);
                DefPrim(">", PrimIsGtNum);
        }

        bool IsSeparator(char c)
        {
                return (isspace(c) || c == '(' || c == ')' || c == '"' ||
                        c == '\'');
        }

        bool IsNumber(const string &s, size_t startPos, size_t endPos)
        {
                size_t i;
                bool hasDot;

                i = startPos;
                if (!isdigit(s[i]) &&
                    !(s[i] == '-' && i+1 < endPos && isdigit(s[i+1])))
                        return (false);

                hasDot = false;
                while (++i < endPos)
                        if (!isdigit(s[i])) {
                                if (s[i] == '.' && !hasDot)
                                        hasDot = true;
                                else
                                        return (false);
                        }

                return (true);
        }

        vector<LispToken> GenerateTokens(const string &s)
        {
                vector<LispToken> tokenList;
                size_t i;

                i = 0;
                while (i < s.length()) {
                        if (isspace(s[i])) {
                                i++;
                                continue;                                
                        }

                        size_t startPos = i;
                        switch (s[i]) {
                        case '"':
                                startPos++;
                                while (++i < s.length() && s[i] != '"')
                                        ;
                                if (i == s.length())
                                        throw LispException(
                                                "Unmatched string quote.");
                                tokenList.push_back(
                                        {
                                                TokenKind::String,
                                                s.substr(startPos, i - startPos)
                                        });
                                i++;
                                break;
                        case '(':
                                // Check for "()" as a special case.
                                if (++i < s.length() && s[i] == ')') {
                                        tokenList.push_back({TokenKind::Nil});
                                        i++;
                                } else
                                        tokenList.push_back(
                                                {TokenKind::BeginParenthesis,
                                                 "("});
                                break;
                        case ')':
                                tokenList.push_back({
                                                TokenKind::EndParenthesis,
                                                ")"});
                                i++;
                                break;
                        case '\'':
                                tokenList.push_back({
                                                TokenKind::Quote, "'"});
                                i++;
                                break;
                        default:
                                while (i < s.length() && !IsSeparator(s[i]))
                                        i++;

                                string value = s.substr(startPos, i - startPos);
                                if (IsNumber(s, startPos, i))
                                        tokenList.push_back(
                                                {TokenKind::Num, value});
                                else if (value == "t" || value == "T")
                                        tokenList.push_back(
                                                {TokenKind::True, value});
                                else
                                        tokenList.push_back(
                                                {TokenKind::Symbol, value});
                                break;
                        }
                }

                return (tokenList);
        }

        const PairExpr *
        ParsePair(const vector<LispToken>& tokens, size_t& cursor)
        {
                auto e = ParseExpr(tokens, cursor);

                if (cursor == tokens.size())
                        throw LispException("Missing closing ')'.");

                if (tokens[cursor].kind == TokenKind::EndParenthesis) {
                        cursor++;
                        return (new PairExpr(e, kNil));
                }

                return (new PairExpr(e, ParsePair(tokens, cursor)));
        }

        const PairExpr *
        ParseQuote(const vector<LispToken>& tokens, size_t& cursor)
        {
                auto e = ParseExpr(tokens, cursor);

                return (new PairExpr(
                                SymbolExpr::GetInstance("QUOTE"),
                                new PairExpr(e, kNil)));
        }

        const LispExprInterface *
        ParseExpr(const vector<LispToken>& tokens, size_t& cursor)
        {
                if (cursor == tokens.size())
                        throw LispException("Unexpected end of the input.");
                auto& t = tokens[cursor++];
                switch (t.kind) {
                case TokenKind::Num:
                        return (new NumExpr(std::stof(t.value)));
                        break;
                case TokenKind::String:
                        return (new StrExpr(t.value));
                        break;
                case TokenKind::Nil:
                        return (kNil);
                        break;
                case TokenKind::Symbol:
                        return (SymbolExpr::GetInstance(t.value));
                        break;
                case TokenKind::BeginParenthesis:
                        return (ParsePair(tokens, cursor));
                        break;
                case TokenKind::True:
                        return (kTrue);
                        break;
                case TokenKind::Quote:
                        return (ParseQuote(tokens, cursor));
                        break;
                default:
                        throw LispException(
                                "Unexpected kind: " + t.value);
                        break;
                }
        }

        void Parse(const vector<LispToken>& tokens,
                   vector<const LispExprInterface *>& exprs,
                   size_t& cursor)
        {
                while (cursor < tokens.size()) {
                        auto e = ParseExpr(tokens, cursor);
                        exprs.push_back(e);
                }
        }

        const bool StartsWith(const PairExpr *p, const string& s)
        {
                auto first = p->First();
                if (first->Type() != ExprKind::Symbol)
                        return (false);

                return (static_cast<const SymbolExpr *>(first)->IsEqual(s));
        }

        const bool IsList(const PairExpr *p, size_t& len)
        {
                len = 1;
                while (p->Second()->Type() == ExprKind::Pair) {
                        len++;
                        p = static_cast<const PairExpr *>(p->Second());
                }

                return (p->Second()->Type() == ExprKind::Nil);
        }

        const void AssertList(const PairExpr *p, size_t n)
        {
                size_t m;

                if (IsList(p, m) && m == n)
                        return;

                throw LispException(p->ToString());
        }

        const LispExprInterface *Nth(const PairExpr *p, size_t n)
        {
                while (n--)
                        p = static_cast<const PairExpr *>(p->Second());

                return (p->First());
        }

        const vector<const SymbolExpr *>
        FunParams(const LispExprInterface *args)
        {
                vector<const SymbolExpr *> params;

                while (args->Type() != ExprKind::Nil) {
                        auto p = PairCast(args);
                        params.push_back(SymbolCast(p->First()));
                        args = p->Second();
                }

                return (params);
        }

        const LispExprInterface *
        Apply(const LispExprInterface *e,
              vector<const LispExprInterface *>& args,
              Env& env)
        {

                if (e->Type() == ExprKind::Primitive) {
                        auto fn = (static_cast<const PrimExpr *>(e))->Impl();
                        return (fn(args));
                }

                if (e->Type() != ExprKind::Function)
                        throw LispException(
                                "Not a procedure: " + e->ToString());

                Env newenv(&env);
                auto fn = static_cast<const FunExpr *>(e);
                auto params = fn->params();
                auto name = fn->name();
                if (args.size() != params.size()) {
                        std::ostringstream ss;
                        if (name == NULL)
                                ss << "<procedure>";
                        else
                                ss << name->ToString();
                        ss << ": given " << args.size() <<
                                " arguments instead of " <<
                                params.size() << '.';
                        throw LispException(ss.str());
                }

                for (size_t i = 0; i < args.size(); i++)
                        newenv.define(params[i], args[i]);

                return (Evaluate(fn->body(), newenv));
        }

        const LispExprInterface *EvalList(const PairExpr *p, Env& env)
        {
                if (StartsWith(p, "IF")) {
                        AssertList(p, 4);
                        auto e = Evaluate(Nth(p, 1), env);
                        if (e->Type() == ExprKind::Nil)
                                e = Nth(p, 3);
                        else
                                e = Nth(p, 2);
                        return (Evaluate(e, env));
                }

                if (StartsWith(p, "COND")) {
                        size_t n;
                        if (!IsList(p, n) || n == 1 || n % 2 != 1)
                                throw LispException(p->ToString());

                        for (;;) {
                                p = static_cast<const PairExpr *>(p->Second());
                                if (p->Type() == ExprKind::Nil)
                                        break;
                                auto e = Evaluate(p->First(), env);
                                if (e->Type() != ExprKind::Nil)
                                        return Evaluate(Nth(p, 1), env);
                                p = static_cast<const PairExpr *>(p->Second());
                        }
                        return (kNil);
                }

                if (StartsWith(p, "SET")) {
                        AssertList(p, 3);
                        auto name = SymbolCast(Nth(p, 1));
                        auto e = Evaluate(Nth(p, 2), env);
                        global_env_.define(name, e);
                        return (e);
                }

                if (StartsWith(p, "QUOTE")) {
                        AssertList(p, 2);
                        // Don't evaluate the argument.
                        return (Nth(p, 1));
                }

                if (StartsWith(p, "DEFINE")) {
                        AssertList(p, 4);
                        auto name = SymbolCast(Nth(p, 1));
                        auto params = FunParams(Nth(p, 2));
                        auto body = Nth(p, 3);

                        global_env_.define(
                                name,
                                new FunExpr(params, body, name));

                        return (kNil);
                }

                if (StartsWith(p, "LAMBDA")) {
                        AssertList(p, 3);
                        auto params = FunParams(Nth(p, 1));
                        auto body = Nth(p, 2);

                        return (new FunExpr(params, body));
                }

                if (StartsWith(p, "APPLY")) {
                        AssertList(p, 3);
                        auto fn = Evaluate(Nth(p, 1), env);
                        auto l = Evaluate(Nth(p, 2), env);
                        vector<const LispExprInterface *> args;

                        while (l->Type() != ExprKind::Nil) {
                                auto p = PairCast(l);
                                args.push_back(p->First());
                                l = p->Second();
                        }
                        return (Apply(fn, args, env));
                }

                if (StartsWith(p, "EVAL")) {
                        AssertList(p, 2);
                        auto e = Evaluate(Nth(p, 1), env);
                        return (Evaluate(e, env));
                }

                auto fn = Evaluate(p->First(), env);
                vector<const LispExprInterface *> args;
                for (auto e = p->Second();
                     e->Type() != ExprKind::Nil;
                     e = p->Second()) {
                        p = PairCast(e);
                        args.push_back(Evaluate(p->First(), env));
                }

                return (Apply(fn, args, env));
        }

        const LispExprInterface *EvalVar(const SymbolExpr *s, Env& env)
        {
                auto e = env.lookup(s);

                if (e)
                        return (e);
                throw LispException("Unbound variable: " + s->ToString());
        }

        const LispExprInterface *
        Evaluate(const LispExprInterface *e, Env& env)
        {
                switch (e->Type()) {
                case ExprKind::Num:
                case ExprKind::String:
                case ExprKind::Nil:
                case ExprKind::True:
                        return (e);
                        break;
                case ExprKind::Pair:
                        return (EvalList(static_cast<const PairExpr *>(e),
                                         env));
                        break;
                case ExprKind::Symbol:
                        return (EvalVar(static_cast<const SymbolExpr *>(e),
                                        env));
                        break;
                default:
                        throw LispException(
                                "Unexpected expression: " + e->ToString());
                        break;
                }

                return (NULL);
        }
};

void RunTests(void)
{
        std::ifstream is("lisp.test");
        SimpleLispInterpreter interp;        
        string line;
        string input;        

        while (std::getline(is, line)) {
                switch (line[0]) {
                case '<':
                        input = line.substr(1);                        
                        std::cerr << "Evaluating: " << input << std::endl;
                        std::cerr << "--> ";                        
                        try {
                                interp.Process(input);
                        } catch (const std::exception &e) {
                                std::cerr << "Error: " << e.what() << std::endl;
                        }
                        break;
                case '>':
                        std::cerr << "Expected output: " << line.substr(1) <<
                                std::endl;                        
                        break;
                default:
                        std::cerr << line << std::endl;
                        break;                        
                }
        }

        is.close();        
}

} // namespace

int
main(void)
{
        SimpleLispInterpreter lisp_interpreter;
        string user_input;

        std::cerr << "Enter an expression (or '!exit' to quit and " <<
                "'!test' to run tests):" << std::endl;
        for (;;) {
                std::cerr << "> ";
                getline(std::cin, user_input);
                if (user_input == "!exit")
                        break;
                if (user_input == "!test") {
                        RunTests();
                        continue;                        
                }
                try {
                        lisp_interpreter.Process(user_input);
                } catch (const std::exception &e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                }
        }

        return (0);
}
