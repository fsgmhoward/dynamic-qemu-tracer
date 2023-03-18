@0xea5a513cb1666fde;

# Schema 1 is for compatibility with the original ground truth generator

struct AnalysisRst {
  instOffsets @0 : InstOffset;
  funcStarts @1 : FuncStart;
}

struct InstOffset {
  # Instructions are offsets after loading in memory (already added base address)
  offset @0 : List(Int64);
}

struct FuncStart {
  func @0 : List(Int64);
}

# Separate Struct to store additional information
struct CaptureResult {
    digest       @0 : Text;
    baseAddress  @1 : Int64;
    # Instructions are offsets before loading in memory (before adding base address)
    instructions @2 : List(Instruction);
}

struct Instruction {
    offset @0 : Int64;
    length @1 : Int8;
}
