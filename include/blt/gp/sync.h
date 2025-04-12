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

#ifndef BLT_GP_SYNC_H
#define BLT_GP_SYNC_H

#include <blt/std/types.h>
#include <blt/gp/fwdecl.h>

namespace blt::gp
{
    class sync_t
    {
    public:
        explicit sync_t(gp_program& program, fs::writer_t& writer);

        void trigger(u64 current_time) const;

        sync_t& with_timer(u64 seconds)
        {
            m_timer_seconds = seconds;
            return *this;
        }

        sync_t& every_generations(u64 generations)
        {
            m_generations = generations;
            return *this;
        }

        sync_t& overwrite_file_on_write()
        {
            m_reset_to_start_of_file = true;
            return *this;
        }

        sync_t& append_to_file_on_write()
        {
            m_reset_to_start_of_file = false;
            return *this;
        }

        /**
         * Save the state of the whole program instead of just the generation information.
         */
        sync_t& whole_program()
        {
            m_whole_program = true;
            return *this;
        }

        /**
         * Only save the current generation to disk.
         */
        sync_t& generation_only()
        {
            m_whole_program = false;
            return *this;
        }

        ~sync_t();

    private:
        gp_program* m_program;
        fs::writer_t* m_writer;
        std::optional<u64> m_timer_seconds;
        std::optional<u64> m_generations;
        bool m_reset_to_start_of_file = false;
        bool m_whole_program = false;
    };
}

#endif //BLT_GP_SYNC_H
