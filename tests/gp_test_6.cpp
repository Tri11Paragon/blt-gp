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
#include <blt/gp/program.h>
#include <blt/gp/generators.h>
#include <blt/gp/tree.h>
#include <blt/std/logging.h>
#include <blt/gp/transformers.h>

static constexpr long SEED = 41912;


blt::gp::type_provider type_system;
blt::gp::gp_program program(type_system, SEED); // NOLINT

blt::gp::operation_t add([](float a, float b) { return a + b; }, "add"); // 0
blt::gp::operation_t sub([](float a, float b) { return a - b; }, "sub"); // 1
blt::gp::operation_t mul([](float a, float b) { return a * b; }, "mul"); // 2
blt::gp::operation_t pro_div([](float a, float b) { return b == 0 ? 0.0f : a / b; }, "div"); // 3

blt::gp::operation_t op_if([](bool b, float a, float c) { return b ? a : c; }, "if"); // 4
blt::gp::operation_t eq_f([](float a, float b) { return a == b; }, "eq_f"); // 5
blt::gp::operation_t eq_b([](bool a, bool b) { return a == b; }, "eq_b"); // 6
blt::gp::operation_t lt([](float a, float b) { return a < b; }, "lt"); // 7
blt::gp::operation_t gt([](float a, float b) { return a > b; }, "gt"); // 8
blt::gp::operation_t op_and([](bool a, bool b) { return a && b; }, "and"); // 9
blt::gp::operation_t op_or([](bool a, bool b) { return a || b; }, "or"); // 10
blt::gp::operation_t op_xor([](bool a, bool b) { return static_cast<bool>(a ^ b); }, "xor"); // 11
blt::gp::operation_t op_not([](bool b) { return !b; }, "not"); // 12

blt::gp::operation_t lit([]() { // 13
    //static std::uniform_real_distribution<float> dist(-32000, 32000);
//    static std::uniform_real_distribution<float> dist(0.0f, 10.0f);
    return program.get_random().get_float(0.0f, 10.0f);
}, "lit");

/**
 * This is a test using multiple types with blt::gp
 */
int main()
{
    type_system.register_type<float>();
    type_system.register_type<bool>();
    
    blt::gp::operator_builder builder{type_system};
    builder.add_operator(add);
    builder.add_operator(sub);
    builder.add_operator(mul);
    builder.add_operator(pro_div);
    
    builder.add_operator(op_if);
    builder.add_operator(eq_f);
    builder.add_operator(eq_b);
    builder.add_operator(lt);
    builder.add_operator(gt);
    builder.add_operator(op_and);
    builder.add_operator(op_or);
    builder.add_operator(op_xor);
    builder.add_operator(op_not);
    
    builder.add_operator(lit, true);
    
    program.set_operations(builder.build());
    
    blt::gp::ramped_half_initializer_t pop_init;
    
    auto pop = pop_init.generate(blt::gp::initializer_arguments{program, type_system.get_type<float>().id(), 500, 3, 10});
    
    blt::gp::population_t new_pop;
    blt::gp::mutation_t mutator;
    blt::gp::grow_generator_t generator;
    
    std::vector<float> pre;
    std::vector<float> pos;

    BLT_INFO("Pre-Mutation:");
    for (auto& tree : pop.for_each_tree())
    {
        auto f = tree.get_evaluation_value<float>(nullptr);
        pre.push_back(f);
        BLT_TRACE(f);
    }
    BLT_INFO("Mutation:");
    for (auto& tree : pop.for_each_tree())
    {
        new_pop.get_individuals().emplace_back(mutator.apply(program, tree));
    }
    BLT_INFO("Post-Mutation");
    for (auto& tree : new_pop.for_each_tree())
    {
        auto f = tree.get_evaluation_value<float>(nullptr);
        pos.push_back(f);
        BLT_TRACE(f);
    }

    BLT_INFO("Stats:");
    blt::size_t eq = 0;
    for (const auto& v : pos)
    {
        for (const auto m : pre)
        {
            if (v == m)
            {
                eq++;
                break;
            }
        }
    }
    BLT_INFO("Equal values: %ld", eq);
    
    return 0;
}
