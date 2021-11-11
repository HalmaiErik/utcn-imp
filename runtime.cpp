// This file is part of the IMP project.

#include <iostream>

#include "runtime.h"
#include "interp.h"



// -----------------------------------------------------------------------------
static void PrintInt(Interp &interp)
{
  auto v = interp.PeekInt();
  std::cout << v;
  interp.Push<int64_t>(v);
}

// -----------------------------------------------------------------------------
static void PrintNewline(Interp &interp)
{
  std::cout << std::endl;
  interp.Push<int64_t>(0);
}


// -----------------------------------------------------------------------------
static void ReadInt(Interp &interp)
{
  int64_t val;
  std::cin >> val;
  interp.Push<int64_t>(val);
}

// -----------------------------------------------------------------------------
std::map<std::string, RuntimeFn> kRuntimeFns = {
  { "print_int", PrintInt },
  { "read_int", ReadInt },
  { "print_newline", PrintNewline },
};
