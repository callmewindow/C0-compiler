#pragma once

#include <cstdint>
#include <utility>

namespace cc0 {

	enum Operation {
	    NOP,
	    BIPUSH,
	    IPUSH,
	    POP,
	    POP2,
	    POPN,
	    DUP,
	    DUP2,
	    LOADC,
	    LOADA,
	    ILOAD,
	    ALOAD,
	    ISTORE,
	    ASTORE,
	    IADD,
	    ISUB,
	    IMUL,
	    IDIV,
	    INEG,
	    ICMP,
	    JMP,
	    JE,
	    JNE,
	    JL,
	    JGE,
	    JG,
	    JLE,
	    CALL,
	    RET,
	    IRET,
	    IPRINT,
	    CPRINT,
	    SPRINT,
	    PRINTL,
	    ISCAN,
	};
	
	class Instruction final {
	public:
		friend void swap(Instruction& lhs, Instruction& rhs);
	public:
	    // 两操作数
		Instruction(Operation opr, std::string x, std::string y){_opr = opr;_param1 = x;_param2 = y;}
		// 一操作数
        Instruction(Operation opr, std::string x){_opr = opr;_param1 = x;}
        // 无操作数
        Instruction(Operation opr){_opr = opr;}

		Instruction() : Instruction(Operation::NOP){}
		Instruction(const Instruction& i) { _opr = i._opr; _param1 = i._param1; _param2 = i._param2; }
		Instruction(Instruction&& i) :Instruction() { swap(*this, i); }
		Instruction& operator=(Instruction i) { swap(*this, i); return *this; }
		bool operator==(const Instruction& i) const { return _opr == i._opr && _param1 == i._param1 && _param2 == i._param2; }

		Operation GetOperation() const { return _opr; }
        std::string GetParam1() const { return _param1; }
        std::string GetParam2() const { return _param2; }
	private:
		Operation _opr;
		std::string _param1;
		std::string _param2;
	};

	inline void swap(Instruction& lhs, Instruction& rhs) {
		using std::swap;
		swap(lhs._opr, rhs._opr);
        swap(lhs._param1, rhs._param1);
        swap(lhs._param2, rhs._param2);
	}
}