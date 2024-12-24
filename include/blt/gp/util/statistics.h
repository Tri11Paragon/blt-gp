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

        [[nodiscard]] std::string pretty_print(const std::string& table_name = "Confusion Matrix") const;

    private:
        u64 is_A_pred_A = 0;
        u64 is_A_pred_B = 0;
        u64 is_B_pred_B = 0;
        u64 is_B_pred_A = 0;
        std::string name_A = "A";
        std::string name_B = "B";
    };

    struct classifier_results_t : public confusion_matrix_t
    {
    public:
        [[nodiscard]] u64 get_hits() const
        {
            return hits;
        }

        [[nodiscard]] u64 get_size() const
        {
            return size;
        }

        [[nodiscard]] double get_percent_hit() const
        {
            return static_cast<double>(hits) / static_cast<double>(hits + misses);
        }

        void hit()
        {
            ++hits;
        }

        void miss()
        {
            ++misses;
        }


    private:
        u64 hits = 0;
        u64 misses = 0;
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
            move.best_fitness = 0;
            move.worst_fitness = 0;
        }

        std::atomic<double> overall_fitness = 0;
        std::atomic<double> average_fitness = 0;
        std::atomic<double> best_fitness = 0;
        std::atomic<double> worst_fitness = 1;
        tracked_vector<double> normalized_fitness{};

        void clear()
        {
            overall_fitness = 0;
            average_fitness = 0;
            best_fitness = 0;
            worst_fitness = 0;
            normalized_fitness.clear();
        }
    };
}

#endif //BLT_GP_UTIL_STATISTICS_H
