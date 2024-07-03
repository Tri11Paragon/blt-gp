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
blt::gp::gp_program program(type_system, std::mt19937_64{SEED}); // NOLINT

blt::gp::operation_t add([](float a, float b) { return a + b; });
blt::gp::operation_t sub([](float a, float b) { return a - b; });
blt::gp::operation_t mul([](float a, float b) { return a * b; });
blt::gp::operation_t pro_div([](float a, float b) { return b == 0 ? 0.0f : a / b; });

blt::gp::operation_t op_if([](bool b, float a, float c) { return b ? a : c; });
blt::gp::operation_t eq_f([](float a, float b) { return a == b; });
blt::gp::operation_t eq_b([](bool a, bool b) { return a == b; });
blt::gp::operation_t lt([](float a, float b) { return a < b; });
blt::gp::operation_t gt([](float a, float b) { return a > b; });
blt::gp::operation_t op_and([](bool a, bool b) { return a && b; });
blt::gp::operation_t op_or([](bool a, bool b) { return a || b; });
blt::gp::operation_t op_xor([](bool a, bool b) { return static_cast<bool>(a ^ b); });
blt::gp::operation_t op_not([](bool b) { return !b; });

blt::gp::operation_t lit([]() {
    //static std::uniform_real_distribution<float> dist(-32000, 32000);
    static std::uniform_real_distribution<float> dist(0.0f, 10.0f);
    return dist(program.get_random());
});

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

//    for (auto& tree : pop.getIndividuals())
//    {
//        auto value = tree.get_evaluation_value<float>(nullptr);
//
//        BLT_TRACE(value);
//    }
    
    blt::gp::crossover_t crossover;
    
    auto& ind = pop.getIndividuals();
    auto results = crossover.apply(program, ind[0], ind[1]);
    
    if (results.has_value())
    {
        BLT_TRACE("Parent 1: %f", ind[0].get_evaluation_value<float>(nullptr));
        BLT_TRACE("Parent 2: %f", ind[1].get_evaluation_value<float>(nullptr));
        BLT_TRACE("------------");
        BLT_TRACE("Child 1: %f", results->child1.get_evaluation_value<float>(nullptr));
        BLT_TRACE("Child 2: %f", results->child2.get_evaluation_value<float>(nullptr));
    } else
    {
        switch (results.error())
        {
            case blt::gp::crossover_t::error_t::NO_VALID_TYPE:
                BLT_ERROR("No valid type!");
                break;
            case blt::gp::crossover_t::error_t::TREE_TOO_SMALL:
                BLT_ERROR("Tree is too small!");
                break;
        }
    }
    
    
    return 0;
}
