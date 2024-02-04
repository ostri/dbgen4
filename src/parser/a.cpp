//
// Created by ostri on 2024/02/04
//

#include "a.h"
a::a()                               = default;
a::~a()                              = default;
a::a(const a& o)                     = default;
a::a(a&& o) noexcept                 = default;
a&  a::operator=(const a& /*o*/)     = default;
a&  a::operator=(a&& /*o*/) noexcept = default;
int a::g() const { return _; }