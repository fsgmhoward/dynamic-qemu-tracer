@0xea5a513cb1666fde;

struct AnalysisRst {
  instOffsets @0 : InstOffset;
  funcStarts @1 : FuncStart;
}

struct InstOffset {
  offset @0 : List(Int64);
}

struct FuncStart {
  func @0 : List(Int64);
}
