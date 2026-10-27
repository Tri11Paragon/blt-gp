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

#include <blt/gp/tree.h>
#include <blt/std/random.h>
#include <blt/logging/logging.h>

using namespace blt::gp;

namespace
{
    auto lam = [](const float a, const float b, const float c)
    {
        return (a + b) / c;
    };

    auto neg = [](const float a)
    {
        return -a;
    };

    auto mul = [](const float a, const float b)
    {
        return a * b;
    };

    auto gul = [](const float a, const float b)
    {
        return a - b + (a /b);
    };

    auto nul = [](const float a, const float b)
    {
        return a * b / (a + b);
    };

    auto pul = [](const float a)
    {
        return a * a;
    };

    auto npul = [](const float a)
    {
        return -a * a;
    };

    auto bul = [](const float a, const float b, const float c)
    {
        return (a + b + c) / (a * b * c);
    };

    auto ln = [](const float a)
    {
        return std::log(a);
    };

    auto sim = [](const float a)
    {
        return std::sin(a);
    };

    auto com = [](const float a)
    {
        return std::cos(a);
    };

    using F = detail::function_wrapper_t<std::vector>;

    template<size_t S>
    BLT_ATTRIB_NO_INLINE void run(const std::array<F, S>& funcs, byte_stack_t<>& stack, const size_t index)
    {
        if (index >= funcs.size())
        {
            BLT_UNREACHABLE;
        }
        funcs[index](stack, stack);
    }

    const std::array funcs{F{lam}, F{neg}, F{mul}, F{gul}, F{nul}, F{pul}, F{npul}, F{bul}, F{ln}, F{sim}, F{com}};
}

int main(const int argc, const char** argv)
{
    if (argc < 2)
    {
        BLT_INFO("Usage: ./program <count>");
        return -1;
    }

    const auto count = std::stoul(argv[1]);
    const auto seed = count * 213043429;
    blt::random::random_t rng{seed};
    std::vector<size_t> program_indices;

    for (size_t i = 0; i < count; ++i)
        program_indices.push_back(rng.get_size_t(0, funcs.size()));

    byte_stack_t stack;
    stack.push(1.0f);
    stack.push(32.0f);
    stack.push(4.0f);

    run(funcs, stack, seed);

    BLT_INFO("Stack A: {}", stack.pop<float>());
    BLT_INFO("Stack B: {}", stack.pop<float>());
    BLT_INFO("Stack C: {}", stack.pop<float>());
}