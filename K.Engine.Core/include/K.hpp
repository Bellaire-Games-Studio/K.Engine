#pragma once

#include <stdio.h>
#include <memory>
#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <Platform/GL.hpp>
#include <Application.hpp>




#define BindEvent(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
