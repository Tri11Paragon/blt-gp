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

#ifndef BLT_GP_UTIL_STATISTICS_H
#define BLT_GP_UTIL_STATISTICS_H

#include <blt/gp/util/trackers.h>
#include <blt/gp/allocator.h>
#include <blt/gp/fwdecl.h>

namespace blt::gp
{
    struct confusion_matrix_t
    {
    public:
        confusion_matrix_t() = default;

        confusion_matrix_t& is_A_predicted_A()
        {
            ++is_A_pred_A;
            return *this;
        }

        confusion_matrix_t& is_A_predicted_B()
        {
            ++is_A_pred_B;
            return *this;
        }

        confusion_matrix_t& is_B_predicted_A()
        {
            ++is_B_pred_A;
            return *this;
        }

        confusion_matrix_t& is_B_predicted_B()
        {
            ++is_B_pred_B;
            return *this;
        }

        confusion_matrix_t& set_name_a(const std::string& name_a)
        {
            name_A = name_a;
            return *this;
        }

        confusion_matrix_t& set_name_b(const std::string& name_b)
        {
            name_B = name_b;
            return *this;
        }

        [[nodiscard]] u64 get_is_a_pred_a() const
        {
            return is_A_pred_A;
        }

        [[nodiscard]] u64 get_is_a_pred_b() const
        {
            return is_A_pred_B;
        }

        [[nodiscard]] u64 get_is_b_pred_b() const
        {
            return is_B_pred_B;
        }

        [[nodiscard]] u64 get_is_b_pred_a() const
        {
            return is_B_pred_A;
        }

        [[nodiscard]] u64 get_hits() const
        {
            return is_A_pred_A + is_B_pred_B;
        }

        [[nodiscard]] u64 get_misses() const
        {
            return is_B_pred_A + is_A_pred_B;
        }

        [[nodiscard]] u64 get_total() const
        {
            return get_hits() + get_misses();
        }

        [[nodiscard]] double get_percent_hit() const
        {
            return static_cast<double>(get_hits()) / static_cast<double>(get_total());
        }

        confusion_matrix_t& operator+=(const confusion_matrix_t& op)
        {
            is_A_pred_A += op.is_A_pred_A;
            is_B_pred_A += op.is_B_pred_A;
            is_A_pred_B += op.is_A_pred_B;
            is_B_pred_B += op.is_B_pred_B;
            return *this;
        }

        confusion_matrix_t& operator/=(const u64 val)
        {
            is_A_pred_A /= val;
            is_B_pred_A /= val;
            is_A_pred_B /= val;
            is_B_pred_B /= val;
            return *this;
        }

        friend confusion_matrix_t operator+(const confusion_matrix_t& op1, const confusion_matrix_t& op2)
        {
            confusion_matrix_t result = op1;
            result += op2;
            return result;
        }

        friend confusion_matrix_t operator/(const confusion_matrix_t& op1, const u64 val)
        {
            confusion_matrix_t result = op1;
            result /= val;
            return result;
        }

        friend bool operator<(const confusion_matrix_t& a, const confusion_matrix_t& b)
        {
            return a.get_percent_hit() < b.get_percent_hit();
        }

        friend bool operator>(const confusion_matrix_t& a, const confusion_matrix_t& b)
        {
            return a.get_percent_hit() > b.get_percent_hit();
        }

        [[nodiscard]] std::string pretty_print(const std::string& table_name = "Confusion Matrix") const;

    private:
        u64 is_A_pred_A = 0;
        u64 is_A_pred_B = 0;
        u64 is_B_pred_B = 0;
        u64 is_B_pred_A = 0;
        std::string name_A = "A";
        std::string name_B = "B";
    };

    struct population_stats
    {
        population_stats() = default;

        population_stats(const population_stats& copy):
            overall_fitness(copy.overall_fitness.load()), average_fitness(copy.average_fitness.load()), best_fitness(copy.best_fitness.load()),
            worst_fitness(copy.worst_fitness.load())
        {
            normalized_fitness.reserve(copy.normalized_fitness.size());
            for (auto v : copy.normalized_fitness)
                normalized_fitness.push_back(v);
        }

        population_stats(population_stats&& move) noexcept:
            overall_fitness(move.overall_fitness.load()), average_fitness(move.average_fitness.load()), best_fitness(move.best_fitness.load()),
            worst_fitness(move.worst_fitness.load()), normalized_fitness(std::move(move.normalized_fitness))
        {
            move.overall_fitness = 0;
            move.average_fitness = 0;
            move.best_fitness = std::numeric_limits<double>::min();
            move.worst_fitness = std::numeric_limits<double>::max();
        }

        std::atomic<double> overall_fitness = 0;
        std::atomic<double> average_fitness = 0;
        std::atomic<double> best_fitness = std::numeric_limits<double>::min();
        std::atomic<double> worst_fitness = std::numeric_limits<double>::max();
        tracked_vector<double> normalized_fitness{};

        void clear()
        {
            overall_fitness = 0;
            average_fitness = 0;
            best_fitness = std::numeric_limits<double>::min();
            worst_fitness = std::numeric_limits<double>::max();
            normalized_fitness.clear();
        }

        friend bool operator==(const population_stats& a, const population_stats& b)
        {
            return a.overall_fitness.load(std::memory_order_relaxed) == b.overall_fitness.load(std::memory_order_relaxed) &&
                a.average_fitness.load(std::memory_order_relaxed) == b.average_fitness.load(std::memory_order_relaxed) &&
                a.best_fitness.load(std::memory_order_relaxed) == b.best_fitness.load(std::memory_order_relaxed) &&
                a.worst_fitness.load(std::memory_order_relaxed) == b.worst_fitness.load(std::memory_order_relaxed) &&
                a.normalized_fitness == b.normalized_fitness;
        }

        friend bool operator!=(const population_stats& a, const population_stats& b)
        {
            return !(a == b);
        }
    };
}

#endif //BLT_GP_UTIL_STATISTICS_H
