#include "analyser.h"

#include <climits>
#include <sstream>
#include <cstring>
#include <map>

using namespace std;
namespace cc0 {
#define TT TokenType
#define opError std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrAll)

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
    std::optional<CompilationError> Analyser::c0Program() {
        std::optional<CompilationError> err;
        std::optional<Token> next;

        globalFlag = true;
        globalCode.clear();
        globalVarList.clear();
        globalOffset = 0;
        funcList.clear();
        constList.clear();

        // 全局不记录栈偏移：全局变量的脚标就等于栈偏移
        while (true) {
            next = nextToken();
            if (!next.has_value()) {
                unreadToken();
                return {};
            }
            // 如果是const一定是变量定义
            if (next.value().GetType() == TT::CONST) {
                unreadToken();
                err = variableDeclaration();
                if (err.has_value()) {
              //      cout << "wrong var dec\n";
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
               //     cout << "wrong func dec\n";
                    return opError;
                }
                // 后面一定是函数定义
                break;
            }
            // 如果int需要再次预读
            if(next.value().GetType() == TT::INT){
                next = nextToken();
                if(!next.has_value()||next.value().GetType() != TT::IDENTIFIER){
                    cout << "no useful identifier\n";
                    return opError;
                }
                next = nextToken();
                if(!next.has_value()){
                    cout << "no useful value\n";
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
                        cout << "wrong func dec\n";
                        return opError;
                    }
                    break;
                }
                unreadToken(); // 当前符号
                unreadToken(); // 标识符
                unreadToken(); // INT
                err = variableDeclaration();
                if(err.has_value()){
                    cout << "wrong var dec\n";
                    return opError;
                }
                continue;
            }
        }
        while (true) {
            // 判断程序结束
            next = nextToken();
            if (!next.has_value()) {
                // 输入结束，判断有无main函数
                if(getFunc("main") == -1){
                    cout << "need main\n";
                    return opError;
                }
                unreadToken();
                return {};
            }
            // 如果代码最后有奇怪的东西则借助函数定义判断
            unreadToken();
            err = functionDefinition();
            if (err.has_value()) {
        //        cout << "wrong func def\n";
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
            cout << "no useful value\n";
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
            cout << "var need int \n";
            return opError;
        }
        // 如果多类型还需要传入type
        err = initDeclaratorList(_const);
        if (err.has_value()) {
      //      cout << "wrong init dec list\n";
            return err;
        }

        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
            cout << "no semicolon\n";
            return opError;
        }

        return {};
    }

    // <init-declarator-list> ::=
    //    <init-declarator>{','<init-declarator>}
    std::optional<CompilationError> Analyser::initDeclaratorList(bool isConst) {
        auto err = initDeclarator(isConst);
        if(err.has_value()) {
            cout << "wrong init dec\n";
            return opError;
        }
        while (true) {
            auto next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::COMMA) {
                unreadToken();
                return {};
            }
            // 是逗号，进行定义
            err = initDeclarator(isConst);
            if (err.has_value()) {
        //        cout << "wrong init dec\n";
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
            cout << "no identifier\n";
            return opError;
        }
        tempVar.varName = next.value().GetValueString();
        if(globalFlag){
            // 全局只需考虑全局和函数
            if(isDeclared(tempVar.varName,"global")||getFunc(tempVar.varName)!=-1){
                cout << "duplicated dec\n";
                return opError;
            }
            globalVarList.emplace_back(tempVar);
            tempPair = getVar(tempVar.varName);
            if(tempPair.first == "-"){
                cout << "no useful var\n";
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
                cout << "duplicated dec\n";
                return opError;
            }
            localVarList.emplace_back(tempVar);
            tempPair = getVar(tempVar.varName);
            if(tempPair.first == "-"){
                cout << "no usrful var\n";
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
                cout << "const need init\n";
                return opError;
            }
            auto err = normalExpression();
            if (err.has_value()) {
       //         cout << "wrong expression\n";
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
                if (err.has_value()) {
           //         cout << "wrong expression\n";
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
            cout << "no usrful value\n";
            return opError;
        }
        if(next.value().GetType() == TT::VOID) {
            funcNow.funcType = "void";
        }else{
            if(next.value().GetType() == TT::INT){
                // 先直接int，因为函数没有返回值直接默认即可
                funcNow.funcType = "int";
            }else{
                cout << "func type error \n";
                return opError;
            }
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            cout << "no func name\n";
            return opError;
        }
        funcNow.funcName = next.value().GetValueString();
        // 一定全局，直接判断重名
        if(isDeclared(funcNow.funcName,"global")||getFunc(funcNow.funcName) != -1){
            cout << "duplicated dec\n";
            return opError;
        }
        funcNow.paramNum = 0;
        auto err = parameterClause(funcNow.paramNum);
        if (err.has_value()) {
      //      cout << "wrong param clause\n";
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
        if (err.has_value()) {
    //        cout << "wrong compound\n";
            return err;
        }
        // 保存函数的代码然后更新
        if(!funcNow.isReturn){
            localCode.emplace_back(RET);
        }
        funcNow.localCode = localCode;
        funcList.back() = funcNow;
        return {};
    }

    // <parameter-clause> ::=
    //    '(' [<parameter-declaration-list>] ')'
    std::optional<CompilationError> Analyser::parameterClause(int& paramNum) {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            cout << "no left ( \n";
            return opError;
        }

        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::RIGHT_PARENTHESIS) {
            paramNum = 0;
            return {};
        } else {
            unreadToken();
            auto err = parameterDeclarationList(paramNum);
            if (err.has_value()) {
       //         cout << "wrong param dec list\n";
                return err;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
                cout << "no right ) \n";
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
        if (err.has_value()) {
      //      cout << "wrong param dec\n";
            return err;
        }
        paramNum++;
        while (true) {
            // 判断first
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::COMMA) {
                err = parameterDeclaration();
                if (err.has_value()) {
             //       cout << "wrong param dec\n";
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
            cout << "var type error \n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            cout << "need identifier\n";
            return opError;
        }
        tempVar.varName = next.value().GetValueString();
        // 此时globalFlag一定是false，只用判断局部
        // 如果定义了但是是全局的也是正常的
        if(isDeclared(tempVar.varName,"local")){
            cout << "duplicated dec\n";
            return opError;
        }
        localVarList.emplace_back(tempVar);
        std::pair<string,string> tempPair = getVar(tempVar.varName);
        if(tempPair.first == "-"){
            cout << "no useful value\n";
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
            cout << "no left { \n";
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
                    if (err.has_value()) {
              //          cout << "wrong var dec\n";
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
        if (err.has_value()) {
        //    cout << "wrong state seq\n";
            return err;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_BRACE) {
            cout << "no right ) \n";
            return opError;
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
            if (err.has_value()) {
           //     cout << "wrong state\n";
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
            cout << "no usrful value\n";
            return opError;
        }
        // 预读first集
        switch (next.value().GetType()) {
            case SEMICOLON:
                break;
            case IF:
                unreadToken();
                err = conditionStatement();
                if (err.has_value()) {
            //        cout << "wrong condition state\n";
                    return err;
                }
                break;
            case WHILE:
                unreadToken();
                err = loopStatement();
                if (err.has_value()) {
             //       cout << "wrong loop state\n";
                    return err;
                }
                break;
            case RETURN:
                unreadToken();
                err = jumpStatement();
                if (err.has_value()) {
               //     cout << "wrong jump state\n";
                    return err;
                }
                break;
            case PRINT:
                unreadToken();
                err = printStatement();
                if (err.has_value()) {
              //      cout << "wrong print state\n";
                    return err;
                }
                break;
            case SCAN:
                unreadToken();
                err = scanStatement();
                if (err.has_value()) {
            //        cout << "wrong scan state\n";
                    return err;
                }
                break;
            case LEFT_BRACE:
                err = statementSequence();
                if (err.has_value()) {
            //        cout << "wrong state seq\n";
                    return err;
                }
                next = nextToken();
                if (!next.has_value() || next.value().GetType() != TT::RIGHT_BRACE) {
                    cout << "no right } \n";
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
                        if (err.has_value()) {
                //            cout << "wrong assign expre\n";
                            return err;
                        }
                        next = nextToken();
                        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                            cout << "no semicolon\n";
                            return opError;
                        }
                        break;
                    }
                    // 函数调用
                    if (type == TT::LEFT_PARENTHESIS) {
                        unreadToken();
                        unreadToken();
                        err = functionCall();
                        if (err.has_value()) {
                      //      cout << "wrong func call\n";
                            return err;
                        }
                        next = nextToken();
                        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                            cout << "no semicolon\n";
                            return opError;
                        }
                        break;
                    }
                    // 相当于default
                    return opError;
                }else{
                    // 只有变量名没有后续
                    return opError;
                }
                break;
            default:
                return opError;
        }
        return {};
    }

    // <function-call> ::=
    //    <identifier> '(' [<expression-list>] ')'
    std::optional<CompilationError> Analyser::functionCall() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            cout << "need identifier\n";
            return opError;
        }
        string tempName = next.value().GetValueString();
        // 使用时同样需要判断是否存在
        // 如果是本地变量（覆盖了函数名）或者不是函数
        if(isDeclared(tempName,"local")||getFunc(tempName) == -1){
            cout << "no such func\n";
            return opError;
        }
        funcInfo funcTemp = funcList[getFunc(tempName)];
//        cout << next.value().GetValueString() << " call" << '\n';
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            cout << "loss left ) \n";
            return opError;
        }
        // 预读判断有没有表达式列表
        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::RIGHT_PARENTHESIS) {
            // 比较参数个数
            if(funcTemp.paramNum!=0){
                cout << "wrong param num \n";
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
            if (err.has_value()||paramNum!=funcTemp.paramNum) {
          //      cout << "wrong expresion list\n";
                return err;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
                cout << "no right ) \n";
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
        if (err.has_value()) {
        //    cout << "wrong expression\n";
            return err;
        }
        paramNum++;
        while (true) {
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::COMMA) {
                err = normalExpression();
                if (err.has_value()) {
              //      cout << "wrong expression\n";
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
            cout << "no identifier\n";
            return opError;
        }
        string tempName = next.value().GetValueString();
        if(((!isDeclared(tempName,"local"))&&(!isDeclared(tempName,"global")))||getFunc(tempName)!=-1||isConst(tempName)){
            cout << "no useful var to assign: " << tempName << '\n';
            return opError;
        }
        std::pair<std::string,std::string> tempPair;
        // 看是不是局部变量
        if(isDeclared(tempName,"local")){
            if(isConst(tempName)){
                cout << "const can not be assign\n";
                return opError;
            }
            tempPair = getVar(tempName);
            if(tempPair.first == "-"){
                cout << "no usrful var\n";
                return opError;
            }
            localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
            // 不使用值，不加载
//            localCode.emplace_back(ILOAD);
        }else{
            // 看是不是全局
            if(isDeclared(tempName,"global")){
                if(isConst(tempName)){
                    cout << "const can not be assign\n";
                    return opError;
                }
                tempPair = getVar(tempName);
                if(tempPair.first == "-"){
                    cout << "no usrful var\n";
                    return opError;
                }
                if(globalFlag){
                    globalCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                }else{
                    localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                }
            }else{
                cout << "var need dec\n";
                return opError;
            }
        }

        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::EQUAL_SIGN) {
            cout << "assign need equal\n";
            return opError;
        }
        // 预读？
        auto err = normalExpression();
        if (err.has_value()) {
           // cout << "wrong expression\n";
            return err;
        }
        if(globalFlag){
            globalCode.emplace_back(ISTORE);
        }else{
            localCode.emplace_back(ISTORE);
        }

        return {};
    }

    // <jump-statement> ::= <return-statement>
    std::optional<CompilationError> Analyser::jumpStatement() {
        std::optional<CompilationError> err;
        auto next = nextToken();
        if (!next.has_value()) {
            cout << "no useful value\n";
            return opError;
        }
        if(next.value().GetType() == TT::RETURN){
            unreadToken();
            err = returnStatement();
            if(err.has_value()){
         //       cout << "wrong return\n";
                return err;
            }
            return {};
        }
        return opError;
    }

    // <return-statement> ::= 'return' [<expression>] ';'
    std::optional<CompilationError> Analyser::returnStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RETURN) {
            cout  << "loss return\n";
            return opError;
        }
        // return一定在函数中
        if(funcNow.funcType == "void"){
            next = nextToken();
            // 如果返回，必须无值
            if(!next.has_value()||next.value().GetType() != TT::SEMICOLON){
                funcNow.isReturn = true;
                localCode.emplace_back(RET);
                cout  << "no semicolon\n";
                return opError;
            }
            return {};
        }
        if(funcNow.funcType == "int"){
            next = nextToken();
            // int则必须有值
            if(!next.has_value()||next.value().GetType() == TT::SEMICOLON){
                cout  << "int need return value\n";
                return opError;
            }
            unreadToken();
            auto err = normalExpression();
            if(err.has_value()){
        //        cout << "wrong expre\n";
                return err;
            }
            // 也要注意判断分号
            next = nextToken();
            if(!next.has_value()||next.value().GetType() != TT::SEMICOLON){
                cout  << "no semicolon\n";
                return opError;
            }
            funcNow.isReturn = true;
            localCode.emplace_back(IRET);
            return {};
        }
        cout << "funcNow error\n";
        return opError;
    }

    // <condition-statement> ::=
    //    'if' '(' <condition> ')' <statement> ['else' <statement>]
    std::optional<CompilationError> Analyser::conditionStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IF) {
            cout << "no if\n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            cout << "no left ( \n";
            return opError;
        }
        auto err = normalCondition();
        if (err.has_value()) {
        //    cout << "wrong condition\n";
            return err;
        }
        unsigned long long conditionEnd = localCode.size()-1;
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            cout << "no right ) \n";
            return opError;
        }
        err = normalStatement();
        if (err.has_value()) {
        //    cout << "wrong statement\n";
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
            if (err.has_value()) {
         //       cout << "wrong statement\n";
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
        if (err.has_value()) {
     //       cout << "wrong expression\n";
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
        if (err.has_value()) {
     //       cout << "wrong expression\n";
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
                return {};
        }
        return {};
    }

    // <loop-statement> ::=
    //    'while' '(' <condition> ')' <statement>
    std::optional<CompilationError> Analyser::loopStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::WHILE) {
            cout << "no while\n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            cout << "no left ( \n";
            return opError;
        }
        auto conditionBegin = localCode.size();
        auto err = normalCondition();
        if (err.has_value()) {
      //      cout << "wrong condition\n";
            return err;
        }
        auto conditionEnd = localCode.size()-1;
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            cout << "no right ( \n";
            return opError;
        }
        err = normalStatement();
        if (err.has_value()) {
    //        cout << "wrong statement\n";
            return err;
        }
        localCode.emplace_back(JMP,to_string(conditionBegin));
        localCode[conditionEnd] = Instruction(localCode[conditionEnd].GetOperation(),to_string(localCode.size()));
        return {};
    }

    // <scan-statement>  ::= 'scan' '(' <identifier> ')' ';'
    std::optional<CompilationError> Analyser::scanStatement() {
        auto next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SCAN) {
            cout << "need scan\n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            cout << "no left ( \n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::IDENTIFIER) {
            cout << "scan need var\n";
            return opError;
        }
        string tempName = next.value().GetValueString();
        // 判断是否合法
        if(((!isDeclared(tempName,"local"))&&(!isDeclared(tempName,"global")))||getFunc(tempName)!=-1||isConst(tempName)){
            cout << "no such var\n";
            return opError;
        }
        std::pair<std::string,std::string> tempPair = getVar(tempName);
        if(tempPair.first == "-"){
            cout << "no usrful value\n";
            return opError;
        }
        localCode.emplace_back(LOADA,tempPair.first,tempPair.second);

        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
            cout << "no right ) \n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
            cout << "no semicolon\n";
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
            cout << "need print\n";
            return opError;
        }
        next = nextToken();
        if (!next.has_value() || next.value().GetType() != TT::LEFT_PARENTHESIS) {
            cout << "no left ( \n";
            return opError;
        }
        // 预读判断follow
        next = nextToken();
        if (next.has_value() && next.value().GetType() == TT::RIGHT_PARENTHESIS) {
            cout << "no right ) \n";
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                cout << "no semicolon\n";
                return opError;
            }
        } else {
            unreadToken();
            auto err = printableList();
            if (err.has_value()) {
        //        cout << "wrong print list\n";
                return err;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::RIGHT_PARENTHESIS) {
                cout << "no right ( \n";
                return opError;
            }
            next = nextToken();
            if (!next.has_value() || next.value().GetType() != TT::SEMICOLON) {
                cout << "no semicolon\n";
                return opError;
            }
        }
        return {};
    }

    // <printable-list>  ::= <printable> {',' <printable>}
    std::optional<CompilationError> Analyser::printableList() {
        auto err = printable();
        if (err.has_value()) {
      //      cout << "wrong print\n";
            return err;
        }
        while (true) {
            auto next = nextToken();
            if (next.has_value() && next.value().GetType() == TT::COMMA) {
                // 输出空格
                localCode.emplace_back(IPUSH,"32");
                localCode.emplace_back(CPRINT);
                err = printable();
                if (err.has_value()) {
           //         cout << "wrong print\n";
                    return err;
                }
                // 多个print需要加空格
            } else {
                // 输出结束，输出回车
                localCode.emplace_back(PRINTL);
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
                    std::string str = next.value().GetValueString();
                    constInfo tempConst;
                    tempConst.type = 'S';
                    tempConst.value = '\"'+str.substr(1,str.length()-2)+'\"';
                    constList.emplace_back(tempConst);
                    localCode.emplace_back(LOADC,to_string(constList.size()-1));
                    localCode.emplace_back(SPRINT);
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
        if (err.has_value()) {
        //    cout << "wrong expression\n";
            return err;
        }
        localCode.emplace_back(IPRINT);
        return {};
    }

    // <expression> ::=
    //    <additive-expression>
    std::optional<CompilationError> Analyser::normalExpression() {
        auto err = additiveExpression();
        if (err.has_value()) {
          //  cout << "wrong add expression\n";
            return err;
        }
        return {};
    }

    // <additive-expression> ::= # 加减表达式
    //     <multiplicative-expression>{<additive-operator><multiplicative-expression>}
    std::optional<CompilationError> Analyser::additiveExpression() {
        auto err = multiplicativeExpression();
        if (err.has_value()) {
          //  cout << "wrong mul expression\n";
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
                if (err.has_value()) {
                //    cout << "wrong mul expression\n";
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
                    if (err.has_value()) {
                 //       cout << "wrong mul expression\n";
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
    //    <unary-expression>{<multiplicative-operator><unary-expression>}
    std::optional<CompilationError> Analyser::multiplicativeExpression() {
        auto err = unaryExpression();
        if (err.has_value()) {
         //   cout << "wrong unary expression\n";
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
                err = unaryExpression();
                if (err.has_value()) {
             //       cout << "wrong unary expression\n";
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
                    if (err.has_value()) {
                //        cout << "wrong unary expression\n";
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

    // <unary-expression> ::=
    //    [<unary-operator>]<primary-expression>
    std::optional<CompilationError> Analyser::unaryExpression() {
        auto next = nextToken();
        if (!next.has_value()) {
            cout  << "expre need value\n";
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
        if (err.has_value()) {
            cout << "wrong pri expression\n";
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
    //    |<function-call>
    std::optional<CompilationError> Analyser::primaryExpression() {
        auto next = nextToken();
        std::optional<CompilationError> err;
        if (!next.has_value()) {
            cout << "no useful value\n";
            return opError;
        }
        switch (next.value().GetType()) {
            case LEFT_PARENTHESIS:
                err = normalExpression();
                if (err.has_value()) {
                    cout << "wrong expression\n";
                    return err;
                }
                next = nextToken();
                if (!next.has_value() || next.value().GetType() != RIGHT_PARENTHESIS) {
                    cout << "no right ) \n";
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
                        cout << "func not dec or is void \n";
                        return opError;
                    }
                    // 如果正常则需要再次回退
                    unreadToken();
                    err = functionCall();
                    if (err.has_value()) {
               //         cout << "wrong func call\n";
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
                            cout << "var not dec \n";
                            return opError;
                        }
                        std::pair<std::string,std::string> tempPair = getVar(tempName);
                        if(tempPair.first == "-"){
                            cout << "no such var\n";
                            return opError;
                        }
                        globalCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                        globalCode.emplace_back(ILOAD);
                    }else{
                        // 如果没有本地或者全局定义就报错
                        if((!isDeclared(tempName,"local"))&&(!isDeclared(tempName,"global"))){
                            cout << "var not dec \n";
                            return opError;
                        }
                        std::pair<std::string,std::string> tempPair = getVar(tempName);
                        if(tempPair.first == "-"){
                            cout << "no such value\n";
                            return opError;
                        }
                        localCode.emplace_back(LOADA,tempPair.first,tempPair.second);
                        localCode.emplace_back(ILOAD);
                    }
                }
                break;
            default:
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
        }else{
            for(auto & var:localVarList)
                if(var.varName == name)
                    if(var.isConst)
                        return true;
            for(auto & var:globalVarList)
                if(var.varName == name)
                    if(var.isConst)
                        return true;
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
    int Analyser::getConst(const string& name) {
        unsigned long long i;
        string temp = '\"'+name+'\"';
        for(i = 0;i<constList.size();i++)
            if(constList[i].value == temp)
                return (int)i;
        return -1;
    }
}