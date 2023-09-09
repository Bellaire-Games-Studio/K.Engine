#pragma once

#include <stdio.h>
#include <memory>
#include <iostream>
#include <algorithm>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <GLFW/glfw3.h>
#endif
#include <Application.hpp>




#define BindEvent(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
