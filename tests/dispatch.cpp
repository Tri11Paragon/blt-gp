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
    auto func0 = [](float& o, float a, float b)
    {
        o = a * b - b;
    };

    auto func1 = [](float& o, float a, float b)
    {
        o = a + b;
    };

    auto func2 = [](float& o, float a, float b)
    {
        o = a - b;
    };

    auto func3 = [](float& o, float a, float b)
    {
        o = a * b;
    };

    auto func4 = [](float& o, float a, float b)
    {
        o = a / (b == 0 ? 1.0f : b);
    };

    auto func5 = [](float& o, float a, float b)
    {
        o = a + b / ((a * b) == 0 ? 1.0f : (a * b));
    };

    auto func6 = [](float& o, float a, float b)
    {
        o = a + a + b * b;
    };

    auto func7 = [](float& o, float a, float b)
    {
        o = std::sqrt(a * a + b * b);
    };

    auto func8 = [](float& o, float a, float b)
    {
        const float fa = a - b;
        o = std::sqrt(fa * fa);
    };

    auto func9 = [](float& o, float a, float b)
    {
        const float fa = a - b;
        o = std::cbrt(fa * fa * fa);
    };

    auto func10 = [](float& o, float a, float b)
    {
        const float fa = a + b;
        o = std::cbrt(fa * fa * fa);
    };

    auto func11 = [](float& o, float a, float b)
    {
        const float fa = std::sqrt(a * b);
        o = std::cbrt(fa * fa * fa);
    };
}

BLT_ATTRIB_NO_INLINE static void dispatch(const size_t index, float& o, float a, float b)
{
    static auto dispatch = make_dispatcher(func0).add(func1)
                    .add(func2).add(func3).add(func4)
                    .add(func5).add(func6).add(func7).add(func8).add(func9).add(func10).add(func11).build();
    dispatch.dispatch(index, o, a, b);
}

int main()
{
    for (float i = 0; i < 1000; i++)
    {
        float a = i * 4123, b = (i * 33);
        const auto func = blt::black_box_ret(static_cast<blt::u32>(i) % 12);
        float o = 0;
        dispatch(func, o, a, b);
        BLT_TRACE("Result: {} || Func {}", o, func);
    }
}
