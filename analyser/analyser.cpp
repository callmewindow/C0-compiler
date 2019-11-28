#include "analyser.h"

#include <climits>
#include <sstream>
#include <map>

namespace miniplc0 {
    long long getNum(std::string num)
    {
        std::stringstream stoi;
        long long num_value;
        stoi << num;
        stoi >> num_value;
        return num_value;
    }

	std::pair<std::vector<Instruction>, std::optional<CompilationError>> Analyser::Analyse() {
		auto err = analyseProgram();
		if (err.has_value())
			return std::make_pair(std::vector<Instruction>(), err);
		else
			return std::make_pair(_instructions, std::optional<CompilationError>());
	}

	// <程序> ::= 'begin'<主过程>'end'
	std::optional<CompilationError> Analyser::analyseProgram() {
		// 示例函数，示例如何调用子程序

		// 'begin'
		auto bg = nextToken();
		if (!bg.has_value() || bg.value().GetType() != TokenType::BEGIN)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoBegin);

		// <主过程>
		auto err = analyseMain();
		if (err.has_value())
			return err;

		// 'end'
		auto ed = nextToken();
		if (!ed.has_value() || ed.value().GetType() != TokenType::END)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoEnd);
		return {};
	}

	// <主过程> ::= <常量声明><变量声明><语句序列>
	// 需要补全
	std::optional<CompilationError> Analyser::analyseMain() {
		// 完全可以参照 <程序> 编写

		// <常量声明>
        auto errCD = analyseConstantDeclaration();
        if (errCD.has_value())
            return errCD;

        // <变量声明>
        auto errVD = analyseVariableDeclaration();
        if (errVD.has_value())
            return errVD;

		// <语句序列>
		auto errSS = analyseStatementSequence();
		if (errSS.has_value())
            return errSS;

		return {};
	}

	// <常量声明> ::= {<常量声明语句>}
	// <常量声明语句> ::= 'const'<标识符>'='<常表达式>';'
	std::optional<CompilationError> Analyser::analyseConstantDeclaration() {
		// 示例函数，示例如何分析常量声明

		// 常量声明语句可能有 0 或无数个
		while (true) {
			// 预读一个 token，不然不知道是否应该用 <常量声明语句> 推导
			// 也是除了错误之外循环终止的唯一条件
			auto next = nextToken();
			if (!next.has_value())
				return {};
			// 如果是 const 那么说明应该推导 <常量声明语句> 否则直接返回
			if (next.value().GetType() != TokenType::CONST) {
				unreadToken();
				return {};
			}

			// <常量声明语句>
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
			// 判断重定义，其实是语义分析的内容，这里结合进行
			if (isDeclared(next.value().GetValueString()))
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);
			// 添加常量，定义语句需要add
			addConstant(next.value());

			// '='
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::EQUAL_SIGN)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrConstantNeedValue);

			// <常表达式>
			int32_t val;
			// 综合属性传值
			auto err = analyseConstantExpression(val);
			if (err.has_value())
				return err;

			// ';'
			next = nextToken();
			if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
				return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);

			// 生成一次 LIT 指令加载常量
			// 此时val已经得到了值，因此插入int
			// 指令的生成需要在声明，计算，赋值时进行，这里需要注意，且应该是分析结束后进行
			_instructions.emplace_back(Operation::LIT, val);
		}
		return {};
	}

	// <变量声明> ::= {<变量声明语句>}
	// <变量声明语句> ::= 'var'<标识符>['='<表达式>]';'
	// 需要补全
	std::optional<CompilationError> Analyser::analyseVariableDeclaration() {
		// 变量声明语句可能有一个或者多个
        while (true)
        {
            // 预读？
            auto next = nextToken();
            if (!next.has_value())
                return {};
            // 'var'
            if (next.value().GetType() != TokenType::VAR) {
                unreadToken();
                return {};
            }

            // <变量声明语句>
            // <标识符>
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
            // 是否定义，这里会检查三种，常量，未初始化变量和初始化变量，因此不必担心后续
            if (isDeclared(next.value().GetValueString()))
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrDuplicateDeclaration);

            // temp存储当前标识符，基于是否初始化进行不同的add
            auto temp = next;

            // 变量可能没有初始化，仍然需要一次预读
            // 但无论如何都需要分配空间，因此先放入一个临时值，防止出现奇怪得错误
            _instructions.emplace_back(Operation::LIT,0);

            // '='
            // 注意是[]，所以可有有无
            // 而且当判断不是等号后，不一定变量声明就结束，而应该判断分号，进而选择报错或继续判断声明语句
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TokenType::EQUAL_SIGN)
            {
                // 判断分号
                if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
                // 是分号则添加一个未初始化的变量然后继续，注意不用回退，分号已经判断完毕
                addUninitializedVariable(temp.value());
                continue;
            }

            // '<表达式>'
            // 注意，这里表达式的分析实现是不提前计算的，其实提前计算也可以，就需要和常量表达式相同传入地址
            auto err = analyseExpression();
            if (err.has_value())
                return err;

            // ';'
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);

            // 说明是初始化的变量，直接初始化
            addVariable(temp.value());

            // 虚拟机指令
            // 这里因为计算好的表达式的值已经在栈顶了，因此首先添加变量，然后存储表达式在栈顶的值即可，相当于更新刚才LIT 0 的值
            _instructions.emplace_back(Operation::STO, getIndex(temp.value().GetValueString()));
        }
		return {};
	}

	// <语句序列> ::= {<语句>}
	// <语句> :: = <赋值语句> | <输出语句> | <空语句>
	// <赋值语句> :: = <标识符>'='<表达式>';'
	// <输出语句> :: = 'print' '(' <表达式> ')' ';'
	// <空语句> :: = ';'
	// 需要补全
	std::optional<CompilationError> Analyser::analyseStatementSequence() {
		while (true) {
			// 预读
			auto next = nextToken();
			if (!next.has_value())
				return {};
			// 防止返回后预读的token消失所以需要先回退
			// 且在调用后面的子程序时也都会预读，因此直接回退即可
			unreadToken();
			if (next.value().GetType() != TokenType::IDENTIFIER &&
				next.value().GetType() != TokenType::PRINT &&
				next.value().GetType() != TokenType::SEMICOLON) {
				return {};
			}
			// 此时也有可能是其他的符号，例如等号，但是将这个错误交给end的判断进行处理即可，就是报错有点奇怪。。
			std::optional<CompilationError> err;
			switch (next.value().GetType()) {
				// 这里需要你针对不同的预读结果来调用不同的子程序
				// 注意我们没有针对空语句单独声明一个函数，因此可以直接在这里返回
				// 注意判断结束后要break出switch，判断其他的<语句>
                case TokenType::IDENTIFIER :
                    err = analyseAssignmentStatement();
                    if(err.has_value())
                        return err;
                    break;

                case TokenType::PRINT:
                    err = analyseOutputStatement();
                    if(err.has_value())
                        return err;
                    break;

                case TokenType::SEMICOLON :
                    // 这里因为没有对应的子程序，因此回退需要取消，即将这个分号读进来，否则下一个判断会在分号无限循环
                    next = nextToken();
                    break;

			default:
				break;
			}
		}
		return {};
	}

	// <常表达式> ::= [<符号>]<无符号整数>
	// 需要补全
	std::optional<CompilationError> Analyser::analyseConstantExpression(int32_t& out) {
		// out 是常表达式的结果
		// 这里你要分析常表达式并且计算结果
		// 注意以下均为常表达式
		// +1 -1 1
		// 同时要注意是否溢出
		// 除了这里，表达式中的计算可能会导致溢出（在+或*时，因此需要注意）

        // [<符号>]
        // 助教编写
        auto next = nextToken();
        auto prefix = 1;
        if (!next.has_value())
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
        if (next.value().GetType() == TokenType::PLUS_SIGN)
            prefix = 1;
        else if (next.value().GetType() == TokenType::MINUS_SIGN) {
            prefix = -1;
            //用于产生负数，0-val
            _instructions.emplace_back(Operation::LIT, 0);
        }
        else
            unreadToken();

        // <无符号整数>
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TokenType::UNSIGNED_INTEGER)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
        // 综合属性赋值
        long long tempNum = getNum(next.value().GetValueString());
        if(tempNum > INT_MAX)
        {
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIntegerOverflow);
        }
        out = tempNum;
        // 这里是不必的，因为完成后在常量赋值处会有一个加载指令的，而且顺序需要和SUB对应，也不能添加
//        _instructions.emplace_back(Operation::LIT, out);

        // 取负
        if (prefix == -1)
            _instructions.emplace_back(Operation::SUB, 0);

        return {};
	}

	// 注意在表达式中都是被使用的，因此涉及变量的指令应为LOD和操作
	// <表达式> ::= <项>{<加法型运算符><项>}
	std::optional<CompilationError> Analyser::analyseExpression() {
		// <项>
		auto err = analyseItem();
		if (err.has_value())
			return err;

		// {<加法型运算符><项>}
		while (true) {
			// 预读
			auto next = nextToken();
			if (!next.has_value())
				return {};

			// 终止条件，不再以+-开头
			auto type = next.value().GetType();
			if (type != TokenType::PLUS_SIGN && type != TokenType::MINUS_SIGN) {
				unreadToken();
				return {};
			}

			// <项>
			// 在这里会向栈中加入数字，然后便可依靠后面加入的指令进行
			err = analyseItem();
			if (err.has_value())
				return err;

			// 根据结果生成指令，此时已经确认为两者中的一种，前面的if
			if (type == TokenType::PLUS_SIGN)
				_instructions.emplace_back(Operation::ADD, 0);
			else if (type == TokenType::MINUS_SIGN)
				_instructions.emplace_back(Operation::SUB, 0);
		}
		return {};
	}

	// <赋值语句> ::= <标识符>'='<表达式>';'
	// 需要补全
	std::optional<CompilationError> Analyser::analyseAssignmentStatement() {
		// 这里除了语法分析以外还要留意
		// 标识符声明过吗？
		// 标识符是常量吗？
		// 需要生成指令吗？
		// 以上三条其实就是在语法分析过程中进行语义分析的过程
		// <标识符>
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNeedIdentifier);
        // 还没有定义，错误赋值
        if (!isDeclared(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
        // 常量不能改变
        if (isConstant(next.value().GetValueString()))
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAssignToConstant);
        // 记录标识符名字
        std::string identifierName = next.value().GetValueString();

        // '='
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TokenType::EQUAL_SIGN)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrConstantNeedValue);

        // <表达式>
        auto err = analyseExpression();
        if (err.has_value())
            return err;

		// ';'
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
            return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);

		// 指令
		// 此时表达式的值在栈顶，因此需要存储到对应标识符的位置
        // 同时注意如果是对未初始化的变量，需要将其调整到初始化的列表中
		if(isUninitializedVariable(identifierName))
        {
		    for(auto it = _uninitialized_vars.begin();it!=_uninitialized_vars.end();it++)
            {
		        if(it->first == identifierName)
                {
		            // 位置的索引直接替换过来，而且定义时会检测重名，因此不必担心会替换什么
		            _vars[identifierName] = it->second;
		            _uninitialized_vars.erase(it);
		            break;
                }
            }
        }
        _instructions.emplace_back(Operation::STO,getIndex(identifierName));
		return {};
	}

	// <输出语句> ::= 'print' '(' <表达式> ')' ';'
	std::optional<CompilationError> Analyser::analyseOutputStatement() {
		// 如果之前 <语句序列> 的实现正确，这里第一个 next 一定是 TokenType::PRINT
		// 也就是在前面正确回退，且不用进行预读，在这里进行读取
		auto next = nextToken();

		// '('
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);

		// <表达式>
		auto err = analyseExpression();
		if (err.has_value())
			return err;

		// ')'
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrInvalidPrint);

		// ';'
		next = nextToken();
		if (!next.has_value() || next.value().GetType() != TokenType::SEMICOLON)
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);

		// 生成相应的指令 WRT
		_instructions.emplace_back(Operation::WRT, 0);
		return {};
	}

	// <项> :: = <因子>{ <乘法型运算符><因子> }
	// 需要补全
	std::optional<CompilationError> Analyser::analyseItem() {
		// 可以参考 <表达式> 实现
        // <因子>
        auto err = analyseFactor();
        if (err.has_value())
            return err;

        // {<乘法型运算符><因子>}
        while (true) {
            // 预读
            auto next = nextToken();
            if (!next.has_value())
                return {};
            // 终止条件
            auto type = next.value().GetType();
            if (type != TokenType::MULTIPLICATION_SIGN && type != TokenType::DIVISION_SIGN) {
                unreadToken();
                return {};
            }

            // <因子>
            err = analyseFactor();
            if (err.has_value())
                return err;

            // 根据结果生成指令
            if (type == TokenType::MULTIPLICATION_SIGN)
                _instructions.emplace_back(Operation::MUL, 0);
            else if (type == TokenType::DIVISION_SIGN)
                _instructions.emplace_back(Operation::DIV, 0);
        }
        return {};
	}

	// <因子> ::= [<符号>]( <标识符> | <无符号整数> | '('<表达式>')' )
	// 需要补全
	std::optional<CompilationError> Analyser::analyseFactor() {
		// [<符号>]
		auto next = nextToken();
		auto prefix = 1;
		if (!next.has_value())
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		if (next.value().GetType() == TokenType::PLUS_SIGN)
			prefix = 1;
		else if (next.value().GetType() == TokenType::MINUS_SIGN) {
			prefix = -1;
			_instructions.emplace_back(Operation::LIT, 0);
		}
		else
			unreadToken();

		// 预读
		next = nextToken();
		if (!next.has_value())
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);

		std::optional<CompilationError> err;
		long long tempNum;
		switch (next.value().GetType()) {
			// 这里和 <语句序列> 类似，需要根据预读结果调用不同的子程序
			// 但是要注意 default 返回的是一个编译错误
			// 且需要注意的是，这里因为有不必回退的，因此回退需要在部分case中进行

            case TokenType::IDENTIFIER :
                // 这里就不必再判断了，因此此处只需有标识符即可，但是需要判断是否已经初始化
//                err = analyseAssignmentStatement();
//                if(err.has_value())
//                    return err;
                // 如果不是 已经初始化的变量或常量 就报错，否则加载它的值（包括未定义和未初始化两种），报错情况最好也分开
                if(!isInitializedVariable(next.value().GetValueString())&&!isConstant(next.value().GetValueString()))
                {
                    // 未初始化
                    if(isUninitializedVariable(next.value().GetValueString()))
                    {
                        return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotInitialized);
                    }
                    // 未定义
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNotDeclared);
                }
                // 正常已定义，找值加载
                _instructions.emplace_back(Operation::LOD, getIndex(next.value().GetValueString()));
                break;

            case TokenType::UNSIGNED_INTEGER :
                // 这里需要取值然后进行虚拟机指令
                tempNum = getNum(next.value().GetValueString());
                if(tempNum > INT_MAX)
                {
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIntegerOverflow);
                }
                _instructions.emplace_back(Operation::LIT, (int32_t)tempNum);
                break;

            case TokenType::LEFT_BRACKET:
                // 此时左括号已经判断，所以直接判断<表达式即可>
                err = analyseExpression();
                if (err.has_value())
                    return err;

                // ')'
                next = nextToken();
                if (!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET)
                    // 注意因子属于项，项属于表达式，所以如果出错直接不完整的表达式即可，其实是助教懒得弄左右括号的错误了
                    return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
                break;

		default:
			return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrIncompleteExpression);
		}

		// 取负，0-num形成-num
		if (prefix == -1)
			_instructions.emplace_back(Operation::SUB, 0);
		return {};
	}

	std::optional<Token> Analyser::nextToken() {
		if (_offset == _tokens.size())
			return {};
		// 考虑到 _tokens[0..._offset-1] 已经被分析过了
		// 所以我们选择 _tokens[0..._offset-1] 的 EndPos 作为当前位置
		_current_pos = _tokens[_offset].GetEndPos();
		return _tokens[_offset++];
	}

	void Analyser::unreadToken() {
		if (_offset == 0)
			DieAndPrint("analyser unreads token from the begining.");
		// 虚假的unreadToken
		_current_pos = _tokens[_offset - 1].GetEndPos();
		_offset--;
	}

	void Analyser::_add(const Token& tk, std::map<std::string, int32_t>& mp) {
		if (tk.GetType() != TokenType::IDENTIFIER)
			DieAndPrint("only identifier can be added to the table.");
		// 在保存变量的同时记录了变量在符号表中的位置
		mp[tk.GetValueString()] = _nextTokenIndex;
		_nextTokenIndex++;
	}

	void Analyser::addVariable(const Token& tk) {
		_add(tk, _vars);
	}

	void Analyser::addConstant(const Token& tk) {
		_add(tk, _consts);
	}

	void Analyser::addUninitializedVariable(const Token& tk) {
		_add(tk, _uninitialized_vars);
	}

	// 从此处可以获得变量的存储位置
	int32_t Analyser::getIndex(const std::string& s) {
		if (_uninitialized_vars.find(s) != _uninitialized_vars.end())
			return _uninitialized_vars[s];
		else if (_vars.find(s) != _vars.end())
			return _vars[s];
		else
			return _consts[s];
	}

	bool Analyser::isDeclared(const std::string& s) {
		return isConstant(s) || isUninitializedVariable(s) || isInitializedVariable(s);
	}

	bool Analyser::isUninitializedVariable(const std::string& s) {
		return _uninitialized_vars.find(s) != _uninitialized_vars.end();
	}
	bool Analyser::isInitializedVariable(const std::string&s) {
		return _vars.find(s) != _vars.end();
	}

	bool Analyser::isConstant(const std::string&s) {
		return _consts.find(s) != _consts.end();
	}
}