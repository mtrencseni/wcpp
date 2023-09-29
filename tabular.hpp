#include <vector>
#include <string>
#include <algorithm>

namespace tabular {

typedef std::vector< std::string > row_t;
typedef std::vector< row_t       > table_t;

row_t tabulate(const std::vector<int>& counts, std::string suffix = "") {
    row_t row;
    std::for_each(counts.begin(), counts.end(),
        [&](auto& count) {
            row.push_back(std::to_string(count));
        });
    if (suffix.length())
        row.push_back(suffix);
    return row;
}

unsigned long get_max_width(const table_t& table, bool skip_last=false) {
    unsigned long max_width = 0;
    std::for_each(table.begin(), table.end(),
        [&](auto& row) {
            auto last = row.end();
            if (skip_last)
                last--;
            std::for_each(row.begin(), last,
                [&](auto& s) {
                    max_width = std::max(max_width, s.length());
                });
        });
    return max_width;
}

void print_table(const table_t& table, std::ostream& os, bool left_justify_last=false) {
    if (table.size() == 1 && table[0].size() == 1) {
        // we're printing just one thing, like the output from `wc -l`
        os << table[0][0] << std::endl;
        return;
    }
    auto w = get_max_width(table, left_justify_last) + 1;
    std::for_each(table.begin(), table.end(),
        [&](auto&  row) {
            auto last = row.end();
            if (left_justify_last)
                last--;
            std::for_each(row.begin(), last,
                [&](auto& s) {
                    os << std::setfill(' ') << std::setw(w) << s;
                });
            if (left_justify_last)
                os << " " << *(--row.end());
            os << std::endl;
        });
}

} // namespace tabular
