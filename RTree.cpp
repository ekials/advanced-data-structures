#include <iostream>
#include <vector>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

using namespace std;

//nro de dimensiones
constexpr int DIM = 2;

const double INF = numeric_limits<double>::infinity();

//corazon del RTree: estructura que representa el Minimum Bounding Rectangle (MBR) de un objeto
//(o de un conjunto de objetos aaa) en el paper: I = (I_0, I_1, .... , I_n-1)
// cada lii es un intervalo cerrado [a, b] que describe la extensi´pn del objeto en la dimension i

struct Rect
{
    array <double, DIM> lo; //lmite inferior en cada dimension
    array <double, DIM> hi; //limite superior en cada dimension

    //1 area: calcula el area o volumen en mas dimensiones del rectangulo, se usa en
    // chooseLeaf (elegir el hijo que menos crece), pickSeeds / pickNext (criteriores de split), enlargement
    double area() const
    {
        double a = 1.0;
        for(int d = 0; d < DIM; d++) a *= hi[d] - lo[d];
        // lo[d] = estremo inferior del intervalo en la dimension d
        // hi[d] = extremo superior del intervalo en la dimension d
        return a;
    }

    //2 overlaps: devuelve true si los dos rectangulos se superponen (incluyendo tocarse en el borde)
    //esta es la funcion clave del search
    //si un MBR de un nodo no se superpone con el rectangulo de consulta, todo 
    //subarbol se puede descartar
    bool overlaps (const Rect& o) const
    {
        for(int d = 0; d < DIM; d++) if(lo[d] > o.hi[d] || o.lo[d] > hi[d]) {return false;}
        return true;
    }

    //3 operator: comparar si 2 rectangulos son excatamente iguales
    // se usa principalemente en el Delete (FindLeaf), el paper exige que se borre el registro con el mismo rectangulo que se inserto
    bool operator == (const Rect& o) const {return lo == o.lo && hi == o.hi;}

    //4 isValid()
    //valida que el rectangulo sea correcto, coordenadas finitas (no NaN ni infinite), lo[d] <= hi[d] en todas las dimensiones
    //no tener esta funcion es critico ... y porque¿
    //problema
    //NaN : Todas las comparaciones dan flase -> se rompe la superposicion y el delete
    //infinito: infinito - infinito = NaN -> las areas y los enlargement se corrompen
    // lo > hi es un rectangulo invalodp, area negativa

    bool isValid () const
    {
        for(int d = 0; d < DIM; d++)
        {
            if(!isfinite(lo[d]) || !isfinite(hi[d]) || lo[d] > hi[d]) return false;
            return true;
        }
    }
};


//1 Rect combine : el MBR que envuelve dos rectangulos
//devuelve el rectangulo mas pequeño que contiene completamente a 'a' y ''b'
//2D (para guiarse TT TT)
/*
a:          b:
┌────┐          ┌──┐
│    │          │  │
└────┘          └──┘

//combine(a,b):
┌──────────────┐
│              │
│              │
└──────────────┘
*/
// se usa cuando actualizamos el MBR de un nodo despues de insertar o borrar(adjustTree, condenseTree)
//tmb dentri de enlargement
//tmb dentro de los algoritmos split (para calcular el area de cada grupo)

Rect combine (const Rect& a, const Rect& b)
{
    Rect r;
    for(int d = 0; d < DIM; d++)
    {
        r.lo[d] = min (a.lo[d], b.lo[d]); //lim inferior:busca el punto de inicio mas a la izq o abajo entre los dos rectangulos, es el menor valor para asegurar que ningun objeto quede fuera por el lado izq o inderior
        r.hi[d] = max (a.hi[d], b.hi[d]); //lim superior:busca el punto final mas a la derecha o arriba,se elige el mayor para garantizar que cubra totalmente el extremo superior / derecho de ambos
    }
    return r;
}

//2 enlargement : cuando crece el area
//calulamos cuanto aumenta el area del rectangulo r si le agregamos el rectnagulo add
// enlargement (r, add) = area (r u add) - area(r)
// para entender mejor: enlargement = area del nuevo rectangulo combinado - area del rectangulo original
// si el rectangulo add ya esta completamente dentro de r : combine (r, add) es exacamente igual a r, su area no cambia, por lo que 36 - 36 = 0 (o el valor que sea), el crecimiento es 0 porque no obluga a estirar los limites del contenedor
// di add esta fuera o parcialmente fuera de r: el rectangulo contenedor tiene que estirarse obligatoriamente para alcanzar los nuevos limites de add, este estiramiento genera una zona extra, puede ser un espacio vacio o dead space, el nro resultante mide exactamente cuanta duperficie extra de area muerta o nueva estamos obligados a abarcar
//importante porque si elegimos mal el nodo al insertar, los rectangulos empezarian a crecer de manera desproporcionada y a cruzarse demasiados entre si, cuando se realza una busqueda search, el algoritmo se vera obligado a revisar muchisimos mas nodos porque los rectangulos se superponen por todas partes, destruyendo la eficiencia de la estructura
//asi q enlargement actua como una metrica de castigo: evalua a los candidatos y prioriza siempre al que crezca menos
// tmb si hay empate en el crecimeinto, se elige el rectangulo que tenga el menor area absoluta (bestarea), la logica es que un rectangulo pequeño y compacto es mejor que uno gigante, aunque ambos crezcan lo mismo
// uso principal en ChooseLeaf y PickNext
double enlargement (const Rect& r, const Rect& add){ return combine (r, add).area() - r.area();}

//3 operator<< print rectangulo legible para debug uwu 
ostream& operator << (ostream& os, const Rect& r)
{
    os << "[(";
    for(int d = 0; d < DIM < d++) os << r.lo[d] << (d + 1 < DIM ? "," : "");
    os << ")-(";
    for(int d = 0; d < DIM; d++) os << r.hi[d] << (d + 1 < DIM ? "," : "");
    return os << ")]";
}

//entradas y nodos
//entrada de hoja: (I, tuple-identifier)
//entrada interna: (I, child-pointer)

struct Node; //declaracion adelantada y ya

//una entrada del nodo
//solo uno de los dos es valido segun el nivel
struct entry 
{
    Rect I; // MBR de esta entrada
    Node* child = nullptr; //child-pointer - solo se usa en nodos internos
    int tuple = -1; // tuple-identifier - solo se usa en hojas
};

//un nodo el R-Tree
// level == 0 es hoja
// level > 0 es nodo interno
//todas las hojas  estan al mismo nivel 
// el vector entries puede tener m y M elementos excepto la raiz
// para entender mejor dentro de cada nodo (carpeta) hay una lista de elementos llamados entries,
// pensemos en cada entry como una etiqueta o tarjetita que esta pegada dentro de nuestr carpeta 
// dependiendo si la caja es una hoja o un nodo interno, la tarjetita dice cosas distintas
// si estamos en una hoja, la tarjetita tiene las coordenadas del obj (rect) y el nro de identificación del obj real (tuple)
// si estas en un nodo interno: la tarjetiva tiene un rectangulo que envuelve a todo lo que esta abajo (rect) y una flecha o puntero (child) que apunta a otra cada mas pequeña que vuelga de ella
//para que el arbol no se deforme y funcione rapido, guttman puso reglas estrictas para estas caja
//1. regla del limite de elementos: una caja no puede tener cualquier cantidad de cosas (M: max de tarjetifas que caben fisicamente en una pagina de disco o en la cada, por ejempllo, si tenemos un max de 50 y metemos 51, la caja revienta y hay que hacer un split osea partirla en 2), m es el min de elementos que debe tener una caja, por ejemplo al menos 2 o la mitad, si una caja se queda con muy poquitos elementos porque borraste cosas, el arbol la desmantela para no desperdiciar espacio, la unica excepcion a esta regla es la caja principal o raiz
//2. Regla de linea de meta (level): todas las hojas (las cjas del fondo, las que tienen los datos reales) tienen que estar obligatoriamente a la misma profundidad (al mismo nivel), no puede haber una hoja flotando a medio camino, todas tocan el suele del arbol al mismo tiempo

struct Node 
{
    explicit Node(int lvl) : level (lvl) {} //crear una caja nueva y asignarle su nivel, que tan arriba o abajo va a estar en el arbol
    int level;
    vector <entry> entries;
    bool isLeaf() const {return level == 0;}
};

//3 coveringRect - dos versione
// estas dos funciones caldulan el MBR minimo que cubre un conjunto de entradas

//Version 1 - a partir de un nodo, calcular el rectangulo minimo contenedor (MBR) que envuelve por completo y de forma ajustada a todas las entradas que se encuentran dentro de un nodo especifico
 //se usa mucho en AdjustTree, actualizar el MBR del padre tmb CondenseTree, cuando se crea una nueva raiz
Rect coveringRect (const Node* n)
{
    Rect r = n -> entries[0].I; // tomamos la primera entrada como punto de partida
    for(const auto& e : n ->entries) r = combine (r, e.I); //vamos combinando con el resto de las entradas del nodo
    return r; // devolver el rectangulo contenedor final
}
//version 2 - a  partir de un vector de entradas
// se usa principalmente dentro de los algoritmos de SSpliy (para caluclar el areda de cada grupo G1 y G2)
//herramienta de apoyo que trabaja con listas temporales de entradas cuando el arbol esta recalculando divisiones
Rect converingRect (const vector <entry>& g)
{
    Rect r = g[0].I;
    for (const auto& e : g) r = combine (r, e.I);
    return r;
}

//algoritmos de division de nodos 
//enumeracion: proposito es crear un selector de opciones para indicaele al rtree cual de los tres metodos diferentes definidos se debe de utilizar para dividir un nodo cuando este se llena por completo
//exhaustive: prueba todas las combinaciones posibles de distribucion de las entradas, para encontrar la division perfecta (menor area posible), pero se vuelve extremadamente lento a medida que el nodo es mas grande
//quadratic: Un metodo intermedio muy popular que calcula costos cuadratiicos para ir agrupando los elementos de forma inteligente sin tardar una eternidad 
//linear: el metodo mas rapido de todos (de costo lineal), utiliza los extremos de las dimensiones para separar los rectangulos con un rendimiento excelente
enum class SplitAlgorithm {Exhaustive, Quadratic, Linear};


/*
Propiedades de RTree
1. Todo nodo hoja tiene entre m y M registros, salvo que sea la raiz
2- Cada registro (I, tuple) de una hoja: I es el menor rectangulo que contiene al obj
3. Todo nodo interno tiene entre m y M hijos, salvo que sea la raiz
4. Cada entrada (I, child) de un nodo interno: I es el menor rectangulo que contiene a los rectangulos del nodo hijo
5. La raiz tiene al menos dos hijos salvo que sea hoja
6. Todas las hoajs estan en el mismo nivel

*/

class RTree
{
    public:
        RTree(int maxEntries, int minEntries, SplitAlgorithm splitAlg) 
        : M(maxEntries), m(minEntries), alg(splitAlg), root(new  Node(0))
        {
            if(m < 2 || m > M / 2) throw invalid_argument ("Se requiere 2 <= m <= M/2");
            if(alg == SplitAlgorithm::Exhaustive && M > 20) throw invalid_argument("el algoritmo exhaustivo solo es viable con M pequeño");
        }  
        ~RTree(){ destroySubtree(root);}
        RTree (const RTree&) = delete;
        RTree& operator = (const RTree&) = delete;


        
    //  3.1 Search
    vector<int> search(const Rect& S) const {
        // TODO: validar S, crear el vector resultado, llamar a la search privada
        return {};
    }

    //  3.2 Insert
    void insert(const Rect& I, int tuple) {
        // TODO: validar I y llamar a insertEntry en el nivel 0
    }

    //  3.3 Delete
    bool remove(const Rect& I, int tuple) {
        // D1. [Encontrar el nodo que contiene el registro]
        // D2. [Eliminar el registro]
        // D3. [Propagar cambios]
        // D4. [Acortar el árbol]
        return false;
    }

    //  3.4 Update
    bool update(const Rect& oldI, const Rect& newI, int tuple) {
        // TODO
        return false;
    }

    int height() const { return root->level + 1; }

    int size() const { return countRecords(root); }

    string checkProperties() const { return checkNode(root, true); }

    void print() const {
        cout << "(altura = " << height() << ")\n";
        print(root, 0);
    }



    private:
        const int m;
        const int M;
        const SplitAlgorithm alg;
        Node* root; 
        //  3.1 Search
        void search(const Node* T, const Rect& S, vector<int>& out) const {
            // S1. [Buscar en subárboles]
            // S2. [Buscar en el nodo hoja]
        }

        //  3.2 Insert
        //  'level' = nivel destino: 0 para registros nuevos; > 0 al reinsertar (CT6)
        void insertEntry(const Entry& E, int level) {
            // I1. [Encontrar posición para el nuevo registro]
            // I2. [Agregar el registro al nodo hoja]
            // I3. [Propagar cambios hacia arriba]
            // I4. [Hacer crecer el árbol en altura]
        }

        // Devuelve el camino desde la raíz hasta el nodo elegido
        vector<Node*> chooseLeaf(const Rect& I, int level) const {
            // CL1. [Inicializar]
            // CL2. [Verificar hoja]
            // CL3. [Elegir subárbol]
            // CL4. [Descender hasta llegar a una hoja]
            return {};
        }

        // Devuelve el hermano de la raíz si la raíz se dividió, o nullptr
        Node* adjustTree(const vector<Node*>& path, Node* LL) {
            // AT1. [Inicializar]
            // AT2. [Verificar si terminó]
            // AT3. [Ajustar el rectángulo en la entrada del padre]
            // AT4. [Propagar el split hacia arriba]
            // AT5. [Subir al siguiente nivel]
            return nullptr;
        }

        //  3.3 Delete
        // Deja en 'path' el camino desde la raíz hasta la hoja
        bool findLeaf(Node* T, const Entry& E, vector<Node*>& path) {
            // FL1. [Buscar en subárboles]
            // FL2. [Buscar en el nodo hoja]
            return false;
        }

        void condenseTree(const vector<Node*>& path) {
            // CT1. [Inicializar]
            // CT2. [Encontrar la entrada en el padre]
            // CT3. [Eliminar nodo con pocas entradas]
            // CT4. [Ajustar el rectángulo]
            // CT5. [Subir un nivel]
            // CT6. [Reinsertar entradas huérfanas]
        }

        //  3.5 Node Splitting
        //  N se queda con el grupo 1 y se devuelve un nodo nuevo con el grupo 2
        Node* splitNode(Node* N) {
            // TODO: elegir el algoritmo según 'alg', repartir en G1 y G2
            return nullptr;
        }

        // 3.5.1 Exhaustive Algorithm
        void exhaustiveSplit(const vector<Entry>& E, vector<Entry>& G1, vector<Entry>& G2) {
            // TODO
        }

        // 3.5.2 Quadratic Split  y  3.5.3 Linear Split
        //  linear = false -> Quadratic Split;  linear = true -> Linear Split
        void splitWithSeeds(vector<Entry> E, vector<Entry>& G1, vector<Entry>& G2, bool linear) {
            // QS1. [Elegir la primera entrada de cada grupo]
            // QS2. [Verificar si terminó]
            // QS3. [Seleccionar entrada a asignar]
        }

        void pickSeeds(const vector<Entry>& E, int& s1, int& s2) const {
            s1 = 0; s2 = 1;
            // PS1. [Calcular la ineficiencia de agrupar entradas]
            // PS2. [Elegir el par más derrochador]
        }

        int pickNext(const vector<Entry>& E, const Rect& R1, const Rect& R2) const {
            // PN1. [Determinar el costo de poner cada entrada en cada grupo]
            // PN2. [Encontrar la entrada con mayor preferencia por un grupo]
            return 0;
        }

        void linearPickSeeds(const vector<Entry>& E, int& s1, int& s2) const {
            s1 = 0; s2 = 1;
            // LPS1. [Encontrar rectángulos extremos en todas las dimensiones]
            // LPS2. [Ajustar por la forma del conjunto de rectángulos]
            // LPS3. [Seleccionar el par más extremo]
        }

        int linearPickNext(const vector<Entry>&) const {
            // TODO
            return 0;
        }


        //  Utilidades

        // Entrada de P que apunta al nodo hijo N  (E_N en el paper)
        static Entry& entryFor(Node* P, Node* N) {
            for (auto& e : P->entries)
                if (e.child == N) return e;
            throw logic_error("el nodo no es hijo de P");
        }

        int countRecords(const Node* n) const {
            if (n->isLeaf()) return (int)n->entries.size();
            int total = 0;
            for (const auto& e : n->entries) total += countRecords(e.child);
            return total;
        }

        string checkNode(const Node* n, bool isRoot) const {
            int k = (int)n->entries.size();
            if (k > M)
                return "un nodo tiene mas de M entradas";
            if (!isRoot && k < m)
                return "propiedad (1)/(3): un nodo no raiz tiene menos de m entradas";
            if (isRoot && !n->isLeaf() && k < 2)
                return "propiedad (5): la raiz interna tiene menos de dos hijos";
            for (const auto& e : n->entries) {
                if (n->isLeaf()) {
                    if (e.child) return "una entrada de hoja tiene puntero a hijo";
                    continue;
                }
                if (!e.child) return "una entrada interna no tiene hijo";
                // Los niveles bajan de a uno hasta 0 => todas las hojas al mismo nivel
                if (e.child->level != n->level - 1)
                    return "propiedad (6): las hojas no estan todas al mismo nivel";
                if (e.child->entries.empty())
                    return "un nodo hijo esta vacio";
                if (!(e.I == coveringRect(e.child)))
                    return "propiedad (4): el rectangulo de una entrada no es el minimo que cubre a su hijo";
                string err = checkNode(e.child, false);
                if (!err.empty()) return err;
            }
            return "";
        }

        static void destroySubtree(Node* n) {
            if (!n->isLeaf())
                for (auto& e : n->entries) destroySubtree(e.child);
            delete n;
        }

        void print(const Node* n, int depth) const {
            string indent(depth * 4, ' ');
            cout << indent << (n->isLeaf() ? "Hoja" : "Nodo interno")
                << " (nivel " << n->level << ", "
                << n->entries.size() << " entradas)\n";
            for (const auto& e : n->entries) {
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


int main ()
{
    return 0;
}
