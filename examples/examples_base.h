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

#ifndef BLT_GP_EXAMPLESEXAMPLES_BASE_H
#define BLT_GP_EXAMPLESEXAMPLES_BASE_H

#include <blt/gp/program.h>

namespace blt::gp::example
{
    class example_base_t
    {
    public:
        template<typename SEED>
        example_base_t(SEED&& seed, const prog_config_t& config): program{std::forward<SEED>(seed), config}
        {
        }

        example_base_t& set_crossover_selection(selection_t& sel)
        {
            crossover_sel = &sel;
            return *this;
        }

        example_base_t& set_mutation_selection(selection_t& sel)
        {
            mutation_sel = &sel;
            return *this;
        }

        example_base_t& set_reproduction_selection(selection_t& sel)
        {
            reproduction_sel = &sel;
            return *this;
        }

        example_base_t& set_all_selections(selection_t& sel)
        {
            crossover_sel = &sel;
            mutation_sel = &sel;
            reproduction_sel = &sel;
            return *this;
        }

        [[nodiscard]] gp_program& get_program() { return program; }
        [[nodiscard]] const gp_program& get_program() const { return program; }

    protected:
        gp_program program;
        selection_t* crossover_sel = nullptr;
        selection_t* mutation_sel = nullptr;
        selection_t* reproduction_sel = nullptr;
        std::function<bool(const tree_t&, fitness_t&, size_t)> fitness_function_ref;
    };
}

#endif //BLT_GP_EXAMPLESEXAMPLES_BASE_H
