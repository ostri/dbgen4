#include <iostream>
#include "t1.hpp"
using dbx::s_sql_1::column;
using dbx::s_sql_1::param;

int main()
{
  column col;
  param  par;
  std::cout << "column" << col.dump() << "\n";
  std::cout << "par" << par.dump() << "\n";
  return 0;
}