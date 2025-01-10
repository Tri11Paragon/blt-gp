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

static const auto SEED_FUNC = [] { return std::random_device()(); };

blt::gp::gp_program program(SEED_FUNC); // NOLINT

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
    
    for (auto& tree : pop.for_each_tree())
    {
        auto value = tree.get_evaluation_value<float>();
        
        BLT_TRACE(value);
    }
    
    return 0;
}
