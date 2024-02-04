#include <iostream>
#include <vector>
#include "dbgen4.hpp"
#include "CLI/App.hpp"
/// both must be there, otherwise we have linker error
#include "CLI/Formatter.hpp"
#include "CLI/Config.hpp"
#include "a.h"
#include "parameters.h"
int main(int argc, char** argv, char** /*env*/)
{
  auto               sts = 0;
  a                  x;
  dbgen4::parameters p;

  CLI::App app{"My program with options and unnamed parameters"};

  // Define options with clear descriptions
  std::string filename = "data.txt";
  app.add_option("-f,--filename", filename, "Name of the input file");

  // Unnamed parameters (strings)
  std::vector<std::string> str_arr;
  app.add_option("strings", str_arr, "A set of unnamed string parameters");

  CLI11_PARSE(app, argc, argv);

  // Access and use parsed options and parameters
  std::cout << "Using filename: " << filename << "\n";
  std::cout << "Unnamed strings: ";
  for (const std::string& str : str_arr) { std::cout << str << " "; }
  std::cout << '\n';

  std::cerr << "hello world\n" << x.g() << "\n";
  return sts;
}
