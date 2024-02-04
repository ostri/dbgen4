//
// Created by ostri on 2024/02/04
//

#pragma once

class a
{
public:
  a();
  virtual ~a();
  a(const a& o);
  a(a&& o) noexcept;
  a&                operator=(const a& o);
  a&                operator=(a&& o) noexcept;
  [[nodiscard]] int g() const;
protected:
private:
  int _ = 3;
};