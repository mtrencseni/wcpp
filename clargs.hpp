#include <string>
#include <vector>
#include <set>

namespace cl_args {

struct cl_args_spec_t {
    std::set<std::string> accepted_flags;
    std::set<std::string> accepted_keys;
    std::string version;
    std::string help;
};

struct cl_args_t {
    std::set<std::string> flags;
    std::map<std::string, std::string> key_values;
    std::vector<std::string> filename_args;
};

cl_args_t get_cl_args(int argc, char** argv) {
    cl_args_t cl_args;
    for(auto i = 1; i < argc; i++) {
        auto arg = std::string(argv[i]);
        if (arg[0] == '-') {
            if (cl_args.filename_args.size() > 0) {
                std::cerr << "error: switches must precede filename arguments"
                << std::endl;
            }
            auto eqi = arg.find('=');
            if (eqi == std::string::npos) {
                // no '=' in arg
                cl_args.flags.insert(arg);
            } else {
                auto k = arg.substr(0, eqi);
                auto v = arg.substr(eqi+1, arg.length());
                cl_args.key_values[k] = v;
            }
        } else {
            cl_args.filename_args.push_back(arg); 
        }
    }
    return cl_args;
}

void normalize_flags(cl_args_t& cl_args) {
    // convert eg. -lm to -l -m
    std::set<std::string> new_flags;
    for (auto f : cl_args.flags) {
        if (f[0] == '-' && f[1] != '-' && f.length() > 2) {
            for (unsigned i = 1; i < f.length(); i++) {
                std::string new_f = std::string("-") + f[i];
                new_flags.insert(new_f);
            }
        } else {
            new_flags.insert(f);
        }
    }
    cl_args.flags = new_flags;
}

void check_args_maybe_exit(
    const cl_args_spec_t& spec,
    const cl_args_t& cl_args)
{
    for (auto f : cl_args.flags) {
        if (!spec.accepted_flags.count(f)) {
            std::cerr << "invalid option: " << f << std::endl;
            exit(1);
        }
    }
    for (auto kv : cl_args.key_values) {
        if (spec.accepted_keys.count(kv.first) == 0) {
            std::cerr << "invalid option: " << kv.first << std::endl;
            exit(1);
        }
    }
    if (cl_args.flags.count("--help") > 0) {
        std::cout << spec.help << std::endl;
        exit(0);
    }
    if (cl_args.flags.count("--version") > 0) {
        std::cout << spec.version << std::endl;
        exit(0);
    }
}

cl_args_t get_normalized_cl_args_after_checks(
    int argc, char** argv,
    const cl_args_spec_t& spec)
{
    auto cl_args = get_cl_args(argc, argv);
    cl_args::normalize_flags(cl_args);
    cl_args::check_args_maybe_exit(spec, cl_args);
    return cl_args;
}

} // namespace tabular
