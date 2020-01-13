#pragma once

#include "error/error.h"
#include "instruction/instruction.h"
#include "tokenizer/token.h"

#include <vector>
#include <optional>
#include <utility>
#include <map>
#include <cstdint>
#include <cstddef> // for std::size_t

namespace cc0 {

    // 变量格式
    struct varInfo{
        std::string varName;
        bool isConst;
//        long long offsetIndex; // 直接在变量表中查看即可
//        std::string varType; // 暂时有int和char两种，最后还是放弃了，太麻烦了
    };

    // 函数格式
    struct funcInfo{
        std::string funcName;
        std::string funcType;
        int paramNum;
        int constOffset;
        bool isReturn;
        // int hierarchy; 此时层次始终默认为一，因此去除属性，不扩展作用域了，太难
        std::vector<cc0::Instruction> localCode;
    };

    // 常量格式
    struct constInfo{
        char type;
        std::string value;
    };

    // jump记录格式
    struct jumpInfo{
        int pos;
        std::string type;
    };

    // 输出格式
    struct resultInfo{
        std::vector<cc0::Instruction> globalCode;
        std::vector<cc0::constInfo> constList;
        std::vector<cc0::funcInfo> funcList;
    };

	class Analyser final {
	private:
		using uint64_t = std::uint64_t;
		using int64_t = std::int64_t;
		using uint32_t = std::uint32_t;
		using int32_t = std::int32_t;
	public:
		Analyser(std::vector<Token> v)
			: _tokens(std::move(v)), _offset(0), _current_pos(0, 0), _nextTokenIndex(0) {}
		Analyser(Analyser&&) = delete;
		Analyser(const Analyser&) = delete;
		Analyser& operator=(Analyser) = delete;

		// 唯一接口
		std::pair<cc0::resultInfo, std::optional<CompilationError>> Analyse();

	private:
		// 所有的递归子程序
//		// <常表达式>
//		// 这里的 out 是常表达式的值
//		std::optional<CompilationError> analyseConstantExpression(int32_t& out);

        // 对可能有特殊含义的值增加了normal前缀
        std::optional<CompilationError> c0Program();
        std::optional<CompilationError> assignmentExpression();
        std::optional<CompilationError> normalExpression();
        std::optional<CompilationError> additiveExpression();
        std::optional<CompilationError> multiplicativeExpression();
        std::optional<CompilationError> castExpression();
        std::optional<CompilationError> unaryExpression();
        std::optional<CompilationError> primaryExpression();
        std::optional<CompilationError> variableDeclaration();
        std::optional<CompilationError> initDeclaratorList(bool isConst);
        std::optional<CompilationError> initDeclarator(bool isConst);
        std::optional<CompilationError> functionDefinition();
        std::optional<CompilationError> parameterClause(int& paramNum);
        std::optional<CompilationError> parameterDeclarationList(int& paramNum);
        std::optional<CompilationError> parameterDeclaration();
        std::optional<CompilationError> functionCall();
        std::optional<CompilationError> expresionList(int& paramNum);
        std::optional<CompilationError> compoundStatement();
        std::optional<CompilationError> statementSequence();
        std::optional<CompilationError> normalStatement();
        std::optional<CompilationError> jumpStatement();
        std::optional<CompilationError> returnStatement();
        std::optional<CompilationError> conditionStatement();
        std::optional<CompilationError> ifCondition();
        std::optional<CompilationError> switchCondition();
        std::optional<CompilationError> labeledStatement();
        std::optional<CompilationError> normalCondition();
        std::optional<CompilationError> loopStatement();
        std::optional<CompilationError> whileLoop();
        std::optional<CompilationError> doWhileLoop();
        std::optional<CompilationError> forLoop();
        std::optional<CompilationError> scanStatement();
        std::optional<CompilationError> printStatement();
        std::optional<CompilationError> printableList();
        std::optional<CompilationError> printable();

	private:
		std::vector<Token> _tokens;
		std::size_t _offset;
		std::pair<uint64_t, uint64_t> _current_pos;


        // 下一个 token 在栈的偏移
        int32_t _nextTokenIndex;

        // Token 缓冲区相关操作
        // 返回下一个 token
        std::optional<Token> nextToken();
        // 回退一个 token
        void unreadToken();

		// 为了简单处理，直接把符号表耦合在语法分析里

		// 脚标直接基于vector的存储顺序
		// 默认值，这里是指变量的默认初始值和int函数的默认返回值
		cc0::resultInfo result ;
        // 变量的占位符
		std::string defaultValue = "0";
		// 全局变量表
		std::vector<cc0::varInfo> globalVarList;
		// 局部变量表
        std::vector<cc0::varInfo> localVarList;

        bool globalFlag;
		// 启动代码
        std::vector<cc0::Instruction> globalCode;
        long long globalOffset;
        // 临时函数代码
        std::vector<cc0::Instruction> localCode;
        long long localOffset;
        // 临时存储switch的表达式代码
        std::vector<cc0::Instruction> switchCode;
        std::vector<std::string> caseValue;

        // 记录break和continue位置的临时变量
        std::vector<cc0::jumpInfo> jumpPos;
        // 记录是否在循环和switch中
        bool inLoop;
        bool inSwitch;

		// 维护函数表及函数体的指令
		std::vector<cc0::funcInfo> funcList;
        funcInfo funcNow,zeroFunc;

        // 维护常量表
        std::vector<cc0::constInfo> constList;

        // 下面是符号表相关操作
        // 判断变量重定义
        bool isDeclared(const std::string& name, const std::string& type);
        // 判断常量
        bool isConst(const std::string& name);
        // 判断函数重名并const 获取函数&脚标
        int getFunc(const std::string& name);
        void resetFunc(){funcNow = zeroFunc;}
        // 获取常量
        int getConst(const std::string& name);
        // 获取变量脚标
        std::pair<std::string, std::string> getVar(const std::string &name);
    };
}
