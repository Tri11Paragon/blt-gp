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

#ifndef BLT_GP_TESTING_H
#define BLT_GP_TESTING_H

#define TEST_ASSERT(cond)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "FAILED assertion: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1);                                                                       \
        }                                                                                       \
    } while (false)

#define TEST_ASSERT_MSG(cond, msg)                                                              \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "FAILED assertion: " #cond " (" << (msg) << ") at "                   \
                      << __FILE__ << ":" << __LINE__ << std::endl;                             \
            std::exit(1);                                                                       \
        }                                                                                       \
    } while (false)

#endif //BLT_GP_TESTING_H
