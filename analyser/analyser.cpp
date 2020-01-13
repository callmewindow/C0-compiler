#include "analyser.h"

#include <climits>
#include <sstream>
#include <cstring>
#include <map>

using namespace std;
namespace cc0 {
#define TT TokenType
#define opError std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAll)
    // 错误输出
    void errOut(std::pair<uint64_t, uint64_t> p,const std::string& errCon){
        cout<<"Syntactic analysis error: Line: "<<p.first<<" Column: "<<p.second<<" Error: "<<errCon<<'\n';
    }

    // 获取char的int内容用于输出
    int getCharNum(std::string str){
        // 去除两侧单引号
        str = str.substr(1,str.length()-2);
        int charNum = 0;
        // 这里是针对不同的字符格式进行输出
        switch (str.length()){
            case 1:
                // 1为单字符，单字符直接转ascii码输出
                charNum = (int)str[0];
                break;
            case 2:
                // 2为转移字符，需要针对第二个字符进行转化
                charNum = (int)str[1];
                break;
            case 4:
                // 4位十六进制ascii码，需要将\x后的值进行转化
                // 首先大写转小写，防止影响
                if(isupper(str[2])) str[2] = tolower(str[2]);
                if(isupper(str[3])) str[3] = tolower(str[3]);
                // 数字直接减去'0'即可
                // 数字则需要基于'a'再加10
                if(isdigit(str[2])) charNum+=16*(str[2]-'0');
                else charNum+=16*(str[2]-'a'+10);
                if(isdigit(str[3])) charNum+=(str[3]-'0');
                else charNum+=(str[3]-'a'+10);
                break;
            default:
                charNum = -1;
                break;
        }
        return charNum;
    }

    std::pair<cc0::resultInfo, std::optional<CompilationError>> Analyser::Analyse() {
        auto err = c0Program();
        if (err.has_value()){
            return std::make_pair(result, err);
        }
        else{
            result.funcList = funcList;
            result.constList = constList;
            result.globalCode = globalCode;
            return std::make_pair(result, std::optional<CompilationError>());
        }
    }

    // <C0-program> ::=
    //    {<variable-declaration>}{<function-definition>}
    // 注意没有内容也是可以的
    // 但是如果要有运行的内容就必须有main函数
    // 最后注意，不是所有情况都需要对错误进行输出，如果是子过程报错，那么在子过程已经输出了错误信息，自己遇到时直接return err即可
    std::optional<CompilationError> Analyser::c0Program() {
        std::optional<CompilationError> err;
        std::optional<Token> next;

        globalFlag = true;
        globalCode.clear();
        globalVarList.clear();
        globalOffset = 0;
        funcList.clear();
        constList.clear();
        jumpPos.clear();
        inLoop = false;
        inSwitch = false;

        // 全局不记录栈偏移：全局变量的脚标就等于栈偏移
        while (true) {
            next = nextToken();
            if (!next.has_value()) {
                // 这里是说明在函数定义之前遇到文件尾，因此直接报错即可
                errOut(_current_pos,"need main function");
                return opError;
            }
            // 如果是const一定是变量定义
            if (next.value().GetType() == TT::CONST) {
                unreadToken();
                err = variableDeclaration();
                if(err.has_value()){
                    return err;
                }
                continue;
            }
            // 如果void一定是函数定义
            if(next.value().GetType() == TT::VOID){
                globalFlag = false;
                unreadToken();
                err = functionDefinition();
                if(err.has_value()){
                    return err;
                }
                // 后面一定是函数定义
                break;
            }
            // 如果int需要再次预读
            if(next.value().GetType() == TT::INT){
                next = nextToken();
                if(!next.has_value()||next.value().GetType() != TT::IDENTIFIER){
                    errOut(_current_pos,"var declaration need var name");
                    return opError;
                }
                next = nextToken();
                if(!next.has_value()){
                    errOut(_current_pos,"var is not declared in this scope");
                    return opError;
                }
                // 左括号一定为函数
                if(next.value().GetType() == TT::LEFT_PARENTHESIS){
                    globalFlag = false;
                    unreadToken(); // 左括号
                    unreadToken(); // 标识符
                    unreadToken(); // INT
                    err = functionDefinition();
                    if(err.has_value()){
                        return err;
                    }
                    break;
                }
                unreadToken(); // 当前符号
                unreadToken(); // 标识符
                unreadToken(); // INT
                err = variableDeclaration();
                if(err.has_value()){
                    return err;
                }
                continue;
            }
            errOut(_current_pos,"wrong var or function type");
            return opError;
        }
        while (true) {
            // 判断程序结束
            next = nextToken();
            if (!next.has_value()) {
                // 输入结束，判断有无main函数
                if(getFunc("main") == -1){
                    errOut(_current_pos,"no main function to run");
                    return opError;
                }
                return {};
            }
            // 如果代码最后有奇怪的东西则借助函数定义判断
            unreadToken();
            err = functionDefinition();
            if(err.has_value()){
                return err;
            }
        }
        return {};
    }

    // 语义要求只能int类型，且const必须初始化
    // <variable-declaration> ::=
    //    [<const-qualifier>]<type-specifier><init-declarator-list>';'
    std::optional<CompilationError> Analyser::variableDeclaration() {
        bool _const;
        std::optional<CompilationError> err;
        auto next = nextToken();
        if (!next.has_value()) {
            errOut(_current_pos,"var is not declared in this scope");
            return opError;
        }
        if (next.value().GetType() == TT::CONST) {
            _const = true;
        } else {
            unreadToken();
            _const = false;
        }
        // 变量类型此时只能是int
        next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::INT){
            errOut(_current_pos,"wrong var declaration type ");
            return opError;
        }
        // 如果多类型还需要传入type
        err = initDeclaratorList(_const);
        if(err.has_value()){
            return err;
        }

        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
            errOut(_current_pos,"no semicolon");
            return opError;
        }

        return {};
    }

    // <init-declarator-list> ::=
    //    <init-declarator>{','<init-declarator>}
    std::optional<CompilationError> Analyser::initDeclaratorList(bool isConst) {
        auto err = initDeclarator(isConst);
        if(err.has_value()){
            return err;
        }
        while (true) {
            auto next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::COMMA) {
                unreadToken();
                return {};
            }
            // 是逗号，进行定义
            err = initDeclarator(isConst);
            if(err.has_value()){
                return err;
            }
        }
        return {};
    }

    // <init-declarator> ::=
    //    <identifier>[<initializer>]
    // <initializer> ::=
    //    '='<expression>
    std::optional<CompilationError> Analyser::initDeclarator(bool isConst) {
        varInfo tempVar;
        std::pair<string,string> tempPair;
        tempVar.isConst = isConst;
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            errOut(_current_pos,"declaration need identifier");
            return opError;
        }
        tempVar.varName = next.value().GetValueString();
        if(globalFlag){
            // 全局只需考虑全局和函数
            if(isDeclared(tempVar.varName,"global")||getFunc(tempVar.varName)!=-1){
                errOut(_current_pos,"duplicated var declaration");
                return opError;
            }
            globalVarList.emplace_back(tempVar);
            tempPair = getVar(tempVar.varName);
            if(tempPair.first == "-"){
                errOut(_current_pos,"var is not declared in this scope");
                return opError;
            }
            // 变量占位
            globalCode.emplace_back(IPUSH,defaultValue);
            globalCode.emplace_back(LOADA,tempPair.first,tempPair.second);
//            cout << tempVar.isConst <<" int "<< tempVar.varName << '\n';
        }else{
            // 局部的话只需要考虑当前作用域
            // 声明为参数的标识符，在同级作用域中只能被声明一次，可以覆盖全局的
            // 此时不是全局变量定义，因此可以有全局变量定义，只需判断local即可
            if(isDeclared(tempVar.varName,"local")){
                errOut(_current_pos,"duplicated var declaration");
                return opError;
            }
            localVarList.emplace_back(tempVar);
            tempPair = getVar(tempVar.varName);
            if(tempPair.first == "-"){
                errOut(_current_pos,"var is not declared in this scope");
                return opError;
            }
            // 变量占位
            localCode.emplace_back(IPUSH,defaultValue);
            localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
//            cout << tempVar.isConst <<" int "<< tempVar.varName << '\n';
        }

        // 预读判断初始化
        if(isConst){
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::EQUAL_SIGN) {
                errOut(_current_pos,"const var need initialization");
                return opError;
            }
            auto err = normalExpression();
            if(err.has_value()){
                return err;
            }
            if(globalFlag){
                globalCode.emplace_back(ISTORE);
            }else{
                localCode.emplace_back(ISTORE);
            }
            return {};
        }else{
            next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::EQUAL_SIGN) {
                auto err = normalExpression();
                if(err.has_value()){
                    return err;
                }
                if(globalFlag){
                    globalCode.emplace_back(ISTORE);
                }else{
                    localCode.emplace_back(ISTORE);
                }
                return {};
            } else {
                unreadToken();
                if(globalFlag){
                    globalCode.emplace_back(IPUSH,defaultValue);
                    globalCode.emplace_back(ISTORE);
                }else{
                    localCode.emplace_back(IPUSH,defaultValue);
                    localCode.emplace_back(ISTORE);
                }
                return {};
            }
        }
        return {};
    }

    // <function-definition> ::=
    //    <type-specifier><identifier><parameter-clause><compound-statement>
    // 函数栈的栈偏移借助localOffset进行
    // 函数中的控制流，如果存在没有返回语句的分支，这些分支的具体返回值是未定义行为
    // 为了方便，和变量一样，int类型return则就返回值默认为0
    // 但是汇编必须保证每一种控制流分支都能够返回（没有return也能返回）。
    std::optional<CompilationError> Analyser::functionDefinition() {
        // 更新局部变量表和局部指令集
        localVarList.clear();
        localCode.clear();
        localOffset = 0;
        resetFunc();
        funcNow.isReturn = false;
        auto next = nextToken();
        if(!next.has_value()){
            errOut(_current_pos,"need function type");
            return opError;
        }
        if(next.value().GetType() == TT::VOID) {
            funcNow.funcType = "void";
        }else{
            if(next.value().GetType() == TT::INT){
                // 先直接int，因为函数没有返回值直接默认即可
                funcNow.funcType = "int";
            }else{
                errOut(_current_pos,"wrong function type");
                return opError;
            }
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            errOut(_current_pos,"declaration need function name");
            return opError;
        }
        funcNow.funcName = next.value().GetValueString();
        // 一定全局，直接判断重名
        if(isDeclared(funcNow.funcName,"global")||getFunc(funcNow.funcName) != -1){
            errOut(_current_pos,"duplicated var declaration");
            return opError;
        }
        funcNow.paramNum = 0;
        auto err = parameterClause(funcNow.paramNum);
        if(err.has_value()){
            return err;
        }
        // 更新函数指令集
        funcNow.localCode = localCode;
        constInfo tempConst;
        tempConst.type = 'S';
        tempConst.value = '\"'+funcNow.funcName+'\"';
        constList.emplace_back(tempConst);
        funcNow.constOffset = constList.size()-1;
        // 先更新偏移再入栈
        funcList.emplace_back(funcNow);
//        cout << funcNow.funcType << ' ' << funcNow.funcName << ' ' << funcNow.paramNum << '\n';
        err = compoundStatement();
        if(err.has_value()){
            return err;
        }
        // 保存函数的代码然后更新
        funcNow.localCode = localCode;
        funcList.back() = funcNow;
        return {};
    }

    // <parameter-clause> ::=
    //    '(' [<parameter-declaration-list>] ')'
    std::optional<CompilationError> Analyser::parameterClause(int& paramNum) {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \' ");
            return opError;
        }

        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::RIGHT_PARENTHESIS) {
            paramNum = 0;
            return {};
        } else {
            unreadToken();
            auto err = parameterDeclarationList(paramNum);
            if(err.has_value()){
                return err;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
                errOut(_current_pos,"no \' ) \' ");
                return opError;
            }
            return {};
        }
        return {};
    }

    // <parameter-declaration-list> ::=
    //    <parameter-declaration>{','<parameter-declaration>}
    std::optional<CompilationError> Analyser::parameterDeclarationList(int& paramNum) {
        auto err = parameterDeclaration();
        if(err.has_value()){
            return err;
        }
        paramNum++;
        while (true) {
            // 判断first
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::COMMA) {
                err = parameterDeclaration();
                if(err.has_value()){
                    return err;
                }
                paramNum++;
            } else {
                // 未使用回退
                unreadToken();
                return {};
            }
        }
    }

    // 同样有不能void的语义要求
    // <parameter-declaration> ::=
    //    [<const-qualifier>]<type-specifier><identifier>
    // 函数内部如果存在与其同名的变量或参数，将导致无法在此函数内调用自身（无法递归）
    std::optional<CompilationError> Analyser::parameterDeclaration() {
        varInfo tempVar;
        auto next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::CONST) {
            tempVar.isConst = true;
        } else {
            tempVar.isConst = false;
            unreadToken();
        }
        // 检测类型
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::INT) {
            errOut(_current_pos,"wrong var type ");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            errOut(_current_pos,"declaration need var name");
            return opError;
        }
        tempVar.varName = next.value().GetValueString();
        // 此时globalFlag一定是false，只用判断局部
        // 如果定义了但是是全局的也是正常的
        if(isDeclared(tempVar.varName,"local")){
            errOut(_current_pos,"duplicated var declaration");
            return opError;
        }
        localVarList.emplace_back(tempVar);
        std::pair<string,string> tempPair = getVar(tempVar.varName);
        if(tempPair.first == "-"){
            errOut(_current_pos,"var is not declared in this scope");
            return opError;
        }
        // 变量占位
        localCode.emplace_back(IPUSH,defaultValue);
        // 局部变量不用
//        localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
//        cout << tempVar.isConst <<" int "<< tempVar.varName << '\n';
        return {};
    }

    // <compound-statement> ::=
    //    '{' {<variable-declaration>} <statement-seq> '}'
    std::optional<CompilationError> Analyser::compoundStatement() {
        std::optional<CompilationError> err;
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_BRACE) {
            errOut(_current_pos,"no \' { \' ");
            return opError;
        }
        // 预读进行变量声明
        while (true) {
            next = nextToken();
            if (next.has_value()) {
                auto type = next.value().GetType();
                if (type == TT::CONST || type == TT::INT) {
                    // 回退标识符
                    unreadToken();
                    err = variableDeclaration();
                    if(err.has_value()){
                        return err;
                    }
                } else {
                    // 不是变量定义
                    unreadToken();
                    break;
                }
            } else {
                // 无值
                unreadToken();
                break;
            }
        }
        err = statementSequence();
        if(err.has_value()){
            return err;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_BRACE) {
            errOut(_current_pos,"no \' } \' ");
            return opError;
        }
        // 对函数未返回的情况进行处理
        if(!funcNow.isReturn){
            if(funcNow.funcType == "void"){
                // 如果void直接手动ret即可
                localCode.emplace_back(RET);
            }
            if(funcNow.funcType == "int"){
                // 如果int则说明没有return语句，一定错误
                errOut(_current_pos,"function need return value");
                return opError;
            }
        }
        return {};
    }

    // <statement-seq> ::=
    //	{<statement>}
    std::optional<CompilationError> Analyser::statementSequence() {
        while (true) {
            // 预读判断follow集而不是判断first集（first集太大）
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::RIGHT_BRACE) {
                // 不要提前使用，按照语法来
                unreadToken();
                return {};
            }
            unreadToken();
            auto err = normalStatement();
            if(err.has_value()){
                return err;
            }
        }
        return {};
    }

    // <statement> ::=
    //     '{' <statement-seq> '}'      # {
    //    |<condition-statement>        # if
    //    |<loop-statement>             # while
    //    |<jump-statement>             # return
    //    |<print-statement>            # print
    //    |<scan-statement>             # scan
    //    |<assignment-expression>';'   # identifier + =
    // 注：只有这里直接判断短的函数调用，可以忽略返回值，其他地方都是需要利用的
    // 因此直接在expression中使用到func call的地方必须全有返回值
    //    |<function-call>';'           # identifier + (
    //    |';'
    std::optional<CompilationError> Analyser::normalStatement() {
        std::optional<CompilationError> err;
        auto next = nextToken();
        if (!next.has_value()) {
            errOut(_current_pos,"incomplete statement");
            return opError;
        }
        // 预读first集
        switch (next.value().GetType()) {
            case SEMICOLON:
                break;
            case SWITCH:
            case IF:
                unreadToken();
                err = conditionStatement();
                if(err.has_value()){
                    return err;
                }
                break;
            case WHILE:
            case DO:
            case FOR:
                unreadToken();
                err = loopStatement();
                if(err.has_value()){
                    return err;
                }
                break;
            case BREAK:
            case CONTINUE:
            case RETURN:
                unreadToken();
                err = jumpStatement();
                if(err.has_value()){
                    return err;
                }
                break;
            case PRINT:
                unreadToken();
                err = printStatement();
                if(err.has_value()){
                    return err;
                }
                break;
            case SCAN:
                unreadToken();
                err = scanStatement();
                if(err.has_value()){
                    return err;
                }
                break;
            case LEFT_BRACE:
                err = statementSequence();
                if(err.has_value()){
                    return err;
                }
                next = nextToken();
                if (!next.has_value() || next.value().GetType() != TT::RIGHT_BRACE) {
                    errOut(_current_pos,"no \' } \' ");
                    return opError;
                }
                break;
            case IDENTIFIER:
                next = nextToken();
                if (next.has_value()) {
                    auto type = next.value().GetType();
                    // 赋值
                    if (type == TT::EQUAL_SIGN) {
                        unreadToken();
                        unreadToken();
                        err = assignmentExpression();
                        if(err.has_value()){
                            return err;
                        }
                        next = nextToken();
                        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                            errOut(_current_pos,"no semicolon");
                            return opError;
                        }
                        break;
                    }
                    // 函数调用
                    if (type == TT::LEFT_PARENTHESIS) {
                        unreadToken();
                        unreadToken();
                        err = functionCall();
                        if(err.has_value()){
                            return err;
                        }
                        next = nextToken();
                        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                            errOut(_current_pos,"no semicolon");
                            return opError;
                        }
                        break;
                    }
                    // 相当于default
                    errOut(_current_pos,"wrong statement content");
                    return opError;
                }else{
                    // 只有变量名没有后续
                    errOut(_current_pos,"wrong statement content");
                    return opError;
                }
                break;
            default:
                errOut(_current_pos,"wrong statement content");
                return opError;
        }
        return {};
    }

    // <function-call> ::=
    //    <identifier> '(' [<expression-list>] ')'
    std::optional<CompilationError> Analyser::functionCall() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            errOut(_current_pos,"functionCall need function name");
            return opError;
        }
        string tempName = next.value().GetValueString();
        // 使用时同样需要判断是否存在
        // 如果是本地变量（覆盖了函数名）或者不是函数
        if(isDeclared(tempName,"local")||getFunc(tempName) == -1){
            errOut(_current_pos,"no such function to call");
            return opError;
        }
        funcInfo funcTemp = funcList[getFunc(tempName)];
//        cout << next.value().GetValueString() << " call" << '\n';
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \' ");
            return opError;
        }
        // 预读判断有没有表达式列表
        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::RIGHT_PARENTHESIS) {
            // 比较参数个数
            if(funcTemp.paramNum!=0){
                errOut(_current_pos,"param num not match ");
                return opError;
            }
            // 一定不是全局
            localCode.emplace_back(CALL,to_string(funcTemp.constOffset));
            return {};
        } else {
            unreadToken();
            int paramNum = 0;
            auto err = expresionList(paramNum);
            // 同步判断
            if(err.has_value()){
                return err;
            }
            // 同样进行个数判断
            if(paramNum != funcTemp.paramNum){
                errOut(_current_pos,"param num not match ");
                return opError;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
                errOut(_current_pos,"no \' ) \' ");
                return opError;
            }
            localCode.emplace_back(CALL,to_string(funcTemp.constOffset));
            return {};
        }
        return {};
    }

    // <expression-list> ::=
    //    <expression>{','<expression>}
    std::optional<CompilationError> Analyser::expresionList(int& paramNum) {
        auto err = normalExpression();
        if(err.has_value()){
            return err;
        }
        paramNum++;
        while (true) {
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::COMMA) {
                err = normalExpression();
                if(err.has_value()){
                    return err;
                }
                paramNum++;
            } else {
                unreadToken();
                return {};
            }
        }
        return {};
    }

    // <assignment-expression> ::=
    //    <identifier><assignment-operator><expression>
    std::optional<CompilationError> Analyser::assignmentExpression() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            errOut(_current_pos,"assignment need var name");
            return opError;
        }
        string tempName = next.value().GetValueString();
        if(((!isDeclared(tempName,"local"))&&(!isDeclared(tempName,"global")))||getFunc(tempName)!=-1){
            errOut(_current_pos,"the var is not declared or is a function");
            return opError;
        }
        std::pair<std::string,std::string> tempPair;
        // 看是不是局部变量
        if(isDeclared(tempName,"local")){
            if(isConst(tempName)){
                errOut(_current_pos,"const var can not be assigned");
                return opError;
            }
            tempPair = getVar(tempName);
            if(tempPair.first == "-"){
                errOut(_current_pos,"var is not declared in this scope");
                return opError;
            }
            localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
            // 不使用值，不加载
//            localCode.emplace_back(ILOAD);
        }else{
            // 看是不是全局
            if(isDeclared(tempName,"global")){
                if(isConst(tempName)){
                    errOut(_current_pos,"const var can not be assigned");
                    return opError;
                }
                tempPair = getVar(tempName);
                if(tempPair.first == "-"){
                    errOut(_current_pos,"var is not declared in this scope");
                    return opError;
                }
                if(globalFlag){
                    globalCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                }else{
                    localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                }
            }else{
                errOut(_current_pos,"var is not declared");
                return opError;
            }
        }

        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::EQUAL_SIGN) {
            errOut(_current_pos,"assignment need equal sign");
            return opError;
        }
        auto err = normalExpression();
        if(err.has_value()){
            return err;
        }
        if(globalFlag){
            globalCode.emplace_back(ISTORE);
        }else{
            localCode.emplace_back(ISTORE);
        }

        return {};
    }

    // <jump-statement> ::=
    //     'break' ';'
    //    |'continue' ';'
    //    |<return-statement>
    std::optional<CompilationError> Analyser::jumpStatement() {
        std::optional<CompilationError> err;
        auto next = nextToken();
        if (!next.has_value()) {
            errOut(_current_pos,"incomplete jumpStatement");
            return opError;
        }
        jumpInfo tmpJmp;
        if(next.value().GetType() == TT::CONTINUE){
            if(!inLoop){
                errOut(_current_pos,"wrong continue position");
                return opError;
            }
            // 预读判断分号
            next = nextToken();
            if(!next.has_value()||next.value().GetType() != TT::SEMICOLON){
                errOut(_current_pos,"no semicolon");
                return opError;
            }
            // 此时的size就是下一条jmp语句的位置
            tmpJmp.pos = localCode.size();
            tmpJmp.type = "continue";
            jumpPos.emplace_back(tmpJmp);
            localCode.emplace_back(JMP);
            return {};
        }
        if(next.value().GetType() == TT::BREAK){
            if(!inLoop&&!inSwitch){
                errOut(_current_pos,"wrong break position");
                return opError;
            }
            next = nextToken();
            if(!next.has_value()||next.value().GetType() != TT::SEMICOLON){
                errOut(_current_pos,"no semicolon");
                return opError;
            }
            tmpJmp.pos = localCode.size();
            tmpJmp.type = "break";
            jumpPos.emplace_back(tmpJmp);
            localCode.emplace_back(JMP);
            return {};
        }
        if(next.value().GetType() == TT::RETURN){
            unreadToken();
            err = returnStatement();
            if(err.has_value()){
                return err;
            }
            return {};
        }
        errOut(_current_pos,"wrong jump statement");
        return opError;
    }

    // <return-statement> ::= 'return' [<expression>] ';'
    std::optional<CompilationError> Analyser::returnStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RETURN) {
            errOut(_current_pos,"no \' return \'");
            return opError;
        }
        // return一定在函数中
        if(funcNow.funcType == "void"){
            next = nextToken();
            // 如果返回，必须无值
            funcNow.isReturn = true;
            localCode.emplace_back(RET);
            if(!next.has_value()||next.value().GetType() != TT::SEMICOLON){
                errOut(_current_pos,"\' void \' function cannot have return value");
                return opError;
            }
            return {};
        }
        if(funcNow.funcType == "int"){
            next = nextToken();
            // int则必须有值
            if(!next.has_value()||next.value().GetType() == TT::SEMICOLON){
                errOut(_current_pos,"\' int \' function need return value");
                return opError;
            }
            unreadToken();
            auto err = normalExpression();
            if(err.has_value()){
                return err;
            }
            // 也要注意判断分号
            next = nextToken();
            if(!next.has_value()||next.value().GetType() != TT::SEMICOLON){
                errOut(_current_pos,"no semicolon");
                return opError;
            }
            funcNow.isReturn = true;
            localCode.emplace_back(IRET);
            return {};
        }
        errOut(_current_pos,"wrong function type");
        return opError;
    }

    // <condition-statement> ::=
    //     'if' '(' <condition> ')' <statement> ['else' <statement>]
    //    |'switch' '(' <expression> ')' '{' {<labeled-statement>} '}'
    std::optional<CompilationError> Analyser::conditionStatement() {
        auto next = nextToken();
        std::optional<CompilationError> err;
        // 此时next一定是两种条件语句的一个
        auto conditionType = next.value().GetType();
        switch (conditionType){
            case IF:
                unreadToken();
                err = ifCondition();
                if(err.has_value()){
                    return err;
                }
                break;
            case SWITCH:
                unreadToken();
                err = switchCondition();
                if(err.has_value()){
                    return err;
                }
                break;
            default:
                errOut(_current_pos,"wrong conditionStatement");
                return opError;
                break;
        }
        return {};
    }

    // 'if' '(' <condition> ')' <statement> ['else' <statement>]
    std::optional<CompilationError> Analyser::ifCondition() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IF) {
            errOut(_current_pos,"no \' if \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        auto err = normalCondition();
        if(err.has_value()){
            return err;
        }
        unsigned long long conditionEnd = localCode.size()-1;
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            errOut(_current_pos,"no \' ) \'");
            return opError;
        }
        err = normalStatement();
        if(err.has_value()){
            return err;
        }
        localCode[conditionEnd] = Instruction(localCode[conditionEnd].GetOperation(),to_string(localCode.size()));

        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::ELSE) {
            localCode.emplace_back(JMP);
            // 更新if的jump
            localCode[conditionEnd] = Instruction(localCode[conditionEnd].GetOperation(),to_string(localCode.size()));
            unsigned long long jumpEnd = localCode.size()-1;
            err = normalStatement();
            if(err.has_value()){
                return err;
            }
            localCode[jumpEnd] = Instruction(localCode[jumpEnd].GetOperation(),to_string(localCode.size()));
        } else {
            unreadToken();
            return {};
        }
        return {};
    }

    // <condition> ::=
    //     <expression>[<relational-operator><expression>]
    std::optional<CompilationError> Analyser::normalCondition() {
        auto err = normalExpression();
        if(err.has_value()){
            return err;
        }
        auto next = nextToken();
        if (!next.has_value()) {
            // 无值的返回也需要回退，因为会再读一次
            unreadToken();
            return {};
        }
        auto type = next.value().GetType();
        switch (type){
            case LESS_SIGN:
            case LESS_EQUAL_SIGN:
            case MORE_SIGN:
            case MORE_EQUAL_SIGN:
            case NOT_EQUAL_SIGN:
            case VALUE_EQUAL_SIGN:
                break;
            default:
                // 未使用的回退
                unreadToken();
                // 此时需要判断栈顶的值是否为0，0则是false，其余均为true
                localCode.emplace_back(IPUSH,defaultValue);
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JE);
                return {};
        }
        err = normalExpression();
        if(err.has_value()){
            return err;
        }
        switch (type){
            case LESS_SIGN:
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JGE);
                break;
            case LESS_EQUAL_SIGN:
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JG);
                break;
            case MORE_SIGN:
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JLE);
                break;
            case MORE_EQUAL_SIGN:
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JL);
                break;
            case NOT_EQUAL_SIGN:
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JE);
                break;
            case VALUE_EQUAL_SIGN:
                localCode.emplace_back(ICMP);
                localCode.emplace_back(JNE);
                break;
            default:
                errOut(_current_pos,"wrong condition statement");
                return opError;
        }
        return {};
    }

    // 'switch' '(' <expression> ')' '{' {<labeled-statement>} '}'
    std::optional<CompilationError> Analyser::switchCondition() {
        // switch中也会用到break
        // 有可能之前有jumpPos，因此需要储存
        std::vector<cc0::jumpInfo> beforeJumpPos;
        beforeJumpPos = jumpPos;
        jumpPos.clear();
        switchCode.clear();
        caseValue.clear();
        bool beforeInSwitch = inSwitch;
        inSwitch = true;
        auto next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::SWITCH){
            errOut(_current_pos,"incomplete switch");
            return opError;
        }
        next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::LEFT_PARENTHESIS){
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        auto eBegin = localCode.size();
        auto err = normalExpression();
        if(err.has_value()){
            return err;
        }
        auto eEnd = localCode.size()-1;
        // 将表达式代码进行另外存储，注意顺序一定从开始到结束
        for(unsigned long long i = eBegin;i<=eEnd;i++){
            switchCode.emplace_back(localCode[i]);
        }
        // 弹出对应条代码
        for(unsigned long long i = eBegin;i<=eEnd;i++){
            localCode.pop_back();
        }
        next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::RIGHT_PARENTHESIS){
            errOut(_current_pos,"no \' ) \'");
            return opError;
        }
        next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::LEFT_BRACE){
            errOut(_current_pos,"no \' { \'");
            return opError;
        }
        bool firstJump = false;
        while(true){
            next = nextToken();
            if(next.has_value()&&next.value().GetType()!=RIGHT_BRACE){
                // 如果第一个是case，则需要跳过第一行的强制jmp指令
                if(!firstJump&&next.value().GetType() == CASE){
                    firstJump = true;
                    localCode.emplace_back(JMP,to_string(localCode.size()+2));
                }
                unreadToken();
                err = labeledStatement();
                if(err.has_value()){
                    return err;
                }
            }else{
                // 右花括号交给外面处理
                unreadToken();
                break;
            }
        }
        next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::RIGHT_BRACE){
            errOut(_current_pos,"no \' } \'");
            return opError;
        }
        auto switchEnd = to_string(localCode.size());
        for(auto & jump : jumpPos) {
            // 都是无条件跳转，直接jmp就行
            if (jump.type == "break") {
                localCode[jump.pos] = Instruction(JMP, switchEnd);
            }
        }
        jumpPos = beforeJumpPos;
        inSwitch = beforeInSwitch;
        return {};
    }

    // <labeled-statement> ::=
    //     'case' (<integer-literal>|<char-literal>) ':' <statement>
    //    |'default' ':' <statement>
    // 每一个case都需要加载expression的值然后对interger进行比较
    std::optional<CompilationError> Analyser::labeledStatement(){
        auto next = nextToken();
        std::optional<CompilationError> err;
        if(!next.has_value()){
            errOut(_current_pos,"incomplete switch label");
            return opError;
        }
        auto labelType = next.value().GetType();
        // 防止跳过初始化语句，提前定义
        auto caseType = next.value().GetType();
        std::string nextValue;
        int jmpPos,statementEnd;
        jumpInfo tmpJmp;
        switch(labelType){
            case CASE:
                // 首先顺流下来的值，跳过自己的条件判断，直接执行代码，因此需要有个jmp
                // 当前的位置加上表达式代码的长度，再加上每个case判断时增加的三个指令的下一个
                localCode.emplace_back(JMP,to_string(localCode.size()+switchCode.size()+3+1));
                // 将expression的值加载过来
                for(auto & code:switchCode){
                    localCode.emplace_back(code);
                }
                next = nextToken();
                if(!next.has_value()){
                    errOut(_current_pos,"need compare value after \'case\'");
                    return opError;
                }
                caseType = next.value().GetType();
                // 这里只可能是int的，因此其他的都是类型错误
                switch(caseType){
                    case DECIMAL_INTEGER:
                    case HEXADECIMAL_INTEGER:
                        nextValue = next.value().GetValueString();
                        // case查重
                        for(auto & caseV:caseValue){
                            if(caseV == nextValue){
                                errOut(_current_pos,"duplicated case value");
                                return opError;
                            }
                        }
                        caseValue.emplace_back(nextValue);
                        localCode.emplace_back(IPUSH,nextValue);
                        localCode.emplace_back(ICMP);
                        // 如果不等于，就跳转到当前case语句的下一条
                        localCode.emplace_back(JNE);
                        break;
                    default:
                        errOut(_current_pos,"wrong case value type");
                        return opError;
                }
                next = nextToken();
                if(!next.has_value()||next.value().GetType()!=TT::COLON){
                    errOut(_current_pos,"no \' : \'");
                    return opError;
                }
                // 记录jne的位置
                jmpPos = localCode.size()-1;
                err = normalStatement();
                if(err.has_value()){
                    return err;
                }
                //
                // 当前statement的下一语句的位置，用于进行多重判断
                statementEnd = localCode.size();
                // 跳过前面的强制jmp
                localCode[jmpPos] = Instruction(localCode[jmpPos].GetOperation(),to_string(statementEnd+1));
                break;
            case DEFAULT:
                // 用default代替
                nextValue = "default";
                for(auto & caseV:caseValue){
                    if(caseV == nextValue){
                        errOut(_current_pos,"duplicated case value");
                        return opError;
                    }
                }
                caseValue.emplace_back(nextValue);
                // default直接默认执行，然后结束强制离开switch
                next = nextToken();
                if(!next.has_value()||next.value().GetType()!=TT::COLON){
                    errOut(_current_pos,"no \' : \'");
                    return opError;
                }
                err = normalStatement();
                if(err.has_value()){
                    return err;
                }
                // 添加虚无的break
                tmpJmp.pos = localCode.size();
                tmpJmp.type = "break";
                jumpPos.emplace_back(tmpJmp);
                localCode.emplace_back(JMP);
                break;
            default:
                errOut(_current_pos,"wrong switch label type");
                return opError;
        }
        return {};
    }

    // <loop-statement> ::=
    //    'while' '(' <condition> ')' <statement>
    //   |'do' <statement> 'while' '(' <condition> ')' ';'
    //   |'for' '('<for-init-statement> [<condition>]';' [<for-update-expression>]')' <statement>
    std::optional<CompilationError> Analyser::loopStatement() {
        // 首先记录循环开始的位置，用于continue和循环的再启动
        // 记录位置之后需要在所有语句编写之后进行回填
        // 对continue同样回填开始位置即可
        // 对break则需要回填一个循环退出后的位置
        // 对break和continue的寻找需要用一个vector进行存储，记录类型和位置
        // 记录之前的循环状态用于退出时更新
        bool beforeInLoop = inLoop;
        inLoop = true;
        auto next = nextToken();
        std::optional<CompilationError> err;
        // 此时next一定是三种循环的一个
        auto loopType = next.value().GetType();
        switch (loopType){
            case WHILE:
                unreadToken();
                err = whileLoop();
                if(err.has_value()){
                    return err;
                }
                break;
            case DO:
                unreadToken();
                err = doWhileLoop();
                if(err.has_value()){
                    return err;
                }
                break;
            case FOR:
                unreadToken();
                err = forLoop();
                if(err.has_value()){
                    return err;
                }
                break;
            default:
                errOut(_current_pos,"wrong loopStatement");
                return opError;
                break;
        }
        inLoop = beforeInLoop;
        return {};
    }
    // 'while' '(' <condition> ')' <statement>
    std::optional<CompilationError> Analyser::whileLoop() {
        // 记录之前的地址，防止嵌套出错
        std::vector<cc0::jumpInfo> beforeJumpPos;
        beforeJumpPos = jumpPos;
        jumpPos.clear();
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::WHILE) {
            errOut(_current_pos,"no \' while \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        // 记录开始位置，size就是下一个指令的位置
        auto conditionBegin = localCode.size();
        auto err = normalCondition();
        if(err.has_value()){
            return err;
        }
        // 记录条件判断语句的结束位置
        auto conditionEnd = localCode.size()-1;
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            errOut(_current_pos,"no \' ) \'");
            return opError;
        }
        err = normalStatement();
        if(err.has_value()){
            return err;
        }
        // 增加jmp到开头的循环语句，对所有的continue也要设置如此
        localCode.emplace_back(JMP,to_string(conditionBegin));
        // 记录循环语句的下一句的位置，更新jmp
        auto loopEnd = to_string(localCode.size());
        // 为条件判断处的jmp语句进行退出地址的回填，对所有的break也要设置如此
        localCode[conditionEnd] = Instruction(localCode[conditionEnd].GetOperation(),loopEnd);
        for(auto & jump : jumpPos){
            // 都是无条件跳转，直接jmp就行
            if(jump.type == "continue"){
                localCode[jump.pos] = Instruction(JMP,to_string(conditionBegin));
            }
            if(jump.type == "break"){
                localCode[jump.pos] = Instruction(JMP,loopEnd);
            }
        }
        jumpPos = beforeJumpPos;
        return {};
    }
    // 'do' <statement> 'while' '(' <condition> ')' ';'
    std::optional<CompilationError> Analyser::doWhileLoop() {
        std::vector<cc0::jumpInfo> beforeJumpPos;
        beforeJumpPos = jumpPos;
        jumpPos.clear();
        auto next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::DO){
            errOut(_current_pos,"no \' do \' ");
            return opError;
        }
        // {}是在statement中判断的
        auto loopBegin = localCode.size();
        auto err = normalStatement();
        if(err.has_value()){
            return err;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::WHILE) {
            errOut(_current_pos,"no \' while \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        // 记录开始位置，size就是下一个指令的位置
        auto conditionBegin = localCode.size();
        err = normalCondition();
        if(err.has_value()){
            return err;
        }
        // 记录条件判断语句的结束位置
        auto conditionEnd = localCode.size()-1;
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            errOut(_current_pos,"no \' ) \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
            errOut(_current_pos,"no semicolon");
            return opError;
        }
        // do-while需要在条件语句的最后jmp到循环的开头，和其他两个的区别
        localCode.emplace_back(JMP,to_string(loopBegin));
        auto loopEnd = to_string(localCode.size());
        // 为条件判断处的jmp语句进行退出地址的回填，对所有的break也要设置如此
        localCode[conditionEnd] = Instruction(localCode[conditionEnd].GetOperation(),loopEnd);
        for(auto & jump : jumpPos){
            // 都是无条件跳转，直接jmp就行
            if(jump.type == "continue"){
                localCode[jump.pos] = Instruction(JMP,to_string(conditionBegin));
            }
            if(jump.type == "break"){
                localCode[jump.pos] = Instruction(JMP,loopEnd);
            }
        }
        jumpPos = beforeJumpPos;
        return {};
    }
    // 'for' '('<for-init-statement> [<condition>]';' [<for-update-expression>]')' <statement>
    // <for-init-statement> ::=
    //    [<assignment-expression>{','<assignment-expression>}]';'
    // <for-update-expression> ::=
    //    (<assignment-expression>|<function-call>){','(<assignment-expression>|<function-call>)}
    std::optional<CompilationError> Analyser::forLoop() {
        std::vector<cc0::jumpInfo> beforeJumpPos;
        beforeJumpPos = jumpPos;
        jumpPos.clear();
        auto next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::FOR){
            errOut(_current_pos,"no \' for \'");
            return opError;
        }
        next = nextToken();
        if(!next.has_value()||next.value().GetType() != TT::LEFT_PARENTHESIS){
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        next = nextToken();
        if(!next.has_value()){
            errOut(_current_pos,"incomplete for head");
            return opError;
        }
        std::optional<CompilationError> err;
        // 判断有无初始化语句
        if(next.value().GetType() != TT::SEMICOLON){
            unreadToken();
            err = assignmentExpression();
            if(err.has_value()){
                return err;
            }
            while (true) {
                next = nextToken();
                if (next.has_value() && next.value().GetType() == TT::COMMA) {
                    err = assignmentExpression();
                    if(err.has_value()){
                        return err;
                    }
                } else {
                    unreadToken();
                    break;
                }
            }
            // 防止影响下一个
            next = nextToken();
            if(!next.has_value()||next.value().GetType()!=TT::SEMICOLON){
                errOut(_current_pos,"incomplete for head");
                return opError;
            }
        }
        next = nextToken();
        if(!next.has_value()){
            errOut(_current_pos,"incomplete for head");
            return opError;
        }
        // 判断有无条件语句
        bool hasCondition = false;
        auto conditionBegin = localCode.size();
        if(next.value().GetType() != TT::SEMICOLON){
            hasCondition = true;
            unreadToken();
            err = normalCondition();
            if(err.has_value()){
                return err;
            }
            next = nextToken();
            if(!next.has_value()||next.value().GetType()!=TT::SEMICOLON){
                errOut(_current_pos,"incomplete for head");
                return opError;
            }
        }
        auto conditionEnd = localCode.size()-1;
        // 没有条件则默认为真，直接顺序执行，不必更新conditionEnd
        next = nextToken();
        if(!next.has_value()){
            errOut(_current_pos,"incomplete for head");
            return opError;
        }
        // 判断有无更新语句
        // 如果没有update则直接跳转到情况开头即可
        auto updateBegin = conditionBegin;
        if(next.value().GetType() != TT::RIGHT_PARENTHESIS){
            // 更新语句需要放在语句的最后进行，因此需要两个跳转
            // 一个是开头跳转到update之后正常的语句
            // 另一个是结尾跳转到condition前面
            // 同时循环最后应该是跳转到update处而不是condition开头
            auto updateBefore = localCode.size();
            localCode.emplace_back(JMP);
            updateBegin = localCode.size();
            // 此时是标识符
//            if(!next.has_value()||next.value().GetType()!=TT::IDENTIFIER){
//                errOut(_current_pos,"wrong for update expression");
//                return opError;
//            }
            next = nextToken();//下一个符号
            // 赋值语句
            if(next.has_value()&&next.value().GetType() == TT::EQUAL_SIGN){
                unreadToken();
                unreadToken();
                err = assignmentExpression();
                if(err.has_value()){
                    return err;
                }
            }else{
                // 函数调用
                if(next.value().GetType() == TT::LEFT_PARENTHESIS){
                    unreadToken();
                    unreadToken();
                    err = functionCall();
                    if(err.has_value()){
                        return err;
                    }
                }else{
                    errOut(_current_pos,"wrong for update expression");
                    return opError;
                }
            }
            while (true) {
                next = nextToken();
                if (next.has_value() && next.value().GetType() == TT::COMMA) {
                    next = nextToken();// 此时应该是标识符
                    if(!next.has_value()){
                        errOut(_current_pos,"wrong for update expression");
                        return opError;
                    }
                    next = nextToken();// 符号
                    if(next.has_value()&&next.value().GetType() == TT::EQUAL_SIGN){
                        unreadToken();
                        unreadToken();
                        err = assignmentExpression();
                        if(err.has_value()){
                            return err;
                        }
                    }else{
                        if(next.value().GetType() == TT::LEFT_PARENTHESIS){
                            unreadToken();
                            unreadToken();
                            err = functionCall();
                            if(err.has_value()){
                                return err;
                            }
                        }else{
                            errOut(_current_pos,"wrong for update expression");
                            return opError;
                        }
                    }
                } else {
                    unreadToken();
                    break;
                }
            }
            next = nextToken();
            if(!next.has_value()||next.value().GetType() != TT::RIGHT_PARENTHESIS){
                errOut(_current_pos,"incomplete for head");
                return opError;
            }
            localCode.emplace_back(JMP,to_string(conditionBegin));
            localCode[updateBefore] = Instruction(JMP,to_string(localCode.size()));
        }
        err = normalStatement();
        if(err.has_value()){
            return err;
        }

        // for需要在语句的最后jmp到更新的开头
        localCode.emplace_back(JMP,to_string(updateBegin));
        auto loopEnd = to_string(localCode.size());
        // 为条件判断处的jmp语句进行退出地址的回填，break同样需要
        if(hasCondition){
            localCode[conditionEnd] = Instruction(localCode[conditionEnd].GetOperation(),loopEnd);
        }
        for(auto & jump : jumpPos){
            // 都是无条件跳转，直接jmp就行
            if(jump.type == "continue"){
                localCode[jump.pos] = Instruction(JMP,to_string(updateBegin));
            }
            if(jump.type == "break"){
                localCode[jump.pos] = Instruction(JMP,loopEnd);
            }
        }
        jumpPos = beforeJumpPos;
        return {};
    }

    // <scan-statement>  ::= 'scan' '(' <identifier> ')' ';'
    std::optional<CompilationError> Analyser::scanStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SCAN) {
            errOut(_current_pos,"no \' scan \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            errOut(_current_pos,"\'scan\' need var to assign");
            return opError;
        }
        string tempName = next.value().GetValueString();
        // 判断是否合法
        if(((!isDeclared(tempName,"local"))&&(!isDeclared(tempName,"global")))||getFunc(tempName)!=-1||isConst(tempName)){
            errOut(_current_pos,"var is not declaration or is function or is const");
            return opError;
        }
        std::pair<std::string,std::string> tempPair = getVar(tempName);
        if(tempPair.first == "-"){
            errOut(_current_pos,"var is not declared in this scope");
            return opError;
        }
        localCode.emplace_back(LOADA,tempPair.first,tempPair.second);

        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            errOut(_current_pos,"no \' ) \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
            errOut(_current_pos,"no semicolon");
            return opError;
        }

        localCode.emplace_back(ISCAN);
        localCode.emplace_back(ISTORE);

        return {};
    }

    // <print-statement> ::= 'print' '(' [<printable-list>] ')' ';'
    std::optional<CompilationError> Analyser::printStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::PRINT) {
            errOut(_current_pos,"no \' print \'");
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            errOut(_current_pos,"no \' ( \'");
            return opError;
        }
        // 预读判断follow
        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::RIGHT_PARENTHESIS) {
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                errOut(_current_pos,"no semicolon");
                return opError;
            }
        } else {
            unreadToken();
            auto err = printableList();
            if(err.has_value()){
                return err;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
                errOut(_current_pos,"no \' ) \'");
                return opError;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                errOut(_current_pos,"no semicolon");
                return opError;
            }
        }
        // 只要print输出结束，都要输出回车
        localCode.emplace_back(PRINTL);
        return {};
    }

    // <printable-list>  ::= <printable> {',' <printable>}
    std::optional<CompilationError> Analyser::printableList() {
        auto err = printable();
        if(err.has_value()){
            return err;
        }
        while (true) {
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::COMMA) {
                // 输出空格
                localCode.emplace_back(IPUSH,"32");
                localCode.emplace_back(CPRINT);
                err = printable();
                if(err.has_value()){
                    return err;
                }
                // 多个print需要加空格
            } else {
                // 注意回退)
                unreadToken();
                return {};
            }
        }
        return {};
    }

    // <printable> ::=
    //    <expression> | <string-literal> | <char-literal> # 有扩展
    std::optional<CompilationError> Analyser::printable() {
        auto next = nextToken();
        // 字面量
        if (next.has_value() && (next.value().GetType() == TT::STRING || next.value().GetType() == TT::CHAR)) {
            // string作为常量存储
            if(next.value().GetType() == TT::STRING){
                std::string str = next.value().GetValueString();
                constInfo tempConst;
                tempConst.type = 'S';
                tempConst.value = '\"'+str.substr(1,str.length()-2)+'\"';
                constList.emplace_back(tempConst);
                localCode.emplace_back(LOADC,to_string(constList.size()-1));
                localCode.emplace_back(SPRINT);
            }else{
                if(next.value().GetType() == TT::CHAR){
                    // char需要入栈输出，因为""和''还是不同的
                    std::string str = next.value().GetValueString();
                    int charNum = getCharNum(str);
                    if(charNum<0||charNum>255){
                        errOut(_current_pos,"wrong char content");
                        return opError;
                    }
                    localCode.emplace_back(IPUSH,to_string(charNum));
                    localCode.emplace_back(CPRINT);
                }else{
                    unreadToken();
                }
            }
            return {};
        } else {
            unreadToken();
        }
        // 表达式
        auto err = normalExpression();
        if(err.has_value()){
            return err;
        }
        localCode.emplace_back(IPRINT);
        return {};
    }

    // <expression> ::=
    //    <additive-expression>
    // 考虑到类型转化，每个表达式需要有一个类型的值，不断传递下去，直到表达式解析完成
    std::optional<CompilationError> Analyser::normalExpression() {
        auto err = additiveExpression();
        if(err.has_value()){
            return err;
        }
        return {};
    }

    // <additive-expression> ::= # 加减表达式
    //     <multiplicative-expression>{<additive-operator><multiplicative-expression>}
    std::optional<CompilationError> Analyser::additiveExpression() {
        auto err = multiplicativeExpression();
        if(err.has_value()){
            return err;
        }
        while (true) {
            // 预读判断+-
            auto next = nextToken();
            if (!next.has_value()) {
                unreadToken();
                return {};
            }
            if (next.value().GetType() == TT::PLUS_SIGN) {
                err = multiplicativeExpression();
                if(err.has_value()){
                    return err;
                }
                if(globalFlag){
                    globalCode.emplace_back(IADD);
                }else{
                    localCode.emplace_back(IADD);
                }
            } else {
                if (next.value().GetType() == TT::MINUS_SIGN) {
                    err = multiplicativeExpression();
                    if(err.has_value()){
                        return err;
                    }
                    if(globalFlag){
                        globalCode.emplace_back(ISUB);
                    }else{
                        localCode.emplace_back(ISUB);
                    }
                } else {
                    // 回退+-
                    unreadToken();
                    return {};
                }
            }
        }
        return {};
    }

    // <multiplicative-expression> ::=
    //     <cast-expression>{<multiplicative-operator><cast-expression>}
    std::optional<CompilationError> Analyser::multiplicativeExpression() {
        auto err = castExpression();
        if(err.has_value()){
            return err;
        }
        while (true) {
            // 预读判断*/
            auto next = nextToken();
            if (!next.has_value()) {
                unreadToken();
                return {};
            }
            if (next.value().GetType() == TT::MULTIPLICATION_SIGN) {
                err = castExpression();
                if(err.has_value()){
                    return err;
                }
                if(globalFlag){
                    globalCode.emplace_back(IMUL);
                }else{
                    localCode.emplace_back(IMUL);
                }
            } else {
                if (next.value().GetType() == TT::DIVISION_SIGN) {
                    err = unaryExpression();
                    if(err.has_value()){
                        return err;
                    }
                    if(globalFlag){
                        globalCode.emplace_back(IDIV);
                    }else{
                        localCode.emplace_back(IDIV);
                    }
                } else {
                    // 回退*/
                    unreadToken();
                    return {};
                }
            }
        }
        return {};
    }

    // <cast-expression> ::=
    //    {'('<type-specifier>')'}<unary-expression>
    std::optional<CompilationError> Analyser::castExpression() {
        bool isTransfer;
        auto next = nextToken();
        while(true){
            isTransfer = false;
            if(next.has_value()&&next.value().GetType() == TT::LEFT_PARENTHESIS){
                next = nextToken();
                if(!next.has_value()){
                    errOut(_current_pos,"wrong expression");
                    return opError;
                }
                auto type = next.value().GetType();
                switch (type){
                    case VOID:
                        errOut(_current_pos,"cannot transfer type to void");
                        return opError;
                    case INT:
                    case CHAR:
                        isTransfer = true;

                        next = nextToken();
                        if(!next.has_value()||next.value().GetType()!=TT::RIGHT_PARENTHESIS){
                            errOut(_current_pos,"incomplete type transfer");
                            return opError;
                        }
                        break;
                    default:
                        // 说明不是类型转化直接退出即可。
                        unreadToken();
                        unreadToken();
                        break;
                }
            }else{
                unreadToken();
            }
            if(isTransfer){
                next = nextToken();
            }else{
                break;
            }
        }
        auto err = unaryExpression();
        if(err.has_value()){
            return err;
        }
        return {};
    }

    // <unary-expression> ::=
    //    [<unary-operator>]<primary-expression>
    std::optional<CompilationError> Analyser::unaryExpression() {
        auto next = nextToken();
        if (!next.has_value()) {
            errOut(_current_pos,"expression is not complete");
            return opError;
        }
        bool _negative = false;
        if (next.value().GetType() == TT::PLUS_SIGN) _negative = false;
        else {
            if (next.value().GetType() == TT::MINUS_SIGN) {
                _negative = true;
            } else {
                // 没用上就回退
                unreadToken();
            }
        }
        auto err = primaryExpression();
        if(err.has_value()){
            return err;
        }
        // 加上符号
        if(_negative){
            if(globalFlag){
                globalCode.emplace_back(INEG);
            }else{
                localCode.emplace_back(INEG);
            }
        }
        return {};
    }

    // <primary-expression> ::=
    //     '('<expression>')'
    //    |<identifier>
    //    |<integer-literal>
    //    |<char-literal> # 取消这一个，最终没有实现
    //    |<function-call>
    std::optional<CompilationError> Analyser::primaryExpression() {
        auto next = nextToken();
        std::optional<CompilationError> err;
        if (!next.has_value()) {
            errOut(_current_pos,"expression is not complete");
            return opError;
        }
        switch (next.value().GetType()) {
            case LEFT_PARENTHESIS:
                err = normalExpression();
                if(err.has_value()){
                    return err;
                }
                next = nextToken();
                if (!next.has_value() || next.value().GetType() != RIGHT_PARENTHESIS) {
                    errOut(_current_pos,"no \' ) \' ");
                    return opError;
                }
                break;
            case DECIMAL_INTEGER:
            case HEXADECIMAL_INTEGER:
                if(globalFlag){
                    globalCode.emplace_back(IPUSH,next.value().GetValueString());
                }else{
                    localCode.emplace_back(IPUSH,next.value().GetValueString());
                }
                break;
            case IDENTIFIER:
                // 再次预读，判断是不是函数
                next = nextToken();
                // 函数
                if (next.has_value() && next.value().GetType() == TT::LEFT_PARENTHESIS) {
                    // 函数，回退到identifier之前
                    unreadToken();
                    unreadToken();
                    //再次预读判断返回值类型
                    next = nextToken();
                    string tempName = next.value().GetValueString();
                    // 未定义或者返回值为void
                    if(getFunc(tempName)==-1||funcList[getFunc(tempName)].funcType == "void"){
                        errOut(_current_pos,"function not declared or has no return value ");
                        return opError;
                    }
                    // 如果正常则需要再次回退
                    unreadToken();
                    err = functionCall();
                    if(err.has_value()){
                        return err;
                    }
                } else {
                    // 简单的变量，将预读的退回回到identifier之前
                    unreadToken();
                    unreadToken();
                    next = nextToken();
                    string tempName = next.value().GetValueString();
                    // 判断是否合法的变量，可能是全局，因为初始化时可以表达式赋值
                    if(globalFlag){
                        if(!isDeclared(tempName,"global")){
                            errOut(_current_pos,"var is not declared in this scope");
                            return opError;
                        }
                        std::pair<std::string,std::string> tempPair = getVar(tempName);
                        if(tempPair.first == "-"){
                            errOut(_current_pos,"var is not declared in this scope");
                            return opError;
                        }
                        globalCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                        globalCode.emplace_back(ILOAD);
                    }else{
                        // 如果没有本地或者全局定义就报错
                        if((!isDeclared(tempName,"local"))&&(!isDeclared(tempName,"global"))){
                            errOut(_current_pos,"var is not declared in this scope");
                            return opError;
                        }
                        std::pair<std::string,std::string> tempPair = getVar(tempName);
                        if(tempPair.first == "-"){
                            errOut(_current_pos,"var is not declared in this scope");
                            return opError;
                        }
                        localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                        localCode.emplace_back(ILOAD);
                    }
                }
                break;
            default:
                errOut(_current_pos,"expression is not complete");
                return opError;
        }
        return {};
    }

    std::optional<Token> Analyser::nextToken() {
        if (_offset == _tokens.size())
            return {};
        _current_pos = _tokens[_offset].GetEndPos();
        return _tokens[_offset++];
    }

    void Analyser::unreadToken() {
        if (_offset == 0)
            DieAndPrint("analyser unreads token from the begining.");
        _current_pos = _tokens[_offset - 1].GetEndPos();
        _offset--;
    }

    bool Analyser::isDeclared(const string& name,const string& type) {
        if(type=="local"){
            for(auto & var:localVarList)
                if(var.varName == name)
                    return true;
            return false;
        }
        if(type=="global"){
            for(auto & var:globalVarList)
                if(var.varName == name)
                    return true;
            return false;
        }
        return false;
    }
    bool Analyser::isConst(const string& name) {
        if(globalFlag){
            for(auto & var:globalVarList)
                if(var.varName == name)
                    if(var.isConst)
                        return true;
            return false;
        }else{
            // 是本地变量则只搜本地，否则搜外面
            if(isDeclared(name,"local")){
                for(auto & var:localVarList)
                    if(var.varName == name)
                        if(var.isConst)
                            return true;
                return false;
            }else{
                for(auto & var:globalVarList)
                    if(var.varName == name)
                        if(var.isConst)
                            return true;
                return false;
            }
        }
        return false;
    }
    std::pair<string,string> Analyser::getVar(const string& name) {
        unsigned long long i;
        // 需要返回栈内的偏移地址才行
        if(globalFlag){
            // 全局只查最外层
            for(i = 0;i<globalVarList.size();i++)
                if(globalVarList[i].varName == name)
                    return make_pair("0",to_string(i));
        }else{
            // 局部则需要从内向外
            for(i = 0;i<localVarList.size();i++)
                if(localVarList[i].varName == name)
                    return make_pair("0",to_string(i));
            for(i = 0;i<globalVarList.size();i++)
                if(globalVarList[i].varName == name)
                    return make_pair("1",to_string(i));
        }
        // 没有这个变量，出错
        return make_pair("-","-");
    }
    // 同样可判断是否和函数重名
    int Analyser::getFunc(const string& name) {
        unsigned long long i;
        for(i = 0;i<funcList.size();i++)
            if(funcList[i].funcName == name)
                return (int)i;
        return -1;
    }
//    int Analyser::getConst(const string& name) {
//        unsigned long long i;
//        string temp = '\"'+name+'\"';
//        for(i = 0;i<constList.size();i++)
//            if(constList[i].value == temp)
//                return (int)i;
//        return -1;
//    }
}