#include "fmt/core.h"
#include "tokenizer/tokenizer.h"
#include "analyser/analyser.h"
#include <iostream>
#include <cstring>

namespace fmt {
    template<>
    struct formatter<cc0::ErrorCode> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const cc0::ErrorCode &p, FormatContext &ctx) {
            std::string name;
            switch (p) {
                case cc0::ErrWrongComment:
                    name = "wrong comment.";
                    break;
                case cc0::ErrWrongChar:
                    name = "wrong char content.";
                    break;
                case cc0::ErrWrongString:
                    name = "wrong string content.";
                    break;
                case cc0::ErrWrongNum:
                    name = "wrong num format.";
                    break;
                case cc0::ErrAll:
                    // 报错内容已经在analyser中输出，这里直接报错退出即可
                    name = "Your code at the left position has the above error. \n";
                    break;
                case cc0::ErrNoError:
                    name = "No error.";
                    break;
                case cc0::ErrStreamError:
                    name = "Stream error.";
                    break;
                case cc0::ErrEOF:
                    name = "EOF";
                    break;
                case cc0::ErrInvalidInput:
                    name = "The input is invalid.";
                    break;
                case cc0::ErrInvalidIdentifier:
                    name = "Identifier is invalid";
                    break;
                case cc0::ErrIntegerOverflow:
                    name = "The integer is too big(int64_t).";
                    break;
                case cc0::ErrNoBegin:
                    name = "The program should start with 'begin'.";
                    break;
                case cc0::ErrNoEnd:
                    name = "The program should end with 'end'.";
                    break;
                case cc0::ErrNeedIdentifier:
                    name = "Need an identifier here.";
                    break;
                case cc0::ErrConstantNeedValue:
                    name = "The constant need a value to initialize.";
                    break;
                case cc0::ErrNoSemicolon:
                    name = "Zai? Wei shen me bu xie fen hao.";
                    break;
                case cc0::ErrInvalidVariableDeclaration:
                    name = "The declaration is invalid.";
                    break;
                case cc0::ErrIncompleteExpression:
                    name = "The expression is incomplete.";
                    break;
                case cc0::ErrNotDeclared:
                    name = "The variable or constant must be declared before being used.";
                    break;
                case cc0::ErrAssignToConstant:
                    name = "Trying to assign value to a constant.";
                    break;
                case cc0::ErrDuplicateDeclaration:
                    name = "The variable or constant has been declared.";
                    break;
                case cc0::ErrNotInitialized:
                    name = "The variable has not been initialized.";
                    break;
                case cc0::ErrInvalidAssignment:
                    name = "The assignment statement is invalid.";
                    break;
                case cc0::ErrInvalidPrint:
                    name = "The output statement is invalid.";
                    break;
                case cc0::ErrWrongSign:
                    name = "Wrong sign.";
                    break;
            }
            return format_to(ctx.out(), name);
        }
    };

    template<>
    struct formatter<cc0::CompilationError> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const cc0::CompilationError &p, FormatContext &ctx) {
            return format_to(ctx.out(), "Line: {} Column: {} Error: {}", p.GetPos().first, p.GetPos().second,
                             p.GetCode());
        }
    };
}

namespace fmt {
    template<>
    struct formatter<cc0::Token> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const cc0::Token &p, FormatContext &ctx) {
            return format_to(ctx.out(),
                             "Line: {} Column: {} Type: {} Value: {}",
                             p.GetStartPos().first, p.GetStartPos().second, p.GetType(), p.GetValueString());
        }
    };

    template<>
    struct formatter<cc0::TokenType> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const cc0::TokenType &p, FormatContext &ctx) {
            std::string name;
            switch (p) {
                case cc0::NULL_TOKEN:
                    name = "NULL_TOKEN";
                    break;
                case cc0::DECIMAL_INTEGER:
                    name = "DECIMAL_INTEGER";
                    break;
                case cc0::HEXADECIMAL_INTEGER:
                    name = "HEXADECIMAL_INTEGER";
                    break;
                case cc0::IDENTIFIER:
                    name = "IDENTIFIER";
                    break;
                case cc0::CONST:
                    name = "CONST";
                    break;
                case cc0::VOID:
                    name = "VOID";
                    break;
                case cc0::INT:
                    name = "INT";
                    break;
                case cc0::CHART:
                    name = "CHARtype";
                    break;
                case cc0::CHAR:
                    name = "CHAR";
                    break;
                case cc0::DOUBLE:
                    name = "DOUBLE";
                    break;
                case cc0::STRING:
                    name = "STRING";
                    break;
                case cc0::STRUCT:
                    name = "STRUCT";
                    break;
                case cc0::IF:
                    name = "IF";
                    break;
                case cc0::ELSE:
                    name = "ELSE";
                    break;
                case cc0::SWITCH:
                    name = "SWITCH";
                    break;
                case cc0::CASE:
                    name = "CASE";
                    break;
                case cc0::DEFAULT:
                    name = "DEFAULT";
                    break;
                case cc0::WHILE:
                    name = "WHILE";
                    break;
                case cc0::FOR:
                    name = "FOR";
                    break;
                case cc0::DO:
                    name = "DO";
                    break;
                case cc0::RETURN:
                    name = "RETURN";
                    break;
                case cc0::BREAK:
                    name = "BREAK";
                    break;
                case cc0::CONTINUE:
                    name = "CONTINUE";
                    break;
                case cc0::PRINT:
                    name = "PRINT";
                    break;
                case cc0::SCAN:
                    name = "SCAN";
                    break;
                case cc0::PLUS_SIGN:
                    name = "PLUS_SIGN";
                    break;
                case cc0::MINUS_SIGN:
                    name = "MINUS_SIGN";
                    break;
                case cc0::MULTIPLICATION_SIGN:
                    name = "MULTIPLICATION_SIGN";
                    break;
                case cc0::DIVISION_SIGN:
                    name = "DIVISION_SIGN";
                    break;
                case cc0::EQUAL_SIGN:
                    name = "EQUAL_SIGN";
                    break;
                case cc0::LESS_SIGN:
                    name = "LESS_SIGN";
                    break;
                case cc0::LESS_EQUAL_SIGN:
                    name = "LESS_EQUAL_SIGN";
                    break;
                case cc0::MORE_SIGN:
                    name = "MORE_SIGN";
                    break;
                case cc0::MORE_EQUAL_SIGN:
                    name = "MORE_EQUAL_SIGN";
                    break;
                case cc0::NOT_EQUAL_SIGN:
                    name = "NOT_EQUAL_SIGN";
                    break;
                case cc0::VALUE_EQUAL_SIGN:
                    name = "VALUE_EQUAL_SIGN";
                    break;
                case cc0::SEMICOLON:
                    name = "SEMICOLON";
                    break;
                case cc0::COLON:
                    name = "COLON";
                    break;
                case cc0::COMMA:
                    name = "COMMA";
                    break;
                case cc0::SINGLE_QUOTATION:
                    name = "SINGLE_QUOTATION";
                    break;
                case cc0::DOUBLE_QUOTATION:
                    name = "DOUBLE_QUOTATION";
                    break;
                case cc0::LEFT_PARENTHESIS:
                    name = "LEFT_PARENTHESIS";
                    break;
                case cc0::RIGHT_PARENTHESIS:
                    name = "RIGHT_PARENTHESIS";
                    break;
                case cc0::LEFT_BRACE:
                    name = "LEFT_BRACE";
                    break;
                case cc0::RIGHT_BRACE:
                    name = "RIGHT_BRACE";
                    break;
            }
            return format_to(ctx.out(), name);
        }
    };
}

namespace fmt {
    template<>
    struct formatter<cc0::Operation> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const cc0::Operation &p, FormatContext &ctx) {
            std::string name;
            switch (p) {
                case cc0::NOP:
                    name = "nop";
                    break;
                case cc0::BIPUSH:
                    name = "bipush";
                    break;
                case cc0::IPUSH:
                    name = "ipush";
                    break;
                case cc0::POP:
                    name = "pop";
                    break;
                case cc0::POP2:
                    name = "pop2";
                    break;
                case cc0::POPN:
                    name = "popn";
                    break;
                case cc0::DUP:
                    name = "dup";
                    break;
                case cc0::DUP2:
                    name = "dup2";
                    break;
                case cc0::LOADC:
                    name = "loadc";
                    break;
                case cc0::LOADA:
                    name = "loada";
                    break;
                case cc0::ILOAD:
                    name = "iload";
                    break;
                case cc0::ALOAD:
                    name = "aload";
                    break;
                case cc0::ISTORE:
                    name = "istore";
                    break;
                case cc0::ASTORE:
                    name = "astore";
                    break;
                case cc0::IADD:
                    name = "iadd";
                    break;
                case cc0::ISUB:
                    name = "isub";
                    break;
                case cc0::IMUL:
                    name = "imul";
                    break;
                case cc0::IDIV:
                    name = "idiv";
                    break;
                case cc0::INEG:
                    name = "ineg";
                    break;
                case cc0::ICMP:
                    name = "icmp";
                    break;
                case cc0::JMP:
                    name = "jmp";
                    break;
                case cc0::JE:
                    name = "je";
                    break;
                case cc0::JNE:
                    name = "jne";
                    break;
                case cc0::JL:
                    name = "jl";
                    break;
                case cc0::JGE:
                    name = "jge";
                    break;
                case cc0::JG:
                    name = "jg";
                    break;
                case cc0::JLE:
                    name = "jle";
                    break;
                case cc0::CALL:
                    name = "call";
                    break;
                case cc0::RET:
                    name = "ret";
                    break;
                case cc0::IRET:
                    name = "iret";
                    break;
                case cc0::IPRINT:
                    name = "iprint";
                    break;
                case cc0::CPRINT:
                    name = "cprint";
                    break;
                case cc0::SPRINT:
                    name = "sprint";
                    break;
                case cc0::PRINTL:
                    name = "printl";
                    break;
                case cc0::ISCAN:
                    name = "iscan";
                    break;
                case cc0::I2C:
                    name = "i2c";
                    break;
                case cc0::CSCAN:
                    name = "cscan";
                    break;
            }
            return format_to(ctx.out(), name);
        }
    };

    template<>
    struct formatter<cc0::Instruction> {
        template<typename ParseContext>
        constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

        template<typename FormatContext>
        auto format(const cc0::Instruction &p, FormatContext &ctx) {
            std::string name;
            switch (p.GetOperation()) {
                // 无操作数
                case cc0::NOP:
                case cc0::POP:
                case cc0::POP2:
                case cc0::DUP:
                case cc0::DUP2:
                case cc0::ILOAD:
                case cc0::ALOAD:
                case cc0::ISTORE:
                case cc0::ASTORE:
                case cc0::IADD:
                case cc0::ISUB:
                case cc0::IMUL:
                case cc0::IDIV:
                case cc0::INEG:
                case cc0::ICMP:
                case cc0::I2C:
                case cc0::RET:
                case cc0::IRET:
                case cc0::IPRINT:
                case cc0::CPRINT:
                case cc0::SPRINT:
                case cc0::PRINTL:
                case cc0::ISCAN:
                case cc0::CSCAN:
                    return format_to(ctx.out(), "{}", p.GetOperation());
                // 单操作数
                case cc0::BIPUSH:
                case cc0::IPUSH:
                case cc0::POPN:
                case cc0::LOADC:
                case cc0::JMP:
                case cc0::JE:
                case cc0::JNE:
                case cc0::JL:
                case cc0::JGE:
                case cc0::JG:
                case cc0::JLE:
                case cc0::CALL:
                    return format_to(ctx.out(), "{} {}", p.GetOperation(), p.GetParam1());
                // 双操作数
                case cc0::LOADA:
                    return format_to(ctx.out(), "{} {}, {}", p.GetOperation(), p.GetParam1(), p.GetParam2());
            }
            std::cout << "wrong instruction\n";
            return format_to(ctx.out(), "NOP");
        }
    };
}
