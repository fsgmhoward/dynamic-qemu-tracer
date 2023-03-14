@0x85156056ff3cc724;

struct Instruction {
	begin @0 : UInt64;
	length @1 : UInt8;
}

struct InstructionTable {
	instructions @0 : List(Instruction);
}
