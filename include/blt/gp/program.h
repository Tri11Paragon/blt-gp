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

#ifndef BLT_GP_PROGRAM_H
#define BLT_GP_PROGRAM_H

#include <blt/gp/fwdecl.h>
#include <functional>
#include <blt/std/ranges.h>
#include <type_traits>

namespace blt::gp
{
    class identifier
    {
    };
    
    template<typename Return, typename... Args>
    class operation
    {
        public:
            using function_t = std::function<Return(Args...)>;
            
            operation(const operation& copy) = default;
            
            operation(operation&& move) = default;
            
            template<typename T>
            explicit operation(const T& functor)
            {
                if constexpr (std::is_same_v<T, function_t>)
                {
                    func = functor;
                } else
                {
                    func = [&functor](Args... args) {
                        return functor(args...);
                    };
                }
            }
            
            explicit operation(function_t&& functor): func(std::move(functor))
            {}
            
            inline Return operator()(Args... args)
            {
                return func(args...);
            }
            
            inline Return operator()(blt::span<void*> args)
            {
                
            }
        
        private:
            function_t func;
    };
    
    template<typename Return, typename Args>
    class operations
    {
    
    };
    
    
}

#endif //BLT_GP_PROGRAM_H
