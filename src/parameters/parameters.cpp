//
// Created by ostri on 2024/02/04 
//

#include "parameters.h"
namespace dbgen4 {
parameters::parameters()                               = default;
parameters::~parameters()                              = default;
parameters::parameters(const parameters& o)                    = default;
parameters::parameters(parameters&& o) noexcept                = default;
parameters& parameters::operator=(const parameters& /*o*/)     = default;
parameters& parameters::operator=(parameters&& /*o*/) noexcept = default;
} // dbgen4
