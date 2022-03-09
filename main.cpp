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

        TreeNode* prev = nullptr;
        for (auto child = left_child; child != nullptr; prev = child, child = child->right_sibling) {
            if (child->item == x) {
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

    // delete the subtree
    void prune_subtree() {
        delete left_child;
        left_child = nullptr;
    }

    // Return true if `x` not in any subtree and thus can be removed.
    // Does NOT handle cross-links, so after this action, the cross-links of
    // nodes whose item is not `x` will become invalid
    bool remove_subtrees_without(Item x) {
        if (!left_child) {
            return item != x;
        }

        bool all_without_x = true;
        TreeNode* prev{};
        for (auto child = left_child; child != nullptr;) {
            bool without_x = child->remove_subtrees_without(x);
            all_without_x = all_without_x && without_x;

            if (without_x) {
                auto temp = child->right_sibling;
                if (prev) {
                    prev->right_sibling = child->right_sibling;
                } else {
                    // child is the leftmost child
                    left_child = child->right_sibling;
                }
                child->right_sibling = nullptr;
                delete child;
                child = temp;
            } else {
                prev = child;
                child = child->right_sibling;
            }
        }

        return all_without_x;
    }

    void append_to_child(TreeNode* target) {
        if (!left_child) {
            left_child = target;
            return;
        }

        auto curr = left_child;
        for (; curr->right_sibling; curr = curr->right_sibling) {}
        curr->right_sibling = target;

        for (auto t = target; t; t = t->right_sibling) {
            t->parent = this;
        }
    }

    void merge_children() {
        if (!left_child) {
            return;
        }

        std::map<Item, TreeNode*> exists;
        TreeNode* prev{};
        for (auto curr = left_child; curr;) {
            if (exists.find(curr->item) == exists.end()) {
                exists[curr->item] = curr;
                prev = curr;
                curr = curr->right_sibling;
            } else {
                // merge counts
                auto major_node = exists[curr->item];
                major_node->count += curr->count;
                major_node->append_to_child(curr->left_child);
                curr->left_child = nullptr;

                // remove curr from tree
                prev->right_sibling = curr->right_sibling;
                delete curr;
                curr = prev->right_sibling;
            }
        }
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
        transactions(std::move(transactions)), min_support_count(ms * transactions.size()) {
        build();
    }

    void build() {
        find_frequent_items();
        build_header_table();
        exclude_non_frequent_items();
        sort_transaction_items();
        construct_fp_tree();
    }

    // mining will make some unrecoverable changes to the tree, so be sure that the tree is copied
    void mine(Item x) {
        prune_by(x);
        refresh_counts(x);
        construct_conditional_fp_tree(x);
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
                std::cout << curr->item << " : " << curr->count << ", ";
            }
            std::cout << std::endl;
        }
    }


    // there is no need to copy transactions and frequent_items
    // since they will not participate in the mining stage
    FPTree(const FPTree& oth)
        : min_support_count(oth.min_support_count), item_counter(oth.item_counter) {
        std::tie(header_table, root) = oth.root->copy(oth.header_table);
    }

    ~FPTree() {
        delete root;
    }

private:
    void find_frequent_items();

    void build_header_table();

    void exclude_non_frequent_items();

    void sort_transaction_items();

    void construct_fp_tree();

    void prune_by(Item x);

    void refresh_counts(Item x);

    void construct_conditional_fp_tree(Item x);

    void mine_conditional_fp_tree(Item x);


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
    double min_support_count;
    std::map<Item, int> item_counter{};
    std::unordered_set<Item> frequent_items{};
    HeaderTable header_table{};
    TreeNode* root{};
    bool remove_one_child(TreeNode* node);
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

void FPTree::prune_by(Item x) {
    root->remove_subtrees_without(x);
}

void FPTree::refresh_counts(Item x) {
    auto links = header_table[get_key(x)];

    // refresh item counts in a bottom-up manner
    for (auto curr = links.first; curr != nullptr; curr = curr->cross_link) {
        for (auto p = curr->parent; p != nullptr; p = p->parent) {
            p->count = 0;
        }
    }
    for (auto curr = links.first; curr != nullptr; curr = curr->cross_link) {
        for (auto p = curr->parent; p != nullptr; p = p->parent) {
            p->count += curr->count;
        }
    }
}

void FPTree::construct_conditional_fp_tree(Item x) {
    std::deque<TreeNode*> nodes_to_be_check;
    nodes_to_be_check.emplace_back(root);

    while (!nodes_to_be_check.empty()) {
        auto node = nodes_to_be_check.front();
        do {
            node->merge_children();
        } while (remove_one_child(node));

        for (auto child = node->left_child; child; child = child->right_sibling) {
            nodes_to_be_check.push_back(child);
        }
        nodes_to_be_check.pop_front();
    }
}

bool FPTree::remove_one_child(TreeNode* node) {
    if (!node->left_child) {
        return false;
    }

    // pair<prev, curr>
    std::pair<TreeNode*, TreeNode*> candidate;

    TreeNode* prev{};
    for (auto curr = node->left_child; curr; prev = curr, curr = curr->right_sibling) {
        if (curr->count < min_support_count) {
            if (!candidate.second ||
                frequent_than(curr->item, candidate.second->item)) {
                candidate = std::make_pair(prev, curr);
            }
        }
    }
    if (candidate.second) {
        candidate.first->right_sibling = candidate.second->right_sibling;
        node->append_to_child(candidate.second->left_child);
        candidate.second->left_child = nullptr;
        candidate.second->right_sibling = nullptr;
        delete candidate.second;
        return true;
    }

    return false;
}

void FPTree::mine_conditional_fp_tree(Item x) {

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
