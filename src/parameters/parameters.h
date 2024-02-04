//
// Created by ostri on 2024/02/04 
//

#pragma once

namespace dbgen4 {
class parameters {
public:
parameters();
virtual ~parameters();
parameters(const parameters& o);
parameters(parameters&& o) noexcept;
parameters& operator=(const parameters& o);
parameters& operator=(parameters&& o) noexcept;
protected:
private:
};
} // dbgen4