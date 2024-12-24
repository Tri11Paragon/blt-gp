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
#include <blt/gp/util/statistics.h>
#include <blt/format/format.h>
#include <numeric>

namespace blt::gp {

    std::string confusion_matrix_t::pretty_print(const std::string& table_name) const
    {
        string::TableFormatter formatter{table_name};
        formatter.addColumn("Predicted " + name_A);
        formatter.addColumn("Predicted " + name_B);
        formatter.addColumn("");

        string::TableRow row;
        row.rowValues.push_back(std::to_string(is_A_pred_A));
        row.rowValues.push_back(std::to_string(is_A_pred_B));
        row.rowValues.push_back("Actual" + name_A);
        formatter.addRow(row);

        string::TableRow row2;
        row2.rowValues.push_back(std::to_string(is_B_pred_A));
        row2.rowValues.push_back(std::to_string(is_B_pred_B));
        row2.rowValues.push_back("Actual" + name_B);
        formatter.addRow(row2);

        auto tbl = formatter.createTable(true, true);
        return std::accumulate(tbl.begin(), tbl.end(), std::string{}, [](const std::string& a, const std::string& b)
        {
            return a + "\n" + b;
        });
    }

}