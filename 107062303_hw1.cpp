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
#include <deque>
#include <chrono>
#include <pthread.h>


using Item = int;
using Transaction = std::vector<Item>;
using TransactionDB = std::vector<Transaction>;

using HeaderKey = std::pair<Item, int>;

struct FrequencyCmp {
    // sort key_pair<item, freq> by greater<freq>, less<item>
    bool operator()(const HeaderKey& lhs, const HeaderKey& rhs) const {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        } else {
            return lhs.first < rhs.first;
        }
    }
};

struct TreeNode;

using HeadTailPointer = std::pair<TreeNode*, TreeNode*>;
using HeaderTable = std::map<HeaderKey, HeadTailPointer, FrequencyCmp>;

using Pattern = std::set<Item>;

struct PatternCmp {
    bool operator()(const Pattern& lhs, const Pattern& rhs) const {
        if (lhs.size() != rhs.size()) {
            return lhs.size() < rhs.size();
        }
        return lhs < rhs;
    }
};

using Patterns = std::vector<Pattern>;
using FrequentPatterns = std::map<Pattern, long double, PatternCmp>;

std::ostream& operator<<(std::ostream& out, const Pattern& p);
std::ostream& operator<<(std::ostream& out, const Patterns& ps);
std::ostream& operator<<(std::ostream& out, const FrequentPatterns& fp);


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
    fin.close();

    return transactions;
}

/// <h1>Tree Node</h1>

struct TreeNode {
    Item item;
    int count{1};
    TreeNode* left_child{};
    TreeNode* right_sibling{};
    TreeNode* cross_link{};
    TreeNode* parent{};

    explicit TreeNode(Item x, TreeNode* p = nullptr) : item(x), parent(p) {}

    TreeNode(const TreeNode& other) : item(other.item), count(other.count) {}

    // Add new node(x) to child, or increment the count if the node(x) already exists.
    // Then return the affected node and whether the insertion is successful or not
    std::pair<TreeNode*, bool> add_item_to_child(Item x) {
        if (!left_child) {
            left_child = new TreeNode(x, this);
            return std::make_pair(left_child, true);
        }

        TreeNode* prev{};
        for (auto child = left_child; child; prev = child, child = child->right_sibling) {
            if (child->item == x) {
                child->count++;
                return std::make_pair(child, false);
            }
        }

        prev->right_sibling = new TreeNode(x, this);
        return std::make_pair(prev->right_sibling, true);
    }

    friend std::ostream& operator<<(std::ostream& out, const TreeNode& tr) {
        out << tr.item << " : " << tr.count;
        return out;
    }

    ~TreeNode() {
        delete left_child;
        delete right_sibling;
    }

};

/// <h1>Utils</h1>

void combinations_with(
    Item x, int k, Pattern& current_pattern,
    const Pattern::const_iterator& start, const Pattern::const_iterator& end,
    Patterns& patterns) {
    if (k == 0) {
        patterns.push_back(current_pattern);
        return;
    }

    for (auto curr = start; curr != end; ++curr) {
        if (*curr == x) {
            continue;
        }

        current_pattern.insert(*curr);
        auto next = curr;
        ++next;
        combinations_with(x, k - 1, current_pattern, next, end, patterns);
        current_pattern.erase(*curr);
    }
}

FrequentPatterns expand_all_combinations(Item x, const FrequentPatterns& paths) {
    FrequentPatterns fp;
    for (const auto& path: paths) {
        for (int k = 0; k <= path.first.size(); ++k) {
            Patterns patterns;
            Pattern pat{x};
            combinations_with(x, k, pat, path.first.cbegin(), path.first.cend(), patterns);
            for (const auto& pattern: patterns) {
                fp[pattern] += path.second;
            }
        }
    }

    return fp;
}

void erase_infrequent_patterns(FrequentPatterns& frequent_pattens, long double min_support_count) {
    for (auto it = frequent_pattens.begin(); it != frequent_pattens.end();) {
        if ((*it).second < min_support_count) {
            it = frequent_pattens.erase(it);
        } else {
            ++it;
        }
    }
}


/// <h1>FP Tree</h1>

class FPTree;

struct ThreadInfo {
    int t_id;
    int t_count;
    FPTree* tree;
    FrequentPatterns* result;
    pthread_mutex_t* mutex;
};

class FPTree {
public:
    explicit FPTree(TransactionDB&& tr, long double ms, long double msc, FPTree* p = nullptr) :
        transactions(std::move(tr)), min_support(ms), min_support_count(msc), parent(p) {
        build();
    }

    void build() {
        find_frequent_items();
        build_header_table();
        exclude_non_frequent_items();
        sort_transaction_items();
        construct_fp_tree();
    }

    FrequentPatterns mine_all(int t_id, int t_count) {
        FrequentPatterns result;
        std::vector<Item> freq_items(frequent_items.cbegin(), frequent_items.cend());

        for (int i = t_id; i < freq_items.size(); i += t_count) {
            result.merge(mine(freq_items[i]));
        }
        return result;
    }

    // mining will make some unrecoverable changes to the tree, so be sure that the tree is copied
    FrequentPatterns mine(Item x) {
        auto cfp_tree = construct_conditional_fp_tree(x);
        auto paths = cfp_tree.find_all_pattern_paths(x);
        auto frequent_pattens = expand_all_combinations(x, paths);
        erase_infrequent_patterns(frequent_pattens, min_support_count);

        return frequent_pattens;
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
            std::cout << row.first.first << " : " << row.first.second << " -> "
                      << row.second.first << " ... " << row.second.second << std::endl;
        }
    }

    void traverse_cross_links() {
        for (const auto& links: header_table) {
            for (TreeNode* curr = links.second.first; curr; curr = curr->cross_link) {
                std::cout << curr->item << " : " << curr->count << ", ";
            }
            std::cout << std::endl;
        }
    }

    ~FPTree() {
        delete root;
    }

    static void* thread_mine(void* tinfo) {
        auto info = static_cast<ThreadInfo*>(tinfo);
        auto patterns = info->tree->mine_all(info->t_id, info->t_count);
        pthread_mutex_lock(info->mutex);
        info->result->merge(patterns);
        pthread_mutex_unlock(info->mutex);
        pthread_exit(nullptr);
        return nullptr;
    }

private:
    void find_frequent_items();

    void build_header_table();

    void exclude_non_frequent_items();

    void sort_transaction_items();

    void construct_fp_tree();

    FPTree construct_conditional_fp_tree(Item x);

    FrequentPatterns find_all_pattern_paths(Item x);

    bool is_frequent(const Item item) {
        return frequent_items.find(item) != frequent_items.end();
    }

    HeaderKey get_key(const Item x) {
        return std::make_pair(x, item_counter[x]);
    }

    bool frequent_than(const Item lhs, const Item rhs) {
        return FrequencyCmp()(get_key(lhs), get_key(rhs));
    }

    TransactionDB transactions;
    long double min_support;
    long double min_support_count;
    std::unordered_map<Item, int> item_counter{};
    std::set<Item> frequent_items{};
    HeaderTable header_table{};
    TreeNode* root{};
    FPTree* parent{};
};

void FPTree::find_frequent_items() {
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
            header_table[item] = std::make_pair(nullptr, nullptr);
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
                       [](Transaction& t) { return t.empty(); }),
        transactions.end());
}

void FPTree::sort_transaction_items() {
    if (!parent) {
        std::for_each(
            transactions.begin(), transactions.end(),
            [this](Transaction& t) {
                std::sort(t.begin(), t.end(),
                          [this](const Item a, const Item b) { return frequent_than(a, b); });
            }
        );
    } else {
        std::for_each(
            transactions.begin(), transactions.end(),
            [this](Transaction& t) {
                std::sort(t.begin(), t.end(),
                          [this](const Item a, const Item b) { return parent->frequent_than(a, b); });
            }
        );
    }
}

void FPTree::construct_fp_tree() {
    root = new TreeNode(-1);
    for (const auto& transaction: transactions) {
        TreeNode* curr = root;
        for (const auto& item: transaction) {
            auto[node, ok] = curr->add_item_to_child(item);

            // handle cross-links
            if (ok) {
                auto key = get_key(item);
                auto& ht_entry = header_table[key];
                if (!ht_entry.first) {
                    ht_entry.first = ht_entry.second = node;
                } else {
                    ht_entry.second->cross_link = node;
                    ht_entry.second = ht_entry.second->cross_link;
                }
            }

            curr = node;
        }
    }
}

FPTree FPTree::construct_conditional_fp_tree(Item x) {
    auto paths = find_all_pattern_paths(x);
    TransactionDB conditional_base;
    for (const auto& path: paths) {
        Transaction t(path.first.cbegin(), path.first.cend());
        conditional_base.insert(
            conditional_base.end(), static_cast<int>(path.second), t);
    }
    FPTree conditional_fp(std::move(conditional_base), min_support, min_support_count, this);
    return conditional_fp;
}

FrequentPatterns FPTree::find_all_pattern_paths(Item x) {
    auto links = header_table[get_key(x)];
    std::deque<TreeNode*> visiting_stack;
    FrequentPatterns paths;

    for (auto leaf = links.first; leaf; leaf = leaf->cross_link) {
        Pattern pat;
        for (auto curr = leaf; curr != root; curr = curr->parent) {
            pat.insert(curr->item);
        }
        paths[pat] += leaf->count;
    }

    return paths;
}


/// <h1>Output Formatting</h1>

std::ostream& operator<<(std::ostream& out, const Pattern& p) {
    out << "{ ";
    for (const auto& item: p) {
        out << item << ", ";
    }
    out << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, const Patterns& ps) {
    for (const auto& p: ps) {
        out << p << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const FrequentPatterns& fp) {
    for (const auto& p: fp) {
        out << p.first << " : " << p.second << std::endl;
    }
    return out;
}


/// <h1>Output</h1>

void print_frequent_patterns_to(std::ostream& out, const FrequentPatterns& fp, std::size_t total) {
    auto flags{out.flags()};

    for (const auto& pattern: fp) {
        auto it = pattern.first.cbegin();
        out << *it++;
        for (; it != pattern.first.cend(); ++it) {
            out << "," << *it;
        }
        out << ":" << std::fixed << std::setprecision(4) << pattern.second / total << std::endl;
    }

    out.flags(flags);
}


int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "invalid argument!" << std::endl;
        std::exit(1);
    }

    auto start = std::chrono::steady_clock::now();

    long double min_support = std::strtold(argv[1], nullptr);
    std::string in_filename = std::string(argv[2]);
    std::string out_filename = std::string(argv[3]);

    auto&& transactions = read_transaction_file(in_filename);
    auto total_counts = transactions.size();
    FPTree fp_tree(std::move(transactions), min_support, total_counts * min_support);

    FrequentPatterns fps;

    constexpr int thread_count = 8;
    pthread_t threads[thread_count];
    ThreadInfo infos[thread_count];

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, nullptr);

    for (int i = 0; i < 8; ++i) {
        infos[i].t_id = i;
        infos[i].t_count = thread_count;
        infos[i].tree = &fp_tree;
        infos[i].mutex = &mutex;
        infos[i].result = &fps;
        pthread_create(&threads[i], nullptr, FPTree::thread_mine, &infos[i]);
    }

    for (auto& thread: threads) {
        pthread_join(thread, nullptr);
    }

    pthread_mutex_destroy(&mutex);

    std::ofstream fout(out_filename);
    print_frequent_patterns_to(fout, fps, total_counts);
    fout.close();

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout << "elapsed time: " << elapsed_seconds.count() << "s" << std::endl;

    return 0;
}
