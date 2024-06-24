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

#ifndef BLT_GP_FWDECL_H
#define BLT_GP_FWDECL_H


namespace blt::gp
{
    
    class gp_program;
    
    class type;
    
    class type_system;
    
    class tree_generator_t;
    
    class grow_generator_t;
    
    class full_generator_t;
    
    namespace detail
    {
        class operator_storage_test;
    }
    
}

#endif //BLT_GP_FWDECL_H
