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

using LightweightTree = std::pair<HeaderTable, TreeNode*>;

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

/// <h1>Tree Node</h1>

struct TreeNode {
    using NodeMapping = std::unordered_map<const TreeNode*, TreeNode*>;

    Item data;
    int count{1};
    TreeNode* left_child{};
    TreeNode* right_sibling{};
    TreeNode* cross_link{};
    TreeNode* parent{};

    explicit TreeNode(Item x, TreeNode* p = nullptr) : data(x), parent(p) {}

    TreeNode(const TreeNode& other) : data(other.data), count(other.count) {}

    // Add new node(x) to child, or increment the count if the node(x) already exists.
    // Then return the affected node and whether the insertion is successful or not
    std::pair<TreeNode*, bool> add_item_to_child(Item x) {
        if (!left_child) {
            left_child = new TreeNode(x, this);
            return std::make_pair(left_child, true);
        }

        TreeNode* prev = nullptr;
        for (auto child = left_child; child != nullptr; prev = child, child = child->right_sibling) {
            if (child->data == x) {
                child->count++;
                return std::make_pair(child, false);
            }
        }

        prev->right_sibling = new TreeNode(x, this);
        return std::make_pair(prev->right_sibling, true);
    }

    // return the root of the copied tree
    LightweightTree copy(HeaderTable ht) const {
        NodeMapping node_map;
        TreeNode* new_root = _copy_tree_edge(node_map, nullptr);
        _copy_cross_link(ht, node_map);

        return std::make_pair(std::move(ht), new_root);
    }

    // delete the child tree
    void prune() {
        delete left_child;
        left_child = nullptr;
    }

    ~TreeNode() {
        delete left_child;
        delete right_sibling;
    }

private:
    TreeNode* _copy_tree_edge(NodeMapping& node_map, TreeNode* p) const {
        auto new_root = new TreeNode(*this);
        new_root->parent = p;
        node_map[this] = new_root;

        if (left_child) {
            new_root->left_child = left_child->_copy_tree_edge(node_map, new_root);
        }
        if (right_sibling) {
            new_root->right_sibling = right_sibling->_copy_tree_edge(node_map, p);
        }
        return new_root;
    }

    void _copy_cross_link(HeaderTable& ht, NodeMapping& node_map) const {
        // copy cross-links
        for (auto&[key, head_tail]: ht) {
            if (head_tail.first) {
                for (auto node = head_tail.first; node != nullptr;) {
                    const auto next = node->cross_link;
                    if (next) {
                        node_map[node]->cross_link = node_map[next];
                    }
                    node = next;
                }
                head_tail.first = node_map[head_tail.first];
                head_tail.second = node_map[head_tail.second];
            }
        }
    }

};

/// <h1>FP Tree</h1>

class FPTree {
public:
    explicit FPTree(TransactionDB&& transactions, double ms) :
        transactions(std::move(transactions)), min_support(ms) {
        build();
    }

    void build() {
        find_frequent_items();
        build_header_table();
        exclude_non_frequent_items();
        sort_transaction_items();
        construct_fp_tree();
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
            for (TreeNode* curr = links.second.first; curr != nullptr; curr = curr->cross_link) {
                std::cout << curr->data << " : " << curr->count << ", ";
            }
            std::cout << std::endl;
        }
    }


    // there is no need to copy transactions and frequent_items
    // since they will not participate in the mining stage
    FPTree(const FPTree& oth)
        : min_support(oth.min_support), item_counter(oth.item_counter) {
        std::tie(header_table, root) = oth.root->copy(oth.header_table);
    }

    ~FPTree() {
        // TODO: delete the tree
        delete root;
    }

private:
    void find_frequent_items();

    void build_header_table();

    void exclude_non_frequent_items();

    void sort_transaction_items();

    void construct_fp_tree();

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
    double min_support;
    std::map<Item, int> item_counter{};
    std::unordered_set<Item> frequent_items{};
    HeaderTable header_table{};
    TreeNode* root{};
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
    std::for_each(
        transactions.begin(), transactions.end(),
        [this](Transaction& t) {
            std::sort(t.begin(), t.end(),
                      [this](const Item a, const Item b) { return frequent_than(a, b); });
        }
    );
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


int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "invalid argument!" << std::endl;
        std::exit(1);
    }

    double min_support = std::strtod(argv[1], nullptr);
    std::string in_filename = std::string(argv[2]);
    std::string out_filename = std::string(argv[3]);

    auto&& transactions = read_transaction_file(in_filename);
    FPTree fp_tree(std::move(transactions), min_support);
//    fp_tree.print_transaction_db();
//    std::cout << std::endl;
//    fp_tree.print_header_table();
//    std::cout << std::endl;
//    fp_tree.traverse_cross_links();
//
//    std::cout << std::endl;
//    FPTree copy{fp_tree};
//    copy.print_header_table();
//    std::cout << std::endl;
//    copy.traverse_cross_links();

    return 0;
}
