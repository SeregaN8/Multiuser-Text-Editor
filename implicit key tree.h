#ifndef IMPLICIT_TREAP_
#define IMPLICIT_TREAP_

#include <random>
#include <chrono>
#include <cstring>
#include <fstream>

std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

struct treap {
    const char* line;
    treap* left, * right;
    treap* father = nullptr;
    int size;
    int64_t text_len;
    int subtree_len;
    int reservist{ -1 };

    treap(const char* line_) : line(line_), left(nullptr), right(nullptr), size(1),
                               text_len(std::strlen(line) + 1), subtree_len(text_len) {}
    treap(const char* line_, treap* l, treap* r) : line(line_), left(l), right(r), text_len(std::strlen(line) + 1) {
        update_size();
    }

    void update_size() {
        size = 1, subtree_len = text_len;
        if (left) size += left->size, subtree_len += left->subtree_len;
        if (right) size += right->size, subtree_len += right->subtree_len;
    }

    void remove_reservation() {
        reservist = -1;
        if (left) left->remove_reservation();
        if (right) right->remove_reservation();
    }

    void set_reservation(int r) {
        reservist = r;
        if (left) left->set_reservation(r);
        if (right) right->set_reservation(r);
    }

    std::pair<int, int> is_tree_reserved() {
        if (left) {
            if (auto pt = left->is_tree_reserved(); pt.first != -1) {
                return pt;
            }
        }
        if (reservist != -1) return { reservist, 1 + (left ? left->size : 0) };
        if (right) {
            if (auto pt = right->is_tree_reserved(); pt.first != -1) {
                return { pt.first, pt.second + 1 + (left ? left->size : 0) };
            }
        }
        return { -1 , -1 };
    }

    ~treap() {
        delete[] line;
        delete left;
        delete right;
    }
};

void split(treap* v, treap*& l, treap*& r, int ind) {
    if (!v) l = r = nullptr;
    else {
        int sz = 1;
        if (v->left) sz += (v->left)->size;
        if (ind >= sz) {
            split(v->right, v->right, r, ind - sz), l = v;
            if (v->right) (v->right)->father = v;
            if (r) r->father = nullptr;
        } else {
            split (v->left, l, v->left, ind), r = v;
            if (v->left) (v->left)->father = v;
            if (l) l->father = nullptr;
        }
        if (l) l->update_size();
        if (r) r->update_size();
    }
}

treap* merge(treap* l, treap* r) {
    if (!l) return r;
    if (!r) return l;
    int s1 = l->size, s2 = r->size;
    if (rng() % (s1 + s2) < s1) {
        l->right = merge(l->right, r);
        if (l->right) (l->right)->father = l;
        l->update_size();
        return l;
    }
    else {
        r->left = merge(l, r->left);
        if (r->left) (r->left)->father = r;
        r->update_size();
        return r;
    }
}

void remove_reservation(int l, int r, treap* &t) {
    if (!t) return;
    if (r < l) return;
    if (l < 1) return;
    treap* t1, * t2, * t3;
    split(t, t2, t3, r);
    split(t2, t1, t2, l - 1);
    t2->remove_reservation();
    t1 = merge(t1, t2);
    t1 = merge(t1, t3);
    t = t1;
}

std::pair<int, int> check_reservation(int l, int r, treap*& t) {
    if (r < l || l < 1 || !t) return { -1 , -1 };
    treap* t1, * t2, * t3;
    split(t, t2, t3, r);
    split(t2, t1, t2, l - 1);
    auto ans = t2->is_tree_reserved();
    if (ans.second != -1 && t1) ans.second += t1->size;
    t1 = merge(t1, t2);
    t1 = merge(t1, t3);
    t = t1;
    return ans;
}

void set_reservation(int l, int r, treap*& t, int reservist) {
    if (!t) return;
    if (r < l) return;
    if (l < 1) return;
    treap* t1, * t2, * t3;
    split(t, t2, t3, r);
    split(t2, t1, t2, l - 1);
    t2->set_reservation(reservist);
    t1 = merge(t1, t2);
    t1 = merge(t1, t3);
    t = t1;
}

treap* built_treap(int l, int r, const std::vector<const char*>& strings) {
    if (l >= r) return nullptr;
    int ind = l + rng() % (r - l);
    return new treap(strings[ind], built_treap(l, ind, strings), built_treap(ind + 1, r, strings));
}

treap* init_from_buffer(const char* buffer) {
    size_t begin_pos{}, end_pos{};
    std::vector<const char*> strs;

    while (buffer[begin_pos]) {
        while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
        int val{}, r_pos = end_pos;
        while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;

        char* detached_line = new char[end_pos + 1 - val - begin_pos];
        size_t write_pos{};

        while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
        detached_line[write_pos] = 0;
        strs.emplace_back(detached_line);

        if (buffer[end_pos] == 0) break;
        begin_pos = ++end_pos;
    }
    return built_treap(0, strs.size(), strs);
}

void write_tree_to_buffer(treap* t, char* buffer) {
    if (!t) return;
    int pos = 0;
    if (t->left) write_tree_to_buffer(t->left, buffer), pos += (t->left)->subtree_len;
    memcpy(buffer + pos, t->line, t->text_len), pos += t->text_len, buffer[pos - 1] = '\n';
    if (t->right) write_tree_to_buffer(t->right, buffer + pos), pos += (t->right)->subtree_len;
    buffer[pos] = 0;
}

int get_ind_by_pt(treap* t) {
    if (!t) return -1;
    int ind = 1 + (t->left ? (t->left)->size : 0);
    while (t->father) {
        treap* f = t->father;
        if (!(f->left)) ++ind;
        else if (f->left != t) ind += 1 + (f->left)->size;
        t = f;
    }
    return ind;
}

treap* get_pt_by_ind(treap* ver, int i) {
    if (!ver) return nullptr;
    int med = 1 + (ver->left ? (ver->left)->size : 0);
    if (i < med) return get_pt_by_ind(ver->left, i);
    if (i == med) return ver;
    return get_pt_by_ind(ver->right, i - med);
}

void remove(treap* &ver, int l, int r) {
    if (!ver) return;
    if (r < l) return;
    if (l < 1) return;
    treap* t1, * t2, * t3;
    split(ver, t2, t3, r);
    split(t2, t1, t2, l - 1);
    delete t2;
    t1 = merge(t1, t3);
    ver = t1;
}

void insert_after(treap*& t, int l, treap* ins) {
    treap* t1, *t2;
    split(t, t1, t2, l - 1);
    t1 = merge(t1, ins);
    t1 = merge(t1, t2);
    t = t1;
}

void write_to_file(treap* t, std::ofstream& ofs) {
    if (!t) return;
    if (t->left) write_to_file(t->left, ofs);
    while (*(t->line)) ofs << *(t->line), ++(t->line);
    ofs << '\n';
    if (t->right) write_to_file(t->right, ofs);
}

#endif
