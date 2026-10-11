#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <vector>
using namespace std;

constexpr int DIM = 2;
const double INF = numeric_limits<double>::infinity();

struct Rect {
    array<double, DIM> lo, hi;

    double area() const {
        double a = 1.0;
        for (int d = 0; d < DIM; d++) a *= hi[d] - lo[d];
        return a;
    }

    bool overlaps(const Rect& o) const {
        for (int d = 0; d < DIM; d++)
            if (lo[d] > o.hi[d] || o.lo[d] > hi[d]) return false;
        return true;
    }

    bool operator==(const Rect& o) const { return lo == o.lo && hi == o.hi; }
};

Rect combine(const Rect& a, const Rect& b) {
    Rect r;
    for (int d = 0; d < DIM; d++) {
        r.lo[d] = min(a.lo[d], b.lo[d]);
        r.hi[d] = max(a.hi[d], b.hi[d]);
    }
    return r;
}

double enlargement(const Rect& r, const Rect& add) {
    return combine(r, add).area() - r.area();
}

ostream& operator<<(ostream& os, const Rect& r) {
    os << "[(";
    for (int d = 0; d < DIM; d++) os << r.lo[d] << (d + 1 < DIM ? "," : "");
    os << ")-(";
    for (int d = 0; d < DIM; d++) os << r.hi[d] << (d + 1 < DIM ? "," : "");
    return os << ")]";
}

struct Node;

struct Entry {
    Rect I;
    Node* child = nullptr;
    int tuple = -1;
};

struct Node {
    explicit Node(int lvl) : level(lvl) {}
    int level;
    vector<Entry> entries;
    bool isLeaf() const { return level == 0; }
};

Rect cover(const vector<Entry>& g) {
    Rect r = g[0].I;
    for (const Entry& e : g) r = combine(r, e.I);
    return r;
}

class RTree {
public:
    RTree(int maxE, int minE, bool lin = false)
        : M(maxE), m(minE), linear(lin), root(new Node(0)) {}

    vector<int> search(const Rect& S) const {
        vector<int> out;
        search(root, S, out);
        return out;
    }

    void insert(const Rect& I, int tuple) {
        insertEntry(Entry{I, nullptr, tuple}, 0);
    }

    bool remove(const Rect& I, int tuple) {
        vector<Node*> path;
        if (!findLeaf(root, I, tuple, path)) return false;
        Node* L = path.back();

        for (size_t i = 0; i < L->entries.size(); i++) {
            if (L->entries[i].I == I && L->entries[i].tuple == tuple) {
                L->entries.erase(L->entries.begin() + i);
                break;
            }
        }

        condenseTree(path);

        if (!root->isLeaf() && root->entries.size() == 1) {
            Node* old = root;
            root = root->entries[0].child;
            delete old;
        }
        return true;
    }

    void print() const {
        cout << "(altura = " << root->level + 1 << ")\n";
        print(root, 0);
    }

private:
    int M, m;
    bool linear;
    Node* root;

    void search(const Node* T, const Rect& S, vector<int>& out) const {
        for (const Entry& E : T->entries) {
            if (!E.I.overlaps(S)) continue;
            if (T->isLeaf()) out.push_back(E.tuple);
            else search(E.child, S, out);
        }
    }

    void insertEntry(const Entry& E, int level) {
        vector<Node*> path = chooseLeaf(E.I, level);
        Node* L = path.back();

        L->entries.push_back(E);
        Node* LL = nullptr;
        if ((int)L->entries.size() > M) LL = splitNode(L);

        Node* rootSplit = adjustTree(path, LL);

        if (rootSplit) {
            Node* newRoot = new Node(root->level + 1);
            newRoot->entries.push_back(Entry{cover(root->entries), root, -1});
            newRoot->entries.push_back(Entry{cover(rootSplit->entries), rootSplit, -1});
            root = newRoot;
        }
    }

    vector<Node*> chooseLeaf(const Rect& I, int level) const {
        Node* N = root;
        vector<Node*> path{N};
        while (N->level > level) {
            const Entry* F = nullptr;
            double bestEnl = INF, bestArea = INF;
            for (const Entry& e : N->entries) {
                double enl = enlargement(e.I, I);
                double ar = e.I.area();
                if (enl < bestEnl || (enl == bestEnl && ar < bestArea)) {
                    F = &e;
                    bestEnl = enl;
                    bestArea = ar;
                }
            }
            N = F->child;
            path.push_back(N);
        }
        return path;
    }

    Node* adjustTree(const vector<Node*>& path, Node* LL) {
        int i = (int)path.size() - 1;
        Node* N = path[i];
        Node* NN = LL;

        while (N != root) {
            Node* P = path[i - 1];
            P->entries[indexOf(P, N)].I = cover(N->entries);

            Node* PP = nullptr;
            if (NN) {
                P->entries.push_back(Entry{cover(NN->entries), NN, -1});
                if ((int)P->entries.size() > M) PP = splitNode(P);
            }

            N = P;
            NN = PP;
            i--;
        }
        return NN;
    }

    bool findLeaf(Node* T, const Rect& I, int tuple, vector<Node*>& path) {
        path.push_back(T);
        for (const Entry& F : T->entries) {
            if (T->isLeaf()) {
                if (F.I == I && F.tuple == tuple) return true;
            } else if (F.I.overlaps(I) && findLeaf(F.child, I, tuple, path)) {
                return true;
            }
        }
        path.pop_back();
        return false;
    }

    void condenseTree(const vector<Node*>& path) {
        int i = (int)path.size() - 1;
        Node* N = path[i];
        vector<Node*> Q;

        while (N != root) {
            Node* P = path[i - 1];
            int k = indexOf(P, N);

            if ((int)N->entries.size() < m) {
                P->entries.erase(P->entries.begin() + k);
                Q.push_back(N);
            } else {
                P->entries[k].I = cover(N->entries);
            }

            N = P;
            i--;
        }

        for (Node* q : Q) {
            for (const Entry& e : q->entries) insertEntry(e, q->level);
            delete q;
        }
    }

    Node* splitNode(Node* N) {
        vector<Entry> E = N->entries, G1, G2;

        int s1, s2;
        if (linear) linearPickSeeds(E, s1, s2);
        else pickSeeds(E, s1, s2);

        G1.push_back(E[s1]);
        G2.push_back(E[s2]);
        Rect R1 = E[s1].I, R2 = E[s2].I;
        E.erase(E.begin() + max(s1, s2));
        E.erase(E.begin() + min(s1, s2));

        while (!E.empty()) {
            if ((int)(G1.size() + E.size()) <= m) {
                G1.insert(G1.end(), E.begin(), E.end());
                break;
            }
            if ((int)(G2.size() + E.size()) <= m) {
                G2.insert(G2.end(), E.begin(), E.end());
                break;
            }

            int k = linear ? 0 : pickNext(E, R1, R2);
            Entry e = E[k];
            E.erase(E.begin() + k);

            double d1 = enlargement(R1, e.I);
            double d2 = enlargement(R2, e.I);
            bool toG1;
            if (d1 != d2) toG1 = d1 < d2;
            else if (R1.area() != R2.area()) toG1 = R1.area() < R2.area();
            else toG1 = G1.size() <= G2.size();

            if (toG1) { G1.push_back(e); R1 = combine(R1, e.I); }
            else      { G2.push_back(e); R2 = combine(R2, e.I); }
        }

        N->entries = G1;
        Node* NN = new Node(N->level);
        NN->entries = G2;
        return NN;
    }

    void pickSeeds(const vector<Entry>& E, int& s1, int& s2) const {
        s1 = 0;
        s2 = 1;
        double worst = -INF;
        for (int i = 0; i < (int)E.size(); i++) {
            for (int j = i + 1; j < (int)E.size(); j++) {
                double d = combine(E[i].I, E[j].I).area() - E[i].I.area() - E[j].I.area();
                if (d > worst) { worst = d; s1 = i; s2 = j; }
            }
        }
    }

    int pickNext(const vector<Entry>& E, const Rect& R1, const Rect& R2) const {
        int best = 0;
        double maxDiff = -INF;
        for (int i = 0; i < (int)E.size(); i++) {
            double d1 = enlargement(R1, E[i].I);
            double d2 = enlargement(R2, E[i].I);
            double diff = d1 > d2 ? d1 - d2 : d2 - d1;
            if (diff > maxDiff) { maxDiff = diff; best = i; }
        }
        return best;
    }

    void linearPickSeeds(const vector<Entry>& E, int& s1, int& s2) const {
        s1 = 0;
        s2 = 1;
        int n = (int)E.size();
        double bestSep = -INF;

        for (int d = 0; d < DIM; d++) {
            int hl = 0;
            for (int i = 1; i < n; i++)
                if (E[i].I.lo[d] > E[hl].I.lo[d]) hl = i;

            int lh = (hl == 0) ? 1 : 0;
            for (int i = 0; i < n; i++)
                if (i != hl && E[i].I.hi[d] < E[lh].I.hi[d]) lh = i;

            double sep = E[hl].I.lo[d] - E[lh].I.hi[d];

            double minLo = INF, maxHi = -INF;
            for (const Entry& e : E) {
                minLo = min(minLo, e.I.lo[d]);
                maxHi = max(maxHi, e.I.hi[d]);
            }
            double width = maxHi - minLo;
            double norm = width > 0 ? sep / width : 0.0;

            if (norm > bestSep) { bestSep = norm; s1 = hl; s2 = lh; }
        }
    }

    static int indexOf(const Node* P, const Node* N) {
        for (int k = 0; k < (int)P->entries.size(); k++)
            if (P->entries[k].child == N) return k;
        return -1;
    }

    void print(const Node* n, int depth) const {
        string indent(depth * 4, ' ');
        cout << indent << (n->isLeaf() ? "Hoja" : "Nodo interno")
             << " (nivel " << n->level << ", " << n->entries.size() << " entradas)\n";
        for (const Entry& e : n->entries) {
            cout << indent << "  " << e.I;
            if (n->isLeaf()) {
                cout << "  tuple=" << e.tuple << "\n";
            } else {
                cout << "\n";
                print(e.child, depth + 1);
            }
        }
    }
};

void printResult(const vector<int>& ids) {
    cout << "tuples: ";
    for (int id : ids) cout << id << " ";
    cout << "\n";
}

int main() {
    vector<Rect> data = {
        {{0, 0}, {2, 2}},   {{1, 1}, {3, 3}},   {{5, 5}, {6, 6}},
        {{8, 8}, {9, 9}},   {{2, 5}, {3, 6}},   {{6, 1}, {7, 2}},
        {{4, 4}, {5, 5}},   {{9, 0}, {10, 1}},  {{0, 8}, {1, 9}},
        {{3, 8}, {4, 9}},   {{7, 6}, {8, 7}},   {{5, 9}, {6, 10}},
    };

    RTree tree(4, 2, false);
    for (int i = 0; i < (int)data.size(); i++) tree.insert(data[i], i);

    cout << "=== Arbol tras insertar " << data.size() << " rectangulos ";
    tree.print();

    Rect S = {{0, 0}, {4, 4}};
    cout << "\n=== Search " << S << " ===\n";
    printResult(tree.search(S));

    cout << "\n=== Delete de tuples 0, 1 y 6 ===\n";
    tree.remove(data[0], 0);
    tree.remove(data[1], 1);
    tree.remove(data[6], 6);
    tree.print();

    cout << "\n=== Search " << S << " tras eliminar ===\n";
    printResult(tree.search(S));

    const char* names[] = {"Quadratic", "Linear"};
    for (int a = 0; a < 2; a++) {
        RTree t(4, 2, a == 1);
        for (int i = 0; i < (int)data.size(); i++) t.insert(data[i], i);
        cout << "\n=== Split " << names[a] << " ";
        t.print();
    }
}