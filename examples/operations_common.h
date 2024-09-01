#pragma once
/*
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

#ifndef BLT_GP_OPERATIONS_COMMON_H
#define BLT_GP_OPERATIONS_COMMON_H

#include <blt/gp/program.h>

blt::gp::operation_t add([](float a, float b) { return a + b; }, "add");
blt::gp::operation_t sub([](float a, float b) { return a - b; }, "sub");
blt::gp::operation_t mul([](float a, float b) { return a * b; }, "mul");
blt::gp::operation_t pro_div([](float a, float b) { return b == 0.0f ? 1.0f : a / b; }, "div");
blt::gp::operation_t op_sin([](float a) { return std::sin(a); }, "sin");
blt::gp::operation_t op_cos([](float a) { return std::cos(a); }, "cos");
blt::gp::operation_t op_exp([](float a) { return std::exp(a); }, "exp");
blt::gp::operation_t op_log([](float a) { return a == 0.0f ? 0.0f : std::log(a); }, "log");

#endif //BLT_GP_OPERATIONS_COMMON_H
