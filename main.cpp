#include <iostream>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <tuple>


using TransactionDB = std::vector<std::vector<int>>;

/// <h1>Input</h1>

TransactionDB read_transaction_file(const std::string &in_fname) {
    TransactionDB transactions(1);
    std::ifstream fin(in_fname);
    fin >> std::noskipws;
    int data;
    char delim;
    while (fin >> data >> delim) {
        transactions.back().push_back(data);

        if (delim == '\n') {
            transactions.emplace_back();
        }
    }
    if (transactions.back().empty()) {
        transactions.pop_back();
    }

    return transactions;
}

/// <h1>FP Tree</h1>

using HeaderRow = std::tuple<int, int, void *>;

class FPTree {
public:
    void build_fp_tree() {}

private:
    void _build_header_table(const std::string &in_fname, double min_supp);

    void _exclude_non_freq_elements() {}

    void _construct_fp_tree() {}


};

void FPTree::_build_header_table(const std::string &in_fname, double min_supp) {

}


int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "invalid argument!" << std::endl;
        std::exit(1);
    }

    double min_support = std::strtod(argv[1], nullptr);
    std::cout << min_support << std::endl;
    std::string in_filename = std::string(argv[2]);
    std::string out_filename = std::string(argv[3]);
    auto transactions = read_transaction_file(in_filename);

    for (const auto &transaction: transactions) {
        for (const auto &item: transaction) {
            std::cout << item << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
