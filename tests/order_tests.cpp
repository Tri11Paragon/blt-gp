/*
 *  <Short Description>
 *  Copyright (C) 2024  Brett Terpstra
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

#include <blt/std/logging.h>
#include <blt/gp/tree.h>
#include <blt/gp/operations.h>
#include <blt/gp/generators.h>
#include <blt/gp/transformers.h>
#include <blt/gp/program.h>
#include <random>

const blt::u64 SEED = std::random_device()();
//const blt::u64 SEED = 3495535167;
blt::gp::random_t b_rand {SEED};

struct context
{
    blt::gp::gp_program* program;
};

struct large_256
{
    blt::u8 data[256];
};

struct large_2048
{
    blt::u8 data[2048];
};

// not actually 4096 but will fill the whole page (4096)
struct large_4096
{
    blt::u8 data[4096];
};

struct large_6123
{
    blt::u8 data[6123];
};

struct large_18290
{
    blt::u8 data[18290];
};

blt::gp::operation_t basic_sub([](float a, float b, bool choice) {
    if (choice)
    {
        BLT_TRACE("Choice Taken! a: %lf b: %lf", a, b);
        return b - a;
    }else
    {
        BLT_TRACE("Choice Not Taken! a: %lf b: %lf", a, b);
        return a - b;
    }
}, "sub");

auto basic_lit_f= blt::gp::operation_t([]() {
    return b_rand.choice() ? 5.0f : 10.0f;
}).set_ephemeral();

auto basic_lit_b = blt::gp::operation_t([]() {
    return false;
}).set_ephemeral();

void basic_test()
{
    blt::gp::gp_program program{SEED};
    
    blt::gp::operator_builder<context> builder{};
    
    program.set_operations(builder.build(basic_sub, basic_lit_f, basic_lit_b));
    
    blt::gp::grow_generator_t gen;
    blt::gp::generator_arguments args{program, program.get_typesystem().get_type<float>().id(), 1, 1};
    blt::gp::tree_t tree{program};
    gen.generate(tree, args);
    
    context ctx{&program};
    auto result = tree.get_evaluation_value<float>(ctx);
    BLT_TRACE(result);
    BLT_ASSERT(result == -5.0f || result == 5.0f || result == 0.0f);
    tree.print(program, std::cout, true, true);
}

int main()
{
    BLT_INFO("Starting with seed %ld", SEED);
    
    basic_test();
}