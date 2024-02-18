#include "appl.hpp"
#include "../parser/parser.hpp"

namespace dbgen4
{
  appl::appl() = default;

  appl::~appl() { l->flush(); };

  int appl::exec(int argc, char** argv, char** env)
  {
    parser p;
    l->info("=========== Application initialized ===========");
    auto sts = p_.load_parameters(argc, argv, env);
    raw_command_line(argc, argv);
    p.load_grammar();

    const int cifra = 42;
    // The people are defined with brace initialization
    static json bad_person  = {{"age", cifra}};
    static json good_person = {{"name", "Albert"}, {"age", cifra}};

    for (const auto& person : {bad_person, good_person}) p.exec(person);

    l->info("Application exit code '{}'", sts);
    return sts;
  }

  void appl::raw_command_line(int argc, char** argv)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::vector<std::string> vec(argv, argv + argc);
    auto                     cmd_line = join(vec, " ");
    l->debug("Raw command line: {}", cmd_line);
  }


}; // namespace dbgen4