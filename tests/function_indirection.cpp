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
#include "./testing.h"

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
        BLT_TRACE("multiplication operator: {} * {}", a, b);
        return a * b;
    };

    auto gul = [](const float a, const float b)
    {
        return a - b + (a / b);
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

    auto randf = []()
    {
        return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    };

    auto fixed = []()
    {
        static float i = 0;
        return ++i;
    };

    auto one = []()
    {
        return 1.0f;
    };

    auto two = []()
    {
        return 2.0f;
    };

    auto three = []()
    {
        return 3.0f;
    };

    auto pi = []()
    {
        return static_cast<float>(M_PI);
    };

    using F = detail::function_wrapper_t<std::vector>;

    template <size_t S>
    BLT_ATTRIB_NO_INLINE void run(const std::array<F, S>& funcs, byte_stack_t<>& stack, const size_t index)
    {
        if (index >= funcs.size())
        {
            BLT_UNREACHABLE;
        }
        funcs[index](stack, stack);
    }

    const std::array funcs{F{lam}, F{neg}, F{mul}, F{gul}, F{nul}, F{pul}, F{npul}, F{bul}, F{ln}, F{sim}, F{com}, F{randf}, F{fixed}};
    constexpr size_t N = funcs.size();
}

static void print_stack(byte_stack_t<>& stack, std::optional<float> assert_value = std::nullopt)
{
    BLT_INFO("S -> {}", stack.size());
    auto i = 0;
    while (!stack.empty())
    {
        auto value = stack.pop<float>();
        BLT_INFO("\t{}: {}", ++i, value);
        TEST_ASSERT(value == assert_value.value_or(value));
    }
}

int main(int argc, char** argv)
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

    run(funcs, stack, 2);
    run(funcs, stack, 2);

    size_t i = 0;
    while (!stack.empty())
    {
        auto value = stack.pop<float>();
        BLT_INFO("Stack {}: {}", ++i, value);
        TEST_ASSERT(value == 128.f);
        BLT_INFO("Standalone stack test passed");
    }

    type_resolver_t t_resolver;
    function_resolver_t f_resolver{t_resolver};

    const auto rlam = f_resolver.push(lam);
    const auto rneg = f_resolver.push(neg);
    const auto rmul = f_resolver.push(mul);
    const auto rgul = f_resolver.push(gul);
    const auto rnul = f_resolver.push(nul);
    const auto rpul = f_resolver.push(pul);
    const auto rnpul = f_resolver.push(npul);
    const auto rbul = f_resolver.push(bul);
    const auto rln = f_resolver.push(ln);
    const auto rsim = f_resolver.push(sim);
    const auto rcom = f_resolver.push(com);
    const auto rrandf = f_resolver.push(randf, true);
    const auto rfixed = f_resolver.push(fixed, true);
    const auto rone = f_resolver.push(one);
    const auto rtwo = f_resolver.push(two);
    const auto rthree = f_resolver.push(three);
    const auto rpi = f_resolver.push(pi);

    auto tree = tree_builder_t{f_resolver}
    .root(rmul, [&](auto& n)
    {
        n.child(rpi);
        n.child(rthree);
    })
    .build();

    stack = tree.evaluate(f_resolver);
    print_stack(stack, 9.424777984619141);

    tree = tree_builder_t{f_resolver}
    .root(rmul, [&](auto& n)
    {
        n.child(rmul, [&](auto& n)
        {
            n.child(rpi);
            n.ephemeral(rfixed);
        });

        n.child(rmul, [&](auto& n)
        {
            n.ephemeral(rfixed);
            n.child(rtwo);
        });
    })
    .build();

    stack = tree.evaluate(f_resolver);
    print_stack(stack, 12.56637096405029);

    auto new_tree = tree.root().child(0).to_tree();

    stack = new_tree.evaluate(f_resolver);
    print_stack(stack, 4);

    generative_tree_builder_t builder{f_resolver};

    builder.begin(rcom);
    {
        builder.begin(rmul);
        {
            builder.leaf(rpi);
            builder.leaf(rthree);
        }
        builder.end();
    }
    builder.end();

    auto cos_tree = builder.build();
    stack = cos_tree.evaluate(f_resolver);
    print_stack(stack, -1);

    tree.replace(tree.root().child(1).child(0), cos_tree);
    stack = tree.evaluate(f_resolver);
    print_stack(stack, -12.56637096405029);

}
