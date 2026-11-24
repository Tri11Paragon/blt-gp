/*
 *  <Short Description>
 *  Copyright (C) 2026  Brett Terpstra
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <cmath>
#include <iostream>
#include <blt/gp/dispatch.h>
#include <blt/logging/logging.h>
#include "testing.h"

using namespace blt::gp;

namespace
{
    auto func0 = [](const float a, const float b)
    {
        return a * b - b;
    };

    auto func1 = [](const float a, const float b)
    {
        return a + b;
    };

    auto func2 = [](const float a, const float b)
    {
        return a - b;
    };

    auto func3 = [](const float a, const float b)
    {
        return a * b;
    };

    auto func4 = [](const float a, const float b)
    {
        return a / (b == 0 ? 1.0f : b);
    };

    auto func5 = [](const float a, const float b)
    {
        return a + b / ((a * b) == 0 ? 1.0f : (a * b));
    };

    auto func6 = [](const float a, const float b)
    {
        return a + a + b * b;
    };

    auto func7 = [](const float a, const float b)
    {
        return std::sqrt(a * a + b * b);
    };

    auto func8 = [](const float a, const float b)
    {
        const float fa = a - b;
        return std::sqrt(fa * fa);
    };

    auto func9 = [](const float a, const float b)
    {
        const float fa = a - b;
        return std::cbrt(fa * fa * fa);
    };

    auto func10 = [](const float a, const float b)
    {
        const float fa = a + b;
        return std::cbrt(fa * fa * fa);
    };

    auto func11 = [](const float a, const float b)
    {
        const float fa = std::sqrt(a * b);
        return std::cbrt(fa * fa * fa);
    };
}

int main()
{
    static auto dispatch = make_dispatcher(func0).add(func1).add(func2).add(func3)
                                                 .add(func4).add(func5).add(func6).add(func7)
                                                 .add(func8).add(func9).add(func10).add(func11).add(
                                                     [](const float a, const float b)
                                                     {
                                                         return (a * b) / (a * a + b * b);
                                                     }).build();
    for (float i = 0; i < 1000; i++)
    {
        const float a = i * 4123, b = (i * 33);
        const auto func = blt::black_box_ret(static_cast<blt::u32>(i) % decltype(dispatch)::Size);
        float o = dispatch.dispatch(func, a, b);
        BLT_TRACE("Result: {} || Func {}", o, func);
    }
}
