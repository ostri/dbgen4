
#include "appl.hpp"
int main(int argc, char** argv, char** env)
{
  dbgen4::appl app;

  return app.exec(argc, argv, env);
}
