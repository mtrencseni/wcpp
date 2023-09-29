#include <algorithm>
#include <iostream>
#include <iterator>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <locale>
#include <vector>
#include <memory>
#include <set>
#include <map>
#include <cstring>
#include <sys/stat.h> // for optimized file size
#include "clargs.hpp"
#include "tabular.hpp"

//
// coreutils implementation:
// https://github.com/coreutils/coreutils/blob/master/src/wc.c
//

struct counter_t {
    void reset() { count = 0; active = true; }
    bool active = false;
    long count = 0;
};

typedef std::vector<counter_t*> counter_vector_t;
typedef std::vector<int> count_vector_t;

class byte_counter_t : public counter_t {
    public:
    void process_file(const std::string fname) {
        struct stat st;
        stat(fname.c_str(), &st);
        count = st.st_size;
    }
    void process_wchar(const wchar_t, unsigned num_bytes, bool) {
        count += num_bytes;
    }
};

thread_local byte_counter_t byte_counter;

class char_counter_t : public counter_t {
    public:
    void process_wchar(const wchar_t, unsigned num_bytes, bool error) {
        if (MB_CUR_MAX == 1) {
            count += num_bytes;
        } else {
            if (error)
                return;
            count++;
        }
    }
};

void process_stream(std::istream* is,
    unsigned (*process_block_func)(const char*, unsigned, bool)
) {
    const int block_read = 128*1024;
    char buf[block_read+sizeof(wchar_t)];
    int n_remaining = 0;
    while (is->peek() != EOF) {
        // there are bytes to be read
        is->read(buf + n_remaining, block_read);
        int n_read = is->gcount();
        int n_available = n_remaining + n_read;
        if (n_available < 1)
            return;
        n_remaining = process_block_func(
            buf, n_available, is->peek() == EOF);
        // assert(n_remaining >= 0);
        // assert(n_remaining <= sizeof(wchar_t));
        // keep unprocessed part
        if (n_remaining > 0) {
            std::memcpy(buf, buf + n_available - n_remaining, n_remaining);
        }
    }
}

thread_local char_counter_t char_counter;

// forward:
unsigned process_block_lines_only(const char* cp, unsigned n, bool eof);

class line_counter_t : public counter_t {
    public:
    void process_file(const std::string fname) {
        std::ifstream f(fname);
        if (f.is_open()) {
            process_stream(&f, &process_block_lines_only);
        }
    }
    void process_wchar(const wchar_t wc, unsigned num_bytes, bool error) {
        if (error)
            return;
        count += (wc == '\n');
    }
};

thread_local line_counter_t line_counter;

// implementation here, so it can increment line_counter.count 
unsigned process_block_lines_only(
    const char* cp, unsigned n, bool eof) {
    void* cur = static_cast<void*>(const_cast<char*>(cp));
    while (cur != nullptr) {
        auto remaining = n - ((char*)cur - cp);
        cur = memchr(static_cast<void*>(cur), '\n', remaining);
        if (cur != nullptr) {
            line_counter.count++;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
            cur++;
#pragma GCC diagnostic pop
        }
    }
    return 0;
}

class longest_line_counter_t : public counter_t {
    public:
    void process_wchar(const wchar_t wc, unsigned, bool error) {
        if (error)
            return;
        if (wc == '\t') {
            current_length += 8 - (current_length % 8);
        } else {
            int width = wcwidth(wc);
            if (wc != '\n' && iswprint(wc) && width > 0)
                current_length += width;
            if (current_length > count)
                count = current_length;
            if (wc == '\n' || wc == '\r' || wc == '\f')
                current_length = 0;
        }
    }
    
    private:
    long current_length = 0;
};

thread_local longest_line_counter_t longest_line_counter;

bool is_word_sep(const wchar_t wc) {
    if (wc == '\n' || wc == '\r' || wc == '\f' || wc == '\v' || wc == ' ') {
        return true;
    }
    return static_cast<bool>(std::iswspace(wc));
}

class word_counter_t : public counter_t {
    public:
    void process_wchar(const wchar_t wc, unsigned, bool error) {
        if (error)
            return;
        bool whitespace = is_word_sep(wc);
        bool printable = iswprint(wc);
        if (in_word && whitespace) {
            in_word = false;
        } else if (!in_word && printable && !whitespace) {
            in_word = true;
            count++;
        }
    }
    
    private:
    bool in_word = false;
};

thread_local word_counter_t word_counter;

static inline bool is_basic (char c)
{
  switch (c)
    {
    case '\t': case '\v': case '\f':
    case ' ': case '!': case '"': case '#': case '%':
    case '&': case '\'': case '(': case ')': case '*':
    case '+': case ',': case '-': case '.': case '/':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case ':': case ';': case '<': case '=': case '>':
    case '?':
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y':
    case 'Z':
    case '[': case '\\': case ']': case '^': case '_':
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y':
    case 'z': case '{': case '|': case '}': case '~':
      return 1;
    default:
      return 0;
    }
}

void process_wchar(const wchar_t wc, unsigned num_bytes, bool error) {
    if (byte_counter.active)
        byte_counter.process_wchar(wc, num_bytes, error);
    if (char_counter.active)
        char_counter.process_wchar(wc, num_bytes, error);
    if (line_counter.active)
        line_counter.process_wchar(wc, num_bytes, error);
    if (longest_line_counter.active)
        longest_line_counter.process_wchar(wc, num_bytes, error);
    if (word_counter.active)
        word_counter.process_wchar(wc, num_bytes, error);
}

unsigned process_block(
    const char* cp, unsigned n, bool eof) {
    long remaining = n;
    std::mbstate_t ps{};
    while (remaining > 0) {
        wchar_t wc;
        long num_bytes = 0;
        if (is_basic(*cp)) {
            wc = *cp;
            num_bytes = 1;
        } else {
            num_bytes = mbrtowc(&wc, cp, remaining, &ps);
        }
        bool error = false;
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wsign-compare"
        if (num_bytes == 0) {
            // encountered null character
            num_bytes = 1;
        } else if (num_bytes == static_cast<std::size_t>(-1)) {
            // encountered bad wide character
            num_bytes = 1;
            error = true;
        } else if (num_bytes == static_cast<std::size_t>(-2)) {
            // encountered incomplete wide character, get more bytes if possible
            if (!eof)
                return remaining;
            num_bytes = remaining;
            error = true;
        } else {
            // success, read a wchar_t
        }
        #pragma GCC diagnostic pop
        process_wchar(wc, num_bytes, error);
        cp += num_bytes;
        remaining -= num_bytes;
    }
    return remaining;
}

counter_vector_t counters_from_arguments(const cl_args::cl_args_t& cl_args) {
    counter_vector_t counters;
    if (cl_args.flags.size() == 0) {
        // these 3, in this order, are counted by gnu wc by default
        line_counter.reset();
        counters.push_back(&line_counter);
        word_counter.reset();
        counters.push_back(&word_counter);
        byte_counter.reset();
        counters.push_back(&byte_counter);
    } else {
        // this is the gnu order if arguments are specified
        if (cl_args.flags.count("-l") or cl_args.flags.count("--lines")) {
            line_counter.reset();
            counters.push_back(&line_counter);
        }
        if (cl_args.flags.count("-w") or cl_args.flags.count("--words")) {
            word_counter.reset();
            counters.push_back(&word_counter);
        }
        if (cl_args.flags.count("-m") or cl_args.flags.count("--chars")) {
            char_counter.reset();
            counters.push_back(&char_counter);
        }
        if (cl_args.flags.count("-c") or cl_args.flags.count("--bytes")) {
            byte_counter.reset();
            counters.push_back(&byte_counter);
        }
        if (cl_args.flags.count("-L") or cl_args.flags.count("--max-line-length")) {
            longest_line_counter.reset();
            counters.push_back(&longest_line_counter);
        }
    }
    return counters;
}

void read_files0_from(cl_args::cl_args_t& cl_args) {
    std::istream* is;
    std::ifstream f;
    if (cl_args.key_values["--files0-from"] == "-") {
        is = &std::cin;
    } else {
        auto filename = cl_args.key_values["--files0-from"];
        f.open(filename);
        if (!f.is_open()) {
            std::cerr << "cannot read from file: " << filename << std::endl;
            exit(1);
        }
        is = &f;
    }
    for (std::string filename; std::getline(*is, filename, '\0');)
        cl_args.filename_args.push_back(filename);
}

count_vector_t to_counts(const counter_vector_t& counters) {
    count_vector_t counts;
    std::for_each(counters.begin(), counters.end(),
            [&](auto& c) { counts.push_back(c->count); });
    return counts;
}

tabular::table_t process_stdin(const cl_args::cl_args_t& cl_args) {
    tabular::table_t table;
    auto counters = counters_from_arguments(cl_args);
    process_stream(&std::cin, &process_block);
    auto counts = to_counts(counters);
    table.push_back(tabular::tabulate(counts));
    return table;
}

count_vector_t process_file(const std::string fname, const cl_args::cl_args_t& cl_args) {
    auto counters = counters_from_arguments(cl_args);
    if (byte_counter.active) {
        byte_counter.process_file(fname);
        byte_counter.active = false;
    }
    if (line_counter.active) {
        line_counter.process_file(fname);
        line_counter.active = false;
    }
    auto count_left = char_counter.active || word_counter.active || longest_line_counter.active;
    if (count_left) {
        std::ifstream f(fname);
        if (f.is_open()) {
            process_stream(&f, &process_block);
        } else {
            std::cerr << "cannot read from file: " << fname << std::endl;
        }
    }
    auto counts = to_counts(counters);
    return counts;
}

tabular::table_t process_files(const cl_args::cl_args_t& cl_args) {
    tabular::table_t table;
    count_vector_t total_counters(cl_args.flags.size(), 0);
    std::for_each(cl_args.filename_args.begin(), cl_args.filename_args.end(),
    [&](auto& fname) {
        auto counts = process_file(fname, cl_args);
        table.push_back(tabular::tabulate(counts, fname));
        for (unsigned i = 0; i < total_counters.size(); i++)
            total_counters[i] += counts[i];
    });
    if (cl_args.filename_args.size() > 1)
        table.push_back(tabular::tabulate(total_counters, "total"));
    return table;
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    cl_args::cl_args_spec_t spec;
    spec.accepted_flags = {
        "-l", "--lines",
        "-w", "--words",
        "-m", "--chars",
        "-c", "--bytes",
        "-L", "--max-line-length",
              "--help",
              "--version"
    };
    spec.accepted_keys = {
        "--files0-from"
    };
    spec.version = "wc by Marton Trencseni (mtrencseni@gmail.com)";
    spec.help = "see wc --help";
    auto cl_args = cl_args::get_normalized_cl_args_after_checks(
        argc, argv, spec
    );
    if (cl_args.key_values.count("--files0-from"))
        read_files0_from(cl_args);
    tabular::table_t table;
    bool left_justify_last;
    if (cl_args.filename_args.size() == 0) {
        table = process_stdin(cl_args);
        left_justify_last = false;
    } else {
        table = process_files(cl_args);
        left_justify_last = true;
    }
    tabular::print_table(table, std::cout, left_justify_last);
}
