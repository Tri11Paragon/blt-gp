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
#include <string_view>
#include <iostream>

static const auto SEED_FUNC = [] { return std::random_device()(); };

blt::gp::gp_program program(SEED_FUNC); // NOLINT

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

auto lit = blt::gp::operation_t([]() {
    //static std::uniform_real_distribution<float> dist(-32000, 32000);
//    static std::uniform_real_distribution<float> dist(0.0f, 10.0f);
    return program.get_random().get_float(0.0f, 10.0f);
}).set_ephemeral();

/**
 * This is a test using multiple types with blt::gp
 */
int main()
{
    blt::gp::operator_builder builder{};
    program.set_operations(builder.build(add, sub, mul, pro_div, op_if, eq_f, eq_b, lt, gt, op_and, op_or, op_xor, op_not, lit));
    
    blt::gp::ramped_half_initializer_t pop_init;
    
    auto pop = pop_init.generate(blt::gp::initializer_arguments{program, program.get_typesystem().get_type<float>().id(), 500, 3, 10});

//    for (auto& tree : pop.getIndividuals())
//    {
//        auto value = tree.get_evaluation_value<float>(nullptr);
//
//        BLT_TRACE(value);
//    }
    
    blt::gp::crossover_t crossover;
    
    auto& ind = pop.get_individuals();
    
    std::vector<float> pre;
    std::vector<float> pos;
    blt::size_t errors = 0;
    BLT_INFO("Pre-Crossover:");
    for (auto& tree : pop.get_individuals())
    {
        auto f = tree.tree.get_evaluation_value<float>();
        pre.push_back(f);
        BLT_TRACE(f);
    }
    
    BLT_INFO("Crossover:");
    blt::gp::population_t new_pop;
    while (new_pop.get_individuals().size() < pop.get_individuals().size())
    {
        auto& random = program.get_random();
        blt::size_t first = random.get_size_t(0ul, pop.get_individuals().size());
        blt::size_t second;
        do
        {
            second = random.get_size_t(0ul, pop.get_individuals().size());
        } while (second == first);
        
        blt::gp::tree_t child1{program};
        blt::gp::tree_t child2{program};
        // crossover function assumes that children have been copied from parents
        child1.copy_fast(ind[first].tree);
        child2.copy_fast(ind[second].tree);
        auto results = crossover.apply(program, ind[first].tree, ind[second].tree, child1, child2);
        if (results)
        {
//            bool print_literals = true;
//            bool pretty_print = false;
//            bool print_returns = false;
//            BLT_TRACE("Parent 1: %f", ind[0].get_evaluation_value<float>());
//            ind[0].print(program, std::cout, print_literals, pretty_print, print_returns);
//            BLT_TRACE("Parent 2: %f", ind[1].get_evaluation_value<float>());
//            ind[1].print(program, std::cout, print_literals, pretty_print, print_returns);
//            BLT_TRACE("------------");
//            BLT_TRACE("Child 1: %f", results->child1.get_evaluation_value<float>());
//            results->child1.print(program, std::cout, print_literals, pretty_print, print_returns);
//            BLT_TRACE("Child 2: %f", results->child2.get_evaluation_value<float>());
//            results->child2.print(program, std::cout, print_literals, pretty_print, print_returns);
            new_pop.get_individuals().emplace_back(std::move(child1));
            new_pop.get_individuals().emplace_back(std::move(child2));
        } else
        {
            BLT_DEBUG("Crossover Failed.");
            errors++;
            new_pop.get_individuals().push_back(ind[first]);
            new_pop.get_individuals().push_back(ind[second]);
        }
    }
    
    BLT_INFO("Post-Crossover:");
    for (auto& tree : new_pop.for_each_tree())
    {
        auto f = tree.get_evaluation_value<float>();
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
    BLT_INFO("Error times: %ld", errors);
    
    return 0;
}
