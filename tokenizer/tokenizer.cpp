#include "tokenizer/tokenizer.h"

#include <cctype>
#include <sstream>
#include <cstring>
#include <iostream>
#include <climits>
#include <algorithm>
#include <cstring>
#include <cmath>

using namespace std;

namespace cc0 {
#define TT TokenType
#define KEY std::pair<std::string,TokenType>
    TT checkKeywords(const std::string token) {
        // 夹杂私货
        KEY keywords[] = {
                KEY("const", TT::CONST),
                KEY("void", TT::VOID),
                KEY("int", TT::INT),
                KEY("char", TT::CHART),
                KEY("double", TT::DOUBLE),
                KEY("struct", TT::STRUCT),
                KEY("if", TT::IF),
                KEY("else", TT::ELSE),
                KEY("switch", TT::SWITCH),
                KEY("case", TT::CASE),
                KEY("default", TT::DEFAULT),
                KEY("while", TT::WHILE),
                KEY("for", TT::FOR),
                KEY("do", TT::DO),
                KEY("return", TT::RETURN),
                KEY("break", TT::BREAK),
                KEY("continue", TT::CONTINUE),
                KEY("print", TT::PRINT),
                KEY("scan", TT::SCAN),

                KEY("wyx", TT::IDENTIFIER)
        };
        for (int i = 0; keywords[i].first != "wyx"; i++) {
            if (token == keywords[i].first) {
                return (TT)keywords[i].second;
            }
        }
        return TT::IDENTIFIER;
    }
//    //获取数字大小
//    long long getTokenNum(std::string token) {
//        std::stringstream stol;
//        long long token_num;
//        stol << token;
//        stol >> token_num;
//        return token_num;
//    }
    // 判断是不是十六进制的字符
    bool isHexDigit(char ch){
        if(isupper(ch)) ch-=('A'-'a');
        if(isdigit(ch)) return true;
        return (ch>='a'&&ch<='f');
    }
    // 判断是不是十六进制数
    string checkHexDigit(string num){
        int l = num.length();
        // 0x后无内容
        if(l<=2) return "null";
        for(int i = 0;i<l;i++) if(isupper(num[i])) num[i]-=('A'-'a');
        //前两位不同
        if(!((num[0] == '0')&&(num[1] == 'x'))) return "null";
        for(int i = 2;i<l;i++) if(!isHexDigit(num[i])) return "null";
        return num;
    }
    // 将十六进制数转为十进制
    string changeHex(string s){
        long long sum = 0;
        int count=s.length();
        for(int i=count-1;i>=0;i--)//从十六进制个位开始，每位都转换成十进制
        {
            if(s[i]>='0'&&s[i]<='9')//数字字符的转换
            {
                sum+=(s[i]-48)*pow(16,count-i-1);
            }
            else if(s[i]>='a'&&s[i]<='f')//字母字符的转换
            {
                sum+=(s[i]-87)*pow(16,count-i-1);
            }
        }
        return to_string(sum);
    }
    bool isChar(char ch){
        if(ch=='\n'||ch=='\r'||ch=='\''||ch=='\"') return false;
        return ch >= ' ' && ch <= '~';
    }

    std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::NextToken() {
        if (!_initialized)
            readAll();
        if (_rdr.bad())
            return std::make_pair(std::optional<Token>(),
                                  std::make_optional<CompilationError>(0, 0, ErrorCode::ErrStreamError));
        if (isEOF())
            return std::make_pair(std::optional<Token>(),
                                  std::make_optional<CompilationError>(0, 0, ErrorCode::ErrEOF));
        auto p = nextToken();
        if (p.second.has_value())
            return std::make_pair(p.first, p.second);
        auto err = checkToken(p.first.value());
        if (err.has_value())
            return std::make_pair(p.first, err.value());
        return std::make_pair(p.first, std::optional<CompilationError>());
    }

    std::pair<std::vector<Token>, std::optional<CompilationError>> Tokenizer::AllTokens() {
        std::vector<Token> resultToken;
        resultToken.clear(); // 相当于新建，不过后面还能处理
        while (true) {
            // 这里看清，等于的是Next，不是实现的next，Next中还会对内容进行进一步的检查，例如标识符的开头问题
            auto p = NextToken();
            if (p.second.has_value()) {
                if (p.second.value().GetCode() == ErrorCode::ErrEOF) // 正常结束
                    return std::make_pair(resultToken, std::optional<CompilationError>());
                else
                    return std::make_pair(std::vector<Token>(), p.second);
            }
            resultToken.emplace_back(p.first.value());
        }
    }

    // 注意：这里的返回值中 Token 和 CompilationError 只能返回一个，不能同时返回。
    std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::nextToken() {
        // 用于存储已经读到的组成当前token字符
        std::stringstream ss;
        // 分析token的结果，作为此函数的返回值
        std::pair<std::optional<Token>, std::optional<CompilationError>> result;
        // <行号，列号>，表示当前token的第一个字符在源代码中的位置
        std::pair<int64_t, int64_t> pos;
        // 记录当前自动机的状态，进入此函数时是初始状态
        DFAState current_state = DFAState::INITIAL_STATE;
        // 这是一个死循环，除非主动跳出
        // 每一次执行while内的代码，都可能导致状态的变更
        while (true) {
            // 读一个字符，请注意auto推导得出的类型是std::optional<char>
            // 这里其实有两种写法
            // 1. 每次循环前立即读入一个 char
            // 2. 只有在可能会转移的状态读入一个 char
            // 因为我们实现了 unread，为了省事我们选择第一种
            auto current_char = nextChar();
            // 针对当前的状态进行不同的操作
            switch (current_state) {

                // 初始状态
                // 这个 case 我们给出了核心逻辑，但是后面的 case 不用照搬。
                case INITIAL_STATE: {
                    // 已经读到了文件尾
                    if (!current_char.has_value())
                        // 返回一个空的token，和编译错误ErrEOF：遇到了文件尾
                        // 文件尾会作为特殊的错误判断，因此不必担心
                        return std::make_pair(std::optional<Token>(),
                                              std::make_optional<CompilationError>(0, 0, ErrEOF));

                    // 获取读到的字符的值，注意auto推导出的类型是char，数字需要转化
                    auto ch = current_char.value();
                    // 标记是否读到了不合法的字符，初始化为否
                    auto invalid = false;

                    // 使用了自己封装的判断字符类型的函数，定义于 tokenizer/utils.hpp
                    // see https://en.cppreference.com/w/cpp/string/byte/isblank
                    if (cc0::isspace(ch)) // 读到的字符是空白字符（空格、换行、制表符等）
                        current_state = DFAState::INITIAL_STATE; // 保留当前状态为初始状态，此处直接break也是可以的
                    else if (!cc0::isprint(ch)) // control codes and backspace
                        invalid = true;
                    else if (cc0::isdigit(ch)){ // 读到的字符是数字
                        if(ch == '0'){ // 0需要特殊判断
                            current_char = nextChar();
                            if(!current_char.has_value()){ // 文件尾，正常进行
                                unreadLast();
                                current_state = DFAState::DECIMAL_INTEGER_STATE; // 切换到十进制整数的状态
                            }else{ // 有值，判断数字类型
                                auto ch2 = current_char.value();
                                if(ch2 == 'x'||ch2 == 'X'){ // 属于0x|0X开头，是十六进制
                                    unreadLast();
                                    current_state = DFAState::HEXADECIMAL_INTEGER_STATE; // 切换到十六进制整数的状态
                                }else{ //不是十六进制，还是要判断下是不是符合十进制
                                    if(isdigit(ch2)){ //是数字，不符合非0开头
                                        // 这里会因为神奇的错误导致pos为0，暂时无法解决
                                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongNum));
                                    }else{
                                        unreadLast(); //回退，捕获0
                                        current_state = DFAState::DECIMAL_INTEGER_STATE; // 切换到十进制整数的状态
                                    }
                                }
                            }
                        }
                        else current_state = DFAState::DECIMAL_INTEGER_STATE; // 切换到十进制整数的状态
                    }
                    else if (cc0::isalpha(ch)) // 读到的字符是英文字母
                        current_state = DFAState::IDENTIFIER_STATE; // 切换到标识符的状态
                    else {
                        // 其他的符号
                        switch (ch) {
                            // 比较符号
                            case '<':
                                current_state = DFAState::LESS_SIGN_STATE;
                                break;
                            case '>':
                                current_state = DFAState::MORE_SIGN_STATE;
                                break;
                            case '!':
                                current_state = DFAState::POINT_SIGN_STATE;
                                break;
                            case '=':
                                current_state = DFAState::EQUAL_SIGN_STATE;
                                break;
                                // 运算富奥
                            case '+':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::PLUS_SIGN, '+', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case '-':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::MINUS_SIGN, '-', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case '*':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::MULTIPLICATION_SIGN, '*', pos,currentPos()),
                                        std::optional<CompilationError>());
                            case '/':
                                current_state = DFAState::SLASH_SIGN_STATE;
                                break;
                                // 其他符号
                                // 在引号外有反斜杠也属于错误行为
                            case '\'':
                                current_state = DFAState::CHAR_STATE;
                                break;
                            case '\"':
                                current_state = DFAState::STRING_STATE;
                                break;
                            case ',':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::COMMA, ',', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case ';':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::SEMICOLON, ';', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case ':':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::COLON, ':', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case '(':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::LEFT_PARENTHESIS, '(', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case ')':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::RIGHT_PARENTHESIS, ')', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case '{':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::LEFT_BRACE, '{', pos, currentPos()),
                                        std::optional<CompilationError>());
                            case '}':
                                return std::make_pair(
                                        std::make_optional<Token>(TokenType::RIGHT_BRACE, '}', pos, currentPos()),
                                        std::optional<CompilationError>());

                                // 不接受的字符导致的不合法的状态
                            default:
                                invalid = true;
                                break;
                        }
                    }
                    // 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
                    if (current_state != DFAState::INITIAL_STATE)
                        pos = previousPos(); // 记录该字符的的位置为token的开始位置
                    // 读到了不合法的字符
                    if (invalid) {
                        // 回退这个字符
                        unreadLast();
                        // 返回编译错误：非法的输入
                        return std::make_pair(std::optional<Token>(),
                                              std::make_optional<CompilationError>(pos, ErrorCode::ErrInvalidInput));
                    }
                    // 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
                    if (current_state != DFAState::INITIAL_STATE) // ignore white spaces
                        ss << ch; // 存储读到的字符
                    break;
                }

                    // 当前状态是十进制整数
                case DECIMAL_INTEGER_STATE: {
                    // 请填空：
                    // 如果当前已经读到了文件尾，则解析已经读到的字符串为整数
                    //     解析成功则返回无符号整数类型的token，否则返回编译错误
                    std::string temp;
                    // 读到文件尾，需要退回
                    if (!current_char.has_value()) {
                        // 注释的是无必要的，因为已经return，不必更新状态，也因此不必清空ss，再来的时候又是新定义的了
                        unreadLast();
                        ss >> temp;
                        //如果返回有值的optional需要make，无值可直接optional
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::DECIMAL_INTEGER, temp, pos, currentPos()),
                                std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    // 如果读到的字符是数字，则存储读到的字符
                    if (isdigit(ch)) {
                        ss << ch;
                        break;
                    }
                    // 如果读到的是字母，则存储读到的字符，并切换状态到标识符，用于报错
                    if (isalpha(ch)) {
                        ss << ch;
                        current_state = DFAState::IDENTIFIER_STATE;
                        break;
                    }
                    // 如果读到的字符不是上述情况之一，则回退读到的字符，并解析已经读到的字符串为整数
                    unreadLast();
                    ss >> temp;
                    return std::make_pair(
                            std::make_optional<Token>(TokenType::DECIMAL_INTEGER, temp, pos, currentPos()),
                            std::optional<CompilationError>());
                }
                // 当前状态是十六进制整数
                case HEXADECIMAL_INTEGER_STATE: {
                    std::string temp;
                    if (!current_char.has_value()) {
                        unreadLast();
                        ss >> temp;
                        // 判断temp的格式是否正确
                        temp = checkHexDigit(temp);
                        if(temp == "null"){
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongNum));
                        }else{
                            return std::make_pair(
                                    std::make_optional<Token>(TokenType::HEXADECIMAL_INTEGER, temp, pos, currentPos()),
                                    std::optional<CompilationError>());
                        }
                    }

                    auto ch = current_char.value();
                    // 十六进制正常输入
                    if (isHexDigit(ch)||ch == 'x'||ch == 'X') {
                        ss << ch;
                        break;
                    }
                    // 如果不是十六进制输入但是还是字母则需要去标识符
                    if (isalpha(ch)) {
                        ss << ch;
                        // 其实直接报错就行
                        current_state = DFAState::IDENTIFIER_STATE;
                        break;
                    }
                    // 如果读到的字符不是上述情况之一，则回退读到的字符，并解析已经读到的字符串为整数
                    unreadLast();
                    ss >> temp;
                    // 判断temp的格式是否正确
                    temp = checkHexDigit(temp);
                    if(temp == "null"){
                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongNum));
                    }else{
                        temp = changeHex(temp);
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::HEXADECIMAL_INTEGER, temp, pos, currentPos()),
                                std::optional<CompilationError>());
                    }
                }
                case IDENTIFIER_STATE: {
                    // 请填空：
                    // 如果当前已经读到了文件尾，则解析已经读到的字符串
                    //     如果解析结果是关键字，那么返回对应关键字的token，否则返回标识符的token
                    // 这里temp只是缓冲区的临时存储，因此所有值都会传过来
                    std::string temp;
                    if (!current_char.has_value()) {
                        unreadLast();
                        ss >> temp;
                        auto type = checkKeywords(temp);
                        return std::make_pair(std::make_optional<Token>(type, temp, pos, currentPos()),
                                              std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    // 如果读到的是字符或字母，则存储读到的字符
                    if (isalpha(ch)||isdigit(ch)) {
                        ss << ch;
                        break;
                    }
                    // 如果读到的字符不是上述情况之一，则回退读到的字符，并解析已经读到的字符串
                    unreadLast();
                    ss >> temp;
                    auto type = checkKeywords(temp);
                    return std::make_pair(std::make_optional<Token>(type, temp, pos, currentPos()),
                                          std::optional<CompilationError>());
                }
                //额外符号的状态
                case CHAR_STATE: {
                    string temp = "\'";
                    while(true){
                        if(current_char.has_value()){
                            auto ch = current_char.value();
                            if(ch == '\''){
                                temp += '\'';
                                return std::make_pair(std::make_optional<Token>(TT::CHAR, temp, pos, currentPos()),
                                                      std::optional<CompilationError>());
                            }
                            if(!isChar(ch)&&ch != '\"') {
                                return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongChar));
                            }
                            if(ch == '\\'){
                                temp += ch;
                                current_char = nextChar();
                                if(current_char.has_value()){
                                    temp += current_char.value();
                                }
                                else unreadLast();
                            }else temp += ch;
                            current_char = nextChar();
                        }else{
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongChar));
                        }
                    }
                }
                case STRING_STATE: {
                    string temp = "\"";
                    while(true){
                        if(current_char.has_value()){
                            auto ch = current_char.value();
                            if(ch == '\"'){
                                temp += '\"';
                                return std::make_pair(std::make_optional<Token>(TT::STRING, temp, pos, currentPos()),
                                                      std::optional<CompilationError>());
                            }
                            if(!isChar(ch)&&ch != '\''){
                                return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongString));
                            }
                            if(ch == '\\'){
                                temp += ch;
                                current_char = nextChar();
                                if(current_char.has_value()){
                                    temp += current_char.value();
                                }
                                else unreadLast();
                            }else temp += ch;
                            current_char = nextChar();
                        }else {
                            return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(pos,ErrorCode::ErrWrongString));
                        }
                    }
                }
                // 除号与注释
                // 注释不遵循最长吞噬，单行遇到回车就结束，多行遇到*/就结束，因此其他的遵循
                case SLASH_SIGN_STATE: {
                    if (!current_char.has_value()) {
                        unreadLast();
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::DIVISION_SIGN, '/', pos, currentPos()),
                                std::optional<CompilationError>());
                    }
                    auto ch = current_char.value();
                    // 两种注释的处理
                    // 单行注释直接读到回车或者没有值
                    if (ch == '/') {
                        current_char = nextChar();
                        while (current_char.has_value()) {
                            if (current_char.value() == '\n') {
                                break;
                            }
                            current_char = nextChar();
                        }
                        // 单行注释如果到达结尾是正常的
                        if (!current_char.has_value()) {
                            unreadLast();
                        }
                        // 防止/影响缓冲区
                        ss.clear();
                        ss.str("");
                        current_state = INITIAL_STATE;
                        break;
                    }
                    // 多行注释则寻找*/
                    if (ch == '*') {
                        current_char = nextChar();
                        while (current_char.has_value()) {
                            if (current_char.value() == '*') {
                                current_char = nextChar();
                                if (current_char.has_value() && current_char.value() == '/'){
                                    break;
                                }else{
                                    if(!current_char.has_value()){
                                        unreadLast();
                                        return std::make_pair(std::optional<Token>(),
                                                              std::make_optional<CompilationError>(pos, ErrorCode::ErrWrongComment));
                                    }else{
                                        // 如果正常有值则进行下去即可，并且此时current_char还是刚才的值
                                        continue;
                                    }
                                }
                            }
                            current_char = nextChar();
                        }
                        // 一定是非正常结束，因此需要报错，不是多行注释
                        if (!current_char.has_value()) {
                            unreadLast();
                            return std::make_pair(std::optional<Token>(),
                                                  std::make_optional<CompilationError>(pos, ErrorCode::ErrWrongComment));
                        }
                        // 防止/影响缓冲区
                        ss.clear();
                        ss.str("");
                        current_state = INITIAL_STATE;
                        break;
                    }
                    // 不是单行注释不是多行注释，因此是正常的除号
                    unreadLast();
                    return std::make_pair(
                            std::make_optional<Token>(TokenType::DIVISION_SIGN, '/', pos, currentPos()),
                            std::optional<CompilationError>());
                }
                case LESS_SIGN_STATE: {
                    if (current_char.has_value() && current_char.value() == '=') {
                        string temp = "<=";
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::LESS_EQUAL_SIGN, string("<="), pos, currentPos()),
                                std::optional<CompilationError>());
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::LESS_SIGN, '<', pos, currentPos()),
                                          std::optional<CompilationError>());
                }
                case MORE_SIGN_STATE: {
                    if (current_char.has_value() && current_char.value() == '=') {
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::MORE_EQUAL_SIGN, string(">="), pos, currentPos()),
                                std::optional<CompilationError>());
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::MORE_SIGN, '>', pos, currentPos()),
                                          std::optional<CompilationError>());
                }
                case POINT_SIGN_STATE: {
                    if (current_char.has_value() && current_char.value() == '=') {
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::NOT_EQUAL_SIGN, string("!="), pos, currentPos()),
                                std::optional<CompilationError>());
                    }
                    unreadLast();
                    return std::make_pair(std::optional<Token>(),
                                          std::make_optional<CompilationError>(pos, ErrorCode::ErrWrongSign));
                }
                case EQUAL_SIGN_STATE: {
                    if (current_char.has_value() && current_char.value() == '=') {
                        return std::make_pair(
                                std::make_optional<Token>(TokenType::VALUE_EQUAL_SIGN, string("=="), pos, currentPos()),
                                std::optional<CompilationError>());
                    }
                    unreadLast();
                    return std::make_pair(std::make_optional<Token>(TokenType::EQUAL_SIGN, '=', pos, currentPos()),
                                          std::optional<CompilationError>());
                }
                    // 预料之外的状态，如果执行到了这里，说明程序异常
                default:
                    DieAndPrint("unhandled state.");
                    break;
            }
        }
        // 预料之外的状态，如果执行到了这里，说明程序异常
        return std::make_pair(std::optional<Token>(), std::optional<CompilationError>());
    }

    //检查一些特殊的token
    std::optional<CompilationError> Tokenizer::checkToken(const Token &t) {
        switch (t.GetType()) {
            case IDENTIFIER: {
                auto val = t.GetValueString();
                // 这里其实就是助教实现好的判断标识符的数字开头的函数，如果这里没有写
                // 在标识符的状态的结束时，其实是需要判断temp的值的，但是已经实现，因此就没有必要了
                if (cc0::isdigit(val[0]))
                    return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                ErrorCode::ErrInvalidIdentifier);
                break;
            }
            case CHAR: {
                auto val = t.GetValueString();
                if(val.length() == 3){
                    if((!isChar(val[1])&&val[1]!='\"')||val[1] == '\\')
                        return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                    ErrorCode::ErrWrongChar);
                    break;
                }
                if(val.length() == 4){
                    if(val[1]!='\\')
                        return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                    ErrorCode::ErrWrongChar);
                    if(val[2]!='\\'&&val[2]!='\''&&val[2]!='\"'&&val[2]!='n'&&val[2]!='r'&&val[2]!='t')
                        return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                    ErrorCode::ErrWrongChar);
                    break;
                }
                if(val.length() == 6){
                    if(!(val[1]=='\\'&&(val[2]=='x'||val[2]=='X')))
                        return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                    ErrorCode::ErrWrongChar);
                    if(!(isHexDigit(val[3])||isdigit(val[3]))||!(isHexDigit(val[4])||isdigit(val[4])))
                        return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                    ErrorCode::ErrWrongChar);
                    break;
                }
                return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                            ErrorCode::ErrWrongChar);
            }
            case STRING: {
                auto val = t.GetValueString();
                unsigned long long i;
                for(i = 1;i<val.length()-1;i++){
                    if(val[i] == '\\'){
                        if(val.length()-i>2){
                            if(val[i+1]=='\\'||val[i+1]=='\''||val[i+1]=='\"'||val[i+1]=='n'||val[i+1]=='r'||val[i+1]=='t'){
                                i+=1;
                                continue;
                            }
                        }
                        if(val.length()-i<=4)
                            return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                        ErrorCode::ErrWrongString);
                        if(!(val[i+1]=='x'||val[i+1]=='X')||!(isHexDigit(val[i+2])||isdigit(val[i+2]))||!(isHexDigit(val[i+3])||isdigit(val[i+3])))
                            return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                        ErrorCode::ErrWrongString);
                        i+=3;
                    }else{
                        if(!isChar(val[i])&&val[i]!='\'')
                            return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second,
                                                                        ErrorCode::ErrWrongString);
                    }
                }
                break;
            }
            default:
                break;
        }
        return {};
    }

    void Tokenizer::readAll() {
        if (_initialized)
            return;
        for (std::string tp; std::getline(_rdr, tp);)
            _lines_buffer.emplace_back(std::move(tp + "\n"));
        _initialized = true;
        _ptr = std::make_pair<int64_t, int64_t>(0, 0);
        return;
    }

    // Note: We allow this function to return a postion which is out of bound according to the design like std::vector::end().
    std::pair<uint64_t, uint64_t> Tokenizer::nextPos() {
        if (_ptr.first >= _lines_buffer.size())
            DieAndPrint("advance after EOF");
        if (_ptr.second == _lines_buffer[_ptr.first].size() - 1)
            return std::make_pair(_ptr.first + 1, 0);
        else
            return std::make_pair(_ptr.first, _ptr.second + 1);
    }

    std::pair<uint64_t, uint64_t> Tokenizer::currentPos() {
        return _ptr;
    }

    std::pair<uint64_t, uint64_t> Tokenizer::previousPos() {
        if (_ptr.first == 0 && _ptr.second == 0)
            DieAndPrint("previous position from beginning");
        if (_ptr.second == 0)
            return std::make_pair(_ptr.first - 1, _lines_buffer[_ptr.first - 1].size() - 1);
        else
            return std::make_pair(_ptr.first, _ptr.second - 1);
    }

    std::optional<char> Tokenizer::nextChar() {
        if (isEOF())
            return {}; // EOF
        auto result = _lines_buffer[_ptr.first][_ptr.second];
        _ptr = nextPos();
        return result;
    }

    bool Tokenizer::isEOF() {
        return _ptr.first >= _lines_buffer.size();
    }

    // Note: Is it evil to unread a buffer?
    void Tokenizer::unreadLast() {
        _ptr = previousPos();
    }
}