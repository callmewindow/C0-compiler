#include "argparse.hpp"
#include "fmt/core.h"

#include "./vm.h"
#include "util/print.hpp"

#include "tokenizer/tokenizer.h"
#include "analyser/analyser.h"
#include "fmts.hpp"
#include "error/error.h"

#include <iostream>


std::vector<cc0::Token> _tokenize(std::istream& input) {
	cc0::Tokenizer tkz(input);
	auto p = tkz.AllTokens();
	if (p.second.has_value()) {
        fmt::print(stderr, "Tokenization error: {}\n", p.second.value());
        exit(2);
	}
	return p.first;
}

void Tokenize(std::istream& input, std::ostream& output) {
	auto v = _tokenize(input);
	for (auto& it : v)
		output << fmt::format("{}\n", it);
	return;
}

void Analyse(std::istream& input, std::ostream& output){
	auto tks = _tokenize(input);
	cc0::Analyser analyser(tks);
	auto p = analyser.Analyse();
	if (p.second.has_value()) {
	    // 语法的具体报错均在analysis.cpp中直接输出
        fmt::print(stderr, "{}\n", p.second.value());
		exit(2);
	}
    auto constList = p.first.constList;
    auto globalCode = p.first.globalCode;
    auto funcList = p.first.funcList;
    unsigned long long i,j;
    // 常量表
    output << ".constants:\n";
    for(i = 0;i<constList.size();i++){
        output<<"    "<<i<<' '<<constList[i].type<<' '<<constList[i].value<<'\n';
    }
    // 启动代码
    output << ".start:\n";
    for(i = 0;i<globalCode.size();i++){
        output<<"    "<<i<<' '<<fmt::format("{}", globalCode[i])<<'\n';
    }
    // 函数定义
    output << ".functions:\n";
    for(i = 0;i<funcList.size();i++){
        output<<"    "<<i<<' '<<funcList[i].constOffset<<' '<<funcList[i].paramNum<<" 0\n";
    }
    // 各个函数的代码
    for(i = 0;i<funcList.size();i++){
        output << ".F" << i << ":\n";
        for(j = 0;j<funcList[i].localCode.size();j++){
            output<<"    "<<j<<' '<<fmt::format("{}", funcList[i].localCode[j])<<'\n';
        }
    }
	return ;
}

void assemble_text(std::ifstream* in, std::ofstream* out, bool run = false) {
    try {
        File f = File::parse_file_text(*in);
        // f.output_text(std::cout);
        f.output_binary(*out);
        if (run) {
            auto avm = std::move(vm::VM::make_vm(f));
            avm->start();
        }
    }
    catch (const std::exception& e) {
        println(std::cerr, e.what());
    }
}

int main(int argc, char** argv) {
    // 选择的扩展有：注释、字面量、循环跳转语句、switch

	argparse::ArgumentParser program("cc0");
	program.add_argument("input")
		.help("speicify the file to be compiled.");

	program.add_argument("-t")
		.default_value(false)
		.implicit_value(true)
		.help("perform tokenization for the input file.");
//
//	program.add_argument("-l")
//		.default_value(false)
//		.implicit_value(true)
//		.help("perform syntactic analysis for the input file.");

    program.add_argument("-s")
            .default_value(false)
            .implicit_value(true)
            .help("translate input file to text assembly file");

    program.add_argument("-c")
            .default_value(false)
            .implicit_value(true)
            .help("translate input file to binary target file");

	program.add_argument("-o", "file")
		.required()
		.default_value(std::string("-"))
		.help("output to specified file");

	try {
		program.parse_args(argc, argv);
	}
	catch (const std::runtime_error& err) {
		fmt::print(stderr, "{}\n\n", err.what());
		program.print_help();
		exit(2);
	}

	auto input_file = program.get<std::string>("input");
	auto output_file = program.get<std::string>("file");
	std::istream* input;
	std::ostream* output;
	std::ifstream inf;
	std::ofstream outf;
	if (input_file != "-") {
		inf.open(input_file,std::ios::binary | std::ios::in);
		if (!inf) {
			fmt::print(stderr, "Fail to open {} for reading.\n", input_file);
			exit(2);
		}
		input = &inf;
	}
	else
		input = &std::cin;

	if (output_file != "-") {
		outf.open(output_file,std::ios::binary | std::ios::out | std::ios::trunc);
		if (!outf) {
			fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
			exit(2);
		}
		output = &outf;
	}
	else{
	    // 没有的话就输出到out文件中
        outf.open("out",std::ios::binary | std::ios::out | std::ios::trunc);
        if (!outf) {
            fmt::print(stderr, "Fail to open {} for writing.\n", output_file);
            exit(2);
        }
        output = &outf;
	}

	int num = 0;
    if(program["-t"] == true) num++;
    if(program["-s"] == true) num++;
    if(program["-c"] == true) num++;

	if (num > 1) {
	    // 多个选项
		fmt::print(stderr, "You can only perform one analysis method at one time.");
		exit(2);
	}

	// 判断三种方式
    if (program["-t"] == true) {
        Tokenize(*input, *output);
    }else if (program["-s"] == true) {
        Analyse(*input, *output);
	}else if (program["-c"] == true) {
        std::ofstream outTemp;
        outTemp.open("./wyxlj.txt",std::ios::binary | std::ios::out | std::ios::trunc);
        auto outputTemp = &outTemp;
		Analyse(*input, *outputTemp);
		outTemp.close();
		std::ifstream infTemp;
        infTemp.open("./wyxlj.txt",std::ios::binary | std::ios::in);
        if (!infTemp) {
            exit(2);
        }
        std::ifstream* inputTemp;
        inputTemp = &infTemp;
        if (output_file == "-" || input_file == output_file) {
            output_file = input_file + ".out";
        }
        assemble_text(inputTemp, dynamic_cast<std::ofstream*>(output), false);
        infTemp.close();
        remove("./wyxlj.txt");
	}else {
		fmt::print(stderr, "You must choose one analysis method.");
		exit(2);
	}

	return 0;
}