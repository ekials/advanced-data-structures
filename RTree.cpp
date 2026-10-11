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

class RTree()
{
    
};


int main ()
{
    return 0;
}
