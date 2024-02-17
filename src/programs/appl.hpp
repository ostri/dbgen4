#ifndef APPL_HPP
#define APPL_HPP

#include "log.hpp"
#include "parameters.hpp"
namespace dbgen4
{
  class appl : public log
  {
  public:
    appl();
    ~appl();
    appl(const appl&)            = delete;
    appl(appl&&)                 = delete;
    appl& operator=(const appl&) = delete;
    appl& operator=(appl&&)      = delete;
    int   exec(int argc, char** argv, char** env); /// execute application
  private:
    /// method logs raw command line
    void       raw_command_line(int argc, char** argv);
    parameters p_; /// comand line parameter structure
  };
};     // namespace dbgen4
#endif // APPL_HPP
