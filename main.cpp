#include <iostream>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>


using Item = int;
using Transaction = std::vector<Item>;
using TransactionDB = std::vector<Transaction>;

using HeaderKey = std::pair<Item, int>;

struct FrequencyCmp {
    // sort key_pair<item, count> by greater<count>, less<item>
    bool operator()(const HeaderKey& lhs, const HeaderKey& rhs) const {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        } else {
            return lhs.first < rhs.first;
        }
    }
};

using HeaderTable = std::map<HeaderKey, void*, FrequencyCmp>;


/// <h1>Input</h1>

TransactionDB read_transaction_file(const std::string& in_fname) {
    TransactionDB transactions(1);
    std::ifstream fin(in_fname);
    fin >> std::noskipws;
    Item data;
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

class FPTree {
public:
    explicit FPTree(TransactionDB&& transactions, double ms) :
        transactions(std::move(transactions)), min_support(ms), frequent_items(), header_table() {
        build_fp_tree();
    }

    void build_fp_tree() {
        find_frequent_items();
        build_header_table();
        exclude_non_frequent_items();
    }

    void print_transaction_db() {
        for (const auto& transaction: transactions) {
            for (const auto& item: transaction) {
                std::cout << item << " ";
            }
            std::cout << std::endl;
        }
    }

    void print_header_table() {
        for (const auto& row: header_table) {
            std::cout << row.first.first << " : " << row.first.second << " -> " << row.second << std::endl;
        }
    }

private:
    void find_frequent_items();

    void build_header_table();

    void exclude_non_frequent_items();

    void construct_fp_tree() {}

    bool is_frequent(Item item) {
        return frequent_items.find(item) != frequent_items.end();
    }

    TransactionDB transactions;
    double min_support;
    std::map<Item, int> item_counter;
    std::unordered_set<Item> frequent_items;
    HeaderTable header_table;
};

void FPTree::find_frequent_items() {
    double min_support_count = min_support * transactions.size();
    for (const auto& transaction: transactions) {
        for (const auto& item: transaction) {
            item_counter[item]++;
        }
    }
    for (auto[item, count]: item_counter) {
        if (count >= min_support_count) {
            frequent_items.insert(item);
        }
    }
}

void FPTree::build_header_table() {
    for (const auto& item: item_counter) {
        if (is_frequent(item.first)) {
            header_table[item] = nullptr;
        }
    }
}

void FPTree::exclude_non_frequent_items() {
    // erase non-frequent items from transaction DB
    for (auto& transaction: transactions) {
        transaction.erase(
            std::remove_if(transaction.begin(), transaction.end(),
                           [this](Item& x) { return !is_frequent(x); }),
            transaction.end());
    }

    // remove empty transactions from DB
    transactions.erase(
        std::remove_if(transactions.begin(), transactions.end(),
                       [](Transaction& x) { return x.empty(); }),
        transactions.end());
}


int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "invalid argument!" << std::endl;
        std::exit(1);
    }

    double min_support = std::strtod(argv[1], nullptr);
    std::cout << min_support << std::endl;
    std::string in_filename = std::string(argv[2]);
    std::string out_filename = std::string(argv[3]);

    auto&& transactions = read_transaction_file(in_filename);
    FPTree fp_tree(std::move(transactions), min_support);
//    fp_tree.print_header_table();
//    fp_tree.print_transaction_db();


    return 0;
}
