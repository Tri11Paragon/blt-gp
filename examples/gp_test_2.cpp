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


static constexpr long SEED = 41912;

blt::gp::type_system type_system;
blt::gp::gp_program program(type_system, std::mt19937_64{SEED}); // NOLINT

blt::gp::operation_t add([](float a, float b) {
    BLT_TRACE("a: %f + b: %f = %f", a, b, a + b);
    return a + b;
});
blt::gp::operation_t sub([](float a, float b) {
    BLT_TRACE("a: %f - b: %f = %f", a, b, a - b);
    return a - b; });
blt::gp::operation_t mul([](float a, float b) {
    BLT_TRACE("a: %f * b: %f = %f", a, b, a * b);
    return a * b; });
blt::gp::operation_t pro_div([](float a, float b) {
    BLT_TRACE("a: %f / b: %f = %f", a, b, (b == 0 ? 0.0f : a / b));
    return b == 0 ? 0.0f : a / b; });
blt::gp::operation_t lit([]() {
    //static std::uniform_real_distribution<float> dist(-32000, 32000);
    static std::uniform_real_distribution<float> dist(0.0f, 10.0f);
    return dist(program.get_random());
});

/**
 * This is a test using a type with blt::gp
 */
int main()
{
    type_system.register_type<float>();
    
    blt::gp::gp_operations silly{type_system};
    silly.add_operator(add);
    silly.add_operator(sub);
    silly.add_operator(mul);
    silly.add_operator(pro_div);
    silly.add_operator(lit, true);
    
    program.set_operations(std::move(silly));
    
    blt::gp::grow_generator_t grow;
    auto tree = grow.generate(blt::gp::generator_arguments{program, type_system.get_type<float>().id(), 3, 7});
    
    auto value = tree.get_evaluation_value<float>(nullptr);
    
    BLT_TRACE(value);
    
    return 0;
}