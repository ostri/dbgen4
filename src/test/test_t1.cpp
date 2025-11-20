#include <iostream>
#include "t1.hpp"
using dbx::s_sql_1::p;
using dbx::s_sql_1::r;

int main()
{
  r res; // results (e.g. after select)
  p par; // parameters (e.g. for inset, update)
  std::cout << "result" << res.dump() << "\n";
  std::cout << "param" << par.dump() << "\n";
  return 0;
}