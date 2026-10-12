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






class RTree()
{
    
};


int main ()
{
    return 0;
}
