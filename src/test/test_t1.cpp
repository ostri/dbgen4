#include <iostream>
#include "t1.hpp"
using p = dbx::s_sql_1::p;
using r = dbx::s_sql_1::r;

int main()
{
  r res; // results (e.g. after select)
  p par; // parameters (e.g. for inset, update)
  std::cout << "result" << res.dump() << "\n";
  std::cout << "param" << par.dump() << "\n";
  par.set_par_1(10, 0); // NOLINT
  par.set_par_1_null(1);
  std::cout << "param" << par.dump(5) << "\n";
  return 0;
}