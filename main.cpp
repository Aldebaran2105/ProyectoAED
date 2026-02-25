#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QKeyEvent>
#include <QTimer>
#include <QPaintEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QFont>
#include <QRandomGenerator>
#include <QPixmap>
#include <QImage>
#include <QImageReader>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <cmath>
#include <algorithm>
#include <vector>

const int TAM_TABLERO = 15;
const int VEL_JUGADOR = 1;
const float VEL_ENEMIGO_ESPECIAL = 0.6f;
const float VEL_ENEMIGO_NORMAL = 0.6f;
const int MAX_ENEMIGOS = 6;
const int MAX_FRUTAS = 30;
const float PROB_PERSECUCION = 0.8f;

// ============= QuadTree (partición espacial para colisiones) =============
struct PointQT {
    double x, y;
    PointQT() : x(0), y(0) {}
    PointQT(double _x, double _y) : x(_x), y(_y) {}
    bool operator==(const PointQT& o) const { return x == o.x && y == o.y; }
};

struct RectQT {
    double x, y, ancho, alto;
    RectQT() : x(0), y(0), ancho(0), alto(0) {}
    RectQT(double _x, double _y, double _w, double _h) : x(_x), y(_y), ancho(_w), alto(_h) {}
    bool contiene(const PointQT& p) const {
        return p.x >= x && p.x <= x + ancho && p.y >= y && p.y <= y + alto;
    }
    bool intersecta(const RectQT& r) const {
        return !(r.x > x + ancho || r.x + r.ancho < x || r.y > y + alto || r.y + r.alto < y);
    }
};

class QuadTree {
    static const int CAPACIDAD = 4;
    static const int PROF_MAX = 6;

    struct Nodo {
        double x1, y1, x2, y2;
        bool esHoja = true;
        std::vector<PointQT> puntos;
        Nodo *nw = nullptr, *ne = nullptr, *sw = nullptr, *se = nullptr;
        int profundidad = 0;

        Nodo(double _x1, double _y1, double _x2, double _y2, int prof)
            : x1(_x1), y1(_y1), x2(_x2), y2(_y2), profundidad(prof) {}

        ~Nodo() {
            delete nw; delete ne; delete sw; delete se;
        }

        bool contiene(double px, double py) const {
            return px >= x1 && px <= x2 && py >= y1 && py <= y2;
        }

        void subdividir() {
            double mx = (x1 + x2) / 2.0, my = (y1 + y2) / 2.0;
            nw = new Nodo(x1, y1, mx, my, profundidad + 1);
            ne = new Nodo(mx, y1, x2, my, profundidad + 1);
            sw = new Nodo(x1, my, mx, y2, profundidad + 1);
            se = new Nodo(mx, my, x2, y2, profundidad + 1);
            for (const auto& p : puntos) {
                if (nw->contiene(p.x, p.y)) nw->puntos.push_back(p);
                else if (ne->contiene(p.x, p.y)) ne->puntos.push_back(p);
                else if (sw->contiene(p.x, p.y)) sw->puntos.push_back(p);
                else se->puntos.push_back(p);
            }
            puntos.clear();
            esHoja = false;
        }
    };

    Nodo* raiz = nullptr;
    double boundX1 = 0, boundY1 = 0, boundX2 = TAM_TABLERO, boundY2 = TAM_TABLERO;

    bool insertar(Nodo* n, const PointQT& p) {
        if (!n->contiene(p.x, p.y)) return false;
        if (n->esHoja) {
            if (static_cast<int>(n->puntos.size()) < CAPACIDAD || n->profundidad >= PROF_MAX) {
                n->puntos.push_back(p);
                return true;
            }
            n->subdividir();
        }
        if (n->nw->contiene(p.x, p.y)) return insertar(n->nw, p);
        if (n->ne->contiene(p.x, p.y)) return insertar(n->ne, p);
        if (n->sw->contiene(p.x, p.y)) return insertar(n->sw, p);
        return insertar(n->se, p);
    }

    void consultarRango(Nodo* n, const RectQT& rango, std::vector<PointQT>& out) const {
        if (!n) return;
        RectQT nodoRect(n->x1, n->y1, n->x2 - n->x1, n->y2 - n->y1);
        if (!nodoRect.intersecta(rango)) return;
        if (n->esHoja) {
            for (const auto& p : n->puntos)
                if (rango.contiene(p)) out.push_back(p);
            return;
        }
        consultarRango(n->nw, rango, out);
        consultarRango(n->ne, rango, out);
        consultarRango(n->sw, rango, out);
        consultarRango(n->se, rango, out);
    }

public:
    QuadTree(double x1, double y1, double x2, double y2)
        : boundX1(x1), boundY1(y1), boundX2(x2), boundY2(y2) {
        raiz = new Nodo(x1, y1, x2, y2, 0);
    }

    ~QuadTree() { delete raiz; }

    void limpiar() {
        delete raiz;
        raiz = new Nodo(boundX1, boundY1, boundX2, boundY2, 0);
    }

    void reiniciar(double x1, double y1, double x2, double y2) {
        boundX1 = x1; boundY1 = y1; boundX2 = x2; boundY2 = y2;
        delete raiz;
        raiz = new Nodo(x1, y1, x2, y2, 0);
    }

    bool insertar(double x, double y) { return insertar(raiz, PointQT(x, y)); }

    void consultarRango(double x, double y, double w, double h, std::vector<PointQT>& out) const {
        out.clear();
        consultarRango(raiz, RectQT(x, y, w, h), out);
    }

    bool hayPuntosEnCelda(double cx, double cy) const {
        std::vector<PointQT> res;
        consultarRango(cx, cy, 1.0, 1.0, res);
        return !res.empty();
    }
};

enum Direccion { Arriba, Abajo, Izquierda, Derecha, Ninguna };
enum TipoCelda { Vacia, Muro, Hielo, Uva, Platano, FrutaNormal, FrutaCongelada };
enum TipoEnemigo { Normal, Especial };

struct Posicion {
    float x = 0, y = 0;
    Posicion() = default;
    Posicion(float _x, float _y) : x(_x), y(_y) {}
    int celdaX() const { return static_cast<int>(std::round(x)); }
    int celdaY() const { return static_cast<int>(std::round(y)); }
};

struct Jugador {
    Posicion pos;
    Direccion dir = Ninguna;
    float velocidad = VEL_JUGADOR;
    bool vivo = true;
    int frutas_recogidas = 0;

    void mover(Direccion d) {
        dir = d;
        switch (d) {
            case Arriba:  pos.y -= velocidad; break;
            case Abajo:   pos.y += velocidad; break;
            case Izquierda: pos.x -= velocidad; break;
            case Derecha:  pos.x += velocidad; break;
            default: break;
        }
    }
    void detener() { dir = Ninguna; }
};

struct Enemigo {
    Posicion pos;
    Direccion dir = Abajo;
    TipoEnemigo tipo = Normal;
    float velocidad = VEL_ENEMIGO_NORMAL;
    bool vivo = true;
    int ticksParaCambiar = 0;

    Enemigo() = default;
    Enemigo(float x, float y, TipoEnemigo t) : pos(x, y), tipo(t) {
        velocidad = (t == Especial) ? VEL_ENEMIGO_ESPECIAL : VEL_ENEMIGO_NORMAL;
    }
    void mover() {
        if (!vivo) return;
        switch (dir) {
            case Arriba:  pos.y -= velocidad; break;
            case Abajo:   pos.y += velocidad; break;
            case Izquierda: pos.x -= velocidad; break;
            case Derecha:  pos.x += velocidad; break;
            default: break;
        }
    }
};

struct Fruta {
    Posicion pos;
    TipoCelda tipoFruta = Uva;
    bool recogida = false;
    bool congelada = false;
    Fruta() = default;
    Fruta(float x, float y, TipoCelda t) : pos(x, y), tipoFruta(t) {}
};

class Mapa {
    TipoCelda casilla[TAM_TABLERO][TAM_TABLERO];
public:
    void inicializar() {
        for (int i = 0; i < TAM_TABLERO; i++)
            for (int j = 0; j < TAM_TABLERO; j++)
                casilla[i][j] = Vacia;
        for (int k = 0; k < TAM_TABLERO; k++) {
            casilla[0][k] = casilla[TAM_TABLERO - 1][k] = Muro;
            casilla[k][0] = casilla[k][TAM_TABLERO - 1] = Muro;
        }
    }

    void ponerMurosAleatorios() {
        int celdasInterior = (TAM_TABLERO - 2) * (TAM_TABLERO - 2);
        int numMuros = 8 + (QRandomGenerator::global()->bounded(10));
        numMuros = std::min(numMuros, celdasInterior / 4);
        int puestos = 0;
        while (puestos < numMuros) {
            int r = 1 + QRandomGenerator::global()->bounded(TAM_TABLERO - 2);
            int c = 1 + QRandomGenerator::global()->bounded(TAM_TABLERO - 2);
            if (casilla[r][c] == Vacia) {
                casilla[r][c] = Muro;
                puestos++;
            }
        }
    }

    TipoCelda obtenerCelda(int fila, int col) const {
        if (fila < 0 || fila >= TAM_TABLERO || col < 0 || col >= TAM_TABLERO)
            return Muro;
        return casilla[fila][col];
    }

    void crearHielo(int fila, int col) {
        if (fila >= 1 && fila < TAM_TABLERO - 1 && col >= 1 && col < TAM_TABLERO - 1 && casilla[fila][col] == Vacia)
            casilla[fila][col] = Hielo;
    }
    void romperHielo(int fila, int col) {
        if (fila >= 0 && fila < TAM_TABLERO && col >= 0 && col < TAM_TABLERO && casilla[fila][col] == Hielo)
            casilla[fila][col] = Vacia;
    }

    bool sePuedePasar(int fila, int col) const {
        TipoCelda t = obtenerCelda(fila, col);
        return (t == Vacia || t == Uva || t == Platano);
    }
    bool puedePasarEspecial(int fila, int col) const {
        if (fila < 0 || fila >= TAM_TABLERO || col < 0 || col >= TAM_TABLERO)
            return false;
        TipoCelda t = casilla[fila][col];
        return (t == Vacia || t == Uva || t == Platano || t == Hielo);
    }

    bool celdaVaciaParaSpawn(int fila, int col) const {
        return obtenerCelda(fila, col) == Vacia;
    }

    void ponerFrutas(Fruta* frutas, int& numFrutas, TipoCelda tipo, int cantidad) {
        int colocadas = 0;
        int intentos = 0;
        while (colocadas < cantidad && intentos < 500) {
            intentos++;
            int r = 1 + QRandomGenerator::global()->bounded(TAM_TABLERO - 2);
            int c = 1 + QRandomGenerator::global()->bounded(TAM_TABLERO - 2);
            if (casilla[r][c] != Vacia) continue;
            bool muyCerca = false;
            for (int i = 0; i < numFrutas; i++) {
                if (frutas[i].recogida) continue;
                int fr = frutas[i].pos.celdaY(), fc = frutas[i].pos.celdaX();
                if (std::abs(fr - r) <= 1 && std::abs(fc - c) <= 1) { muyCerca = true; break; }
            }
            if (!muyCerca) {
                frutas[numFrutas++] = Fruta(static_cast<float>(c), static_cast<float>(r), tipo);
                casilla[r][c] = tipo;
                colocadas++;
            }
        }
    }
};

class LogicaEnemigo {
public:
    static bool hayFrutaCongeladaEn(int fila, int col, const Fruta* frutas, int numFrutas) {
        for (int i = 0; i < numFrutas; i++) {
            if (frutas[i].recogida) continue;
            if (frutas[i].pos.celdaY() == fila && frutas[i].pos.celdaX() == col && frutas[i].congelada)
                return true;
        }
        return false;
    }

    static bool puedePasarCelda(int fila, int col, const Enemigo& e, const Mapa& mapa, const Fruta* frutas, int numFrutas) {
        bool mapaOk = (e.tipo == Especial) ? mapa.puedePasarEspecial(fila, col) : mapa.sePuedePasar(fila, col);
        return mapaOk && !hayFrutaCongeladaEn(fila, col, frutas, numFrutas);
    }

    static void elegirDireccionHaciaJugador(Enemigo& e, const Jugador& jug, const Mapa& mapa, const Fruta* frutas, int numFrutas) {
        int jr = jug.pos.celdaY(), jc = jug.pos.celdaX();
        int er = e.pos.celdaY(), ec = e.pos.celdaX();
        int dr = jr - er, dc = jc - ec;
        Direccion preferida = Ninguna;
        if (std::abs(dr) >= std::abs(dc)) {
            preferida = (dr > 0) ? Abajo : Arriba;
        } else {
            preferida = (dc > 0) ? Derecha : Izquierda;
        }
        int nr = er, nc = ec;
        switch (preferida) {
            case Arriba: nr--; break;
            case Abajo: nr++; break;
            case Izquierda: nc--; break;
            case Derecha: nc++; break;
            default: break;
        }
        bool puede = puedePasarCelda(nr, nc, e, mapa, frutas, numFrutas);
        if (puede) {
            e.dir = preferida;
            return;
        }
        Direccion otra = (std::abs(dr) >= std::abs(dc)) ? (dc > 0 ? Derecha : Izquierda) : (dr > 0 ? Abajo : Arriba);
        nr = er; nc = ec;
        switch (otra) {
            case Arriba: nr--; break;
            case Abajo: nr++; break;
            case Izquierda: nc--; break;
            case Derecha: nc++; break;
            default: break;
        }
        puede = puedePasarCelda(nr, nc, e, mapa, frutas, numFrutas);
        if (puede) { e.dir = otra; return; }
        int d = QRandomGenerator::global()->bounded(4);
        e.dir = static_cast<Direccion>(d);
    }

    static void actualizar(Enemigo& e, const Jugador& jug, Mapa& mapa, const Fruta* frutas, int numFrutas) {
        if (!e.vivo) return;
        e.ticksParaCambiar--;
        if (e.ticksParaCambiar <= 0) {
            e.ticksParaCambiar = 5 + QRandomGenerator::global()->bounded(15);
            if (QRandomGenerator::global()->generateDouble() < PROB_PERSECUCION)
                elegirDireccionHaciaJugador(e, jug, mapa, frutas, numFrutas);
            else {
                int d = QRandomGenerator::global()->bounded(4);
                e.dir = static_cast<Direccion>(d);
            }
        }
        Posicion ant = e.pos;
        e.mover();
        int nr = e.pos.celdaY(), nc = e.pos.celdaX();
        bool pasar = puedePasarCelda(nr, nc, e, mapa, frutas, numFrutas);
        if (!pasar) {
            e.pos = ant;
            int d = QRandomGenerator::global()->bounded(4);
            e.dir = static_cast<Direccion>(d);
        }
    }

    static bool colisionConJugador(const Enemigo& e, const Jugador& jug) {
        if (!jug.vivo) return false;
        return e.pos.celdaX() == jug.pos.celdaX() && e.pos.celdaY() == jug.pos.celdaY();
    }
};

class CongelarDescongelar {
public:
    static void congelar(Jugador& jug, Mapa& mapa, Enemigo* enemigos, int numEnemigos, Fruta* frutas, int numFrutas) {
        int jr = jug.pos.celdaY(), jc = jug.pos.celdaX();
        int dr = 0, dc = 0;
        switch (jug.dir) {
            case Arriba: dr = -1; break;
            case Abajo: dr = 1; break;
            case Izquierda: dc = -1; break;
            case Derecha: dc = 1; break;
            default: dr = -1; break;
        }
        int r = jr + dr, c = jc + dc;
        while (r >= 1 && r < TAM_TABLERO - 1 && c >= 1 && c < TAM_TABLERO - 1) {
            if (mapa.obtenerCelda(r, c) == Muro) break;
            bool hayEnemigo = false;
            for (int i = 0; i < numEnemigos; i++)
                if (enemigos[i].vivo && enemigos[i].pos.celdaY() == r && enemigos[i].pos.celdaX() == c) { hayEnemigo = true; break; }
            if (hayEnemigo) break;
            for (int i = 0; i < numFrutas; i++)
                if (!frutas[i].recogida && frutas[i].pos.celdaY() == r && frutas[i].pos.celdaX() == c)
                    frutas[i].congelada = true;
            if (mapa.obtenerCelda(r, c) == Vacia) mapa.crearHielo(r, c);
            r += dr; c += dc;
        }
    }

    static void descongelar(Jugador& jug, Mapa& mapa, Enemigo*, int, Fruta* frutas, int numFrutas) {
        int jr = jug.pos.celdaY(), jc = jug.pos.celdaX();
        int dr = 0, dc = 0;
        switch (jug.dir) {
            case Arriba: dr = -1; break;
            case Abajo: dr = 1; break;
            case Izquierda: dc = -1; break;
            case Derecha: dc = 1; break;
            default: dr = -1; break;
        }
        int r = jr + dr, c = jc + dc;
        while (r >= 1 && r < TAM_TABLERO - 1 && c >= 1 && c < TAM_TABLERO - 1) {
            if (mapa.obtenerCelda(r, c) == Muro) break;
            for (int i = 0; i < numFrutas; i++)
                if (!frutas[i].recogida && frutas[i].pos.celdaY() == r && frutas[i].pos.celdaX() == c)
                    frutas[i].congelada = false;
            if (mapa.obtenerCelda(r, c) == Hielo) mapa.romperHielo(r, c);
            r += dr; c += dc;
        }
    }
};

enum EstadoJuego { Menu, Jugando, Ganaste, Perdiste };

class Juego {
public:
    EstadoJuego estado = Menu;
    int nivel = 1;
    Jugador jugador;
    bool esBot = false;
    Mapa mapa;
    Enemigo enemigos[MAX_ENEMIGOS];
    int numEnemigos = 0;
    Fruta frutas[MAX_FRUTAS];
    int numFrutas = 0;
    int uvasRestantes = 0;
    int platanosRestantes = 0;
    QuadTree quadTreeEnemigos{0.0, 0.0, static_cast<double>(TAM_TABLERO), static_cast<double>(TAM_TABLERO)};
    int ticksDesdeInicio = 0;
    Direccion ultimaDirBot = Ninguna;
    int pasosBloqueadoBot = 0;

    void tickBot() {
        if (!esBot || estado != Jugando || !jugador.vivo) return;

        Posicion ant = jugador.pos;
        if (pasosBloqueadoBot >= 2) {
            int jr = ant.celdaY();
            int jc = ant.celdaX();
            Direccion dirs[4] = {Arriba, Abajo, Izquierda, Derecha};
            for (int k = 0; k < 4; ++k) {
                int r = QRandomGenerator::global()->bounded(4);
                std::swap(dirs[k], dirs[r]);
            }
            for (Direccion d : dirs) {
                int nr = jr, nc = jc;
                switch (d) {
                    case Arriba:    nr--; break;
                    case Abajo:     nr++; break;
                    case Izquierda: nc--; break;
                    case Derecha:   nc++; break;
                    default: break;
                }
                if (nr < 0 || nr >= TAM_TABLERO || nc < 0 || nc >= TAM_TABLERO) continue;
                if (!mapa.sePuedePasar(nr, nc)) continue;
                if (hayFrutaCongeladaEn(nr, nc)) continue;
                moverJugador(d);
                ultimaDirBot = d;
                break;
            }
            } else {
            int jr = jugador.pos.celdaY();
            int jc = jugador.pos.celdaX();
            int mejorIdx = -1;
            int mejorDist = 1e9;
            for (int i = 0; i < numFrutas; i++) {
                if (frutas[i].recogida) continue;
                int fr = frutas[i].pos.celdaY();
                int fc = frutas[i].pos.celdaX();
                int dist = std::abs(fr - jr) + std::abs(fc - jc);
                if (dist < mejorDist) {
                    mejorDist = dist;
                    mejorIdx = i;
                }
            }
            if (mejorIdx == -1) return;
            int fr = frutas[mejorIdx].pos.celdaY();
            int fc = frutas[mejorIdx].pos.celdaX();
            int dr = fr - jr;
            int dc = fc - jc;
            Direccion d1 = Ninguna, d2 = Ninguna;
            if (std::abs(dr) >= std::abs(dc)) {
                d1 = (dr > 0) ? Abajo : Arriba;
                if (dc != 0) d2 = (dc > 0) ? Derecha : Izquierda;
            } else {
                d1 = (dc > 0) ? Derecha : Izquierda;
                if (dr != 0) d2 = (dr > 0) ? Abajo : Arriba;
            }

            if (d1 != Ninguna) {
                moverJugador(d1);
                if (jugador.pos.celdaX() == ant.celdaX() && jugador.pos.celdaY() == ant.celdaY() && d2 != Ninguna) {
                    moverJugador(d2);
                }
            } else if (d2 != Ninguna) {
                moverJugador(d2);
            }
            ultimaDirBot = jugador.dir;
        }

        int jr = jugador.pos.celdaY();
        int jc = jugador.pos.celdaX();
        if (jr == ant.celdaY() && jc == ant.celdaX()) {
            pasosBloqueadoBot++;
            if (pasosBloqueadoBot > 6) pasosBloqueadoBot = 6;
        } else {
            pasosBloqueadoBot = 0;
        }
    }

    void iniciarNivel(int n) {
        nivel = n;
        estado = Jugando;
        jugador.vivo = true;
        jugador.dir = Ninguna;
        jugador.frutas_recogidas = 0;
        ticksDesdeInicio = 0;
        pasosBloqueadoBot = 0;
        ultimaDirBot = Ninguna;
        mapa.inicializar();
        mapa.ponerMurosAleatorios();
        int pr = TAM_TABLERO / 2, pc = TAM_TABLERO / 2;
        if (mapa.obtenerCelda(pr, pc) != Vacia) {
            for (int d = 1; d < TAM_TABLERO/2; d++) {
                if (mapa.obtenerCelda(pr - d, pc) == Vacia) { pr -= d; break; }
                if (mapa.obtenerCelda(pr + d, pc) == Vacia) { pr += d; break; }
                if (mapa.obtenerCelda(pr, pc - d) == Vacia) { pc -= d; break; }
                if (mapa.obtenerCelda(pr, pc + d) == Vacia) { pc += d; break; }
            }
        }
        jugador.pos = Posicion(static_cast<float>(pc), static_cast<float>(pr));
        numEnemigos = std::min(nivel, MAX_ENEMIGOS);
        for (int i = 0; i < numEnemigos; i++) {
            int er, ec;
            int intentos = 0;
            do {
                // filas altas del mapa (1..3) para evitar spawnear junto al jugador central
                er = 1 + QRandomGenerator::global()->bounded(std::min(3, TAM_TABLERO - 2));
                ec = 1 + QRandomGenerator::global()->bounded(TAM_TABLERO - 2);
                if (++intentos > 200) break;
            } while (!mapa.celdaVaciaParaSpawn(er, ec) ||
                     ocupadoPorOtroEnemigo(ec, er, i) ||
                     (std::abs(er - pr) + std::abs(ec - pc) <= 2));
            enemigos[i] = Enemigo(static_cast<float>(ec), static_cast<float>(er), (i == numEnemigos - 1 && nivel >= 3) ? Especial : Normal);
            enemigos[i].ticksParaCambiar = QRandomGenerator::global()->bounded(10);
        }

        numFrutas = 0;
        int cantUvas = 5 + nivel * 2;
        cantUvas = std::min(cantUvas, 15);
        mapa.ponerFrutas(frutas, numFrutas, Uva, cantUvas);
        uvasRestantes = numFrutas;
        platanosRestantes = 0;
        quadTreeEnemigos.reiniciar(0.0, 0.0, static_cast<double>(TAM_TABLERO), static_cast<double>(TAM_TABLERO));
    }

    bool ocupadoPorOtroEnemigo(int c, int r, int excepto) const {
        for (int i = 0; i < numEnemigos; i++) {
            if (i == excepto) continue;
            if (enemigos[i].pos.celdaX() == c && enemigos[i].pos.celdaY() == r) return true;
        }
        return false;
    }

    bool hayFrutaCongeladaEn(int fila, int col) const {
        for (int i = 0; i < numFrutas; i++) {
            if (frutas[i].recogida) continue;
            if (frutas[i].pos.celdaY() == fila && frutas[i].pos.celdaX() == col && frutas[i].congelada)
                return true;
        }
        return false;
    }

    void moverJugador(Direccion d) {
        if (estado != Jugando || !jugador.vivo) return;
        Posicion ant = jugador.pos;
        jugador.mover(d);
        int fr = jugador.pos.celdaY(), fc = jugador.pos.celdaX();
        TipoCelda t = mapa.obtenerCelda(fr, fc);
        if (t == Muro || t == Hielo || hayFrutaCongeladaEn(fr, fc)) jugador.pos = ant;
    }

    void verFrutas() {
        int pr = jugador.pos.celdaY(), pc = jugador.pos.celdaX();
        for (int i = 0; i < numFrutas; i++) {
            if (frutas[i].recogida || frutas[i].congelada) continue;
            if (frutas[i].pos.celdaY() == pr && frutas[i].pos.celdaX() == pc) {
                frutas[i].recogida = true;
                jugador.frutas_recogidas++;
                if (frutas[i].tipoFruta == Uva) uvasRestantes--;
                else if (frutas[i].tipoFruta == Platano) platanosRestantes--;
            }
        }
    }

    void verSiGano() {
        if (uvasRestantes == 0 && platanosRestantes == 0) estado = Ganaste;
    }

    void actualizar() {
        if (estado != Jugando) return;
        ticksDesdeInicio++;
        if (esBot) tickBot();
        for (int i = 0; i < numEnemigos; i++) {
            if (!enemigos[i].vivo) continue;
            LogicaEnemigo::actualizar(enemigos[i], jugador, mapa, frutas, numFrutas);
        }
        quadTreeEnemigos.limpiar();
        for (int i = 0; i < numEnemigos; i++) {
            if (!enemigos[i].vivo) continue;
            double ex = enemigos[i].pos.celdaX() + 0.5, ey = enemigos[i].pos.celdaY() + 0.5;
            quadTreeEnemigos.insertar(ex, ey);
        }
        int jx = jugador.pos.celdaX(), jy = jugador.pos.celdaY();
        if (ticksDesdeInicio > 1 &&
            quadTreeEnemigos.hayPuntosEnCelda(static_cast<double>(jx), static_cast<double>(jy))) {
            jugador.vivo = false;
            estado = Perdiste;
            return;
        }
        verFrutas();
        verSiGano();
    }

    void congelar() { CongelarDescongelar::congelar(jugador, mapa, enemigos, numEnemigos, frutas, numFrutas); }
    void descongelar() { CongelarDescongelar::descongelar(jugador, mapa, enemigos, numEnemigos, frutas, numFrutas); }
};

static QString rutaSprites() {
    QDir d(QCoreApplication::applicationDirPath());
    // 1) Junto al .exe (cmake-build-debug/SPRITES)
    QString r1 = d.absoluteFilePath("SPRITES");
    if (QFileInfo::exists(r1 + "/quieto/0.png")) return r1;
    // 2) Carpeta del proyecto (PROYECTO/SPRITES)
    if (d.cdUp()) {
        QString r2 = d.absoluteFilePath("SPRITES");
        if (QFileInfo::exists(r2 + "/quieto/0.png")) return r2;
    }
    return r1;
}


class AnimacionJugador {
public:
    enum Tipo { Idle, Caminar, Congelar, RomperHielo, Rip, Ganar };

    QVector<QPixmap> quieto;           // 0-1
    QVector<QVector<QPixmap>> caminar; // [dir][0-7]: arriba=0, abajo=1, izquierda=2, derecha=3
    QVector<QVector<QPixmap>> hacerhielo; // [dir][0-9]
    QVector<QPixmap> romperhielo;      // 2-9
    QVector<QPixmap> rip;              // 0-12
    QVector<QPixmap> ganar;            // 0-5

    bool cargar(const QString& base) {
        qDebug() << "[Sprites] Buscando en:" << base;
        QDir b(base);
        if (!b.exists()) {
            qDebug() << "[Sprites] ERROR: Carpeta no existe";
            return false;
        }

        for (int i = 0; i <= 1; i++) {
            QString ruta = QDir(base).absoluteFilePath("quieto/" + QString::number(i) + ".png");
            QImage img(ruta);
            if (img.isNull()) {
                qDebug() << "[Sprites] ERROR cargando:" << ruta;
                qDebug() << "[Sprites] QImageReader error:" << QImageReader(ruta).errorString();
                return false;
            }
            quieto.append(QPixmap::fromImage(img));
        }

        const char* dirs[] = {"abajo","arriba","derecha","izquierda"};
        caminar.resize(4);
        for (int d = 0; d < 4; d++) {
            QString carpeta = QDir(base).absoluteFilePath(QString("caminar/") + dirs[d]);
            for (int i = 0; i <= 7; i++) {
                QString ruta = QDir(carpeta).absoluteFilePath(QString::number(i) + ".png");
                QImage img(ruta);
                if (img.isNull()) { qDebug() << "[Sprites] ERROR:" << ruta; return false; }
                caminar[d].append(QPixmap::fromImage(img));
            }
        }

        const int hacerhieloFrames[] = {10, 10, 8, 8};
        hacerhielo.resize(4);
        for (int d = 0; d < 4; d++) {
            QString carpeta = QDir(base).absoluteFilePath(QString("hacerhielo/") + dirs[d]);
            for (int i = 0; i < hacerhieloFrames[d]; i++) {
                QString ruta = QDir(carpeta).absoluteFilePath(QString::number(i) + ".png");
                QImage img(ruta);
                if (img.isNull()) { qDebug() << "[Sprites] ERROR:" << ruta; return false; }
                hacerhielo[d].append(QPixmap::fromImage(img));
            }
        }

        for (int i = 2; i <= 9; i++) {
            QString ruta = QDir(base).absoluteFilePath("romperhielo/" + QString::number(i) + ".png");
            QImage img(ruta);
            if (img.isNull()) { qDebug() << "[Sprites] ERROR:" << ruta; return false; }
            romperhielo.append(QPixmap::fromImage(img));
        }

        for (int i = 0; i <= 12; i++) {
            QString ruta = QDir(base).absoluteFilePath("rip/" + QString::number(i) + ".png");
            QImage img(ruta);
            if (img.isNull()) { qDebug() << "[Sprites] ERROR:" << ruta; return false; }
            rip.append(QPixmap::fromImage(img));
        }

        for (int i = 0; i <= 5; i++) {
            QString ruta = QDir(base).absoluteFilePath("ganar/" + QString::number(i) + ".png");
            QImage img(ruta);
            if (img.isNull()) { qDebug() << "[Sprites] ERROR:" << ruta; return false; }
            ganar.append(QPixmap::fromImage(img));
        }
        qDebug() << "[Sprites] OK: Todos los sprites cargados";
        return true;
    }

    int dirToIndex(Direccion d) const {
        switch (d) {
            case Abajo: return 0;
            case Arriba: return 1;
            case Derecha: return 2;
            case Izquierda: return 3;
            default: return 0;
        }
    }

    QPixmap frame(Tipo t, Direccion dir, int frame) const {
        int di = dirToIndex(dir);
        switch (t) {
            case Idle: return quieto.value(frame % quieto.size());
            case Caminar: return caminar.value(di).value(frame % caminar[di].size());
            case Congelar: return hacerhielo.value(di).value(std::min(frame, static_cast<int>(hacerhielo[di].size())-1));
            case RomperHielo: return romperhielo.value(std::min(frame, static_cast<int>(romperhielo.size())-1));
            case Rip: return rip.value(std::min(frame, static_cast<int>(rip.size())-1));
            case Ganar: return ganar.value(std::min(frame, static_cast<int>(ganar.size())-1));
        }
        return quieto.value(0);
    }

    int maxFrame(Tipo t, Direccion dir) const {
        int di = dirToIndex(dir);
        switch (t) {
            case Idle: return quieto.size();
            case Caminar: return caminar.value(di).size();
            case Congelar: return hacerhielo.value(di).size();
            case RomperHielo: return romperhielo.size();
            case Rip: return rip.size();
            case Ganar: return ganar.size();
        }
        return 1;
    }
};

class AnimacionEnemigo {
public:
    QVector<QPixmap> quieto;
    QVector<QVector<QPixmap>> caminar;

    bool cargar(const QString& base) {
        QString carpeta = QDir(base).absoluteFilePath("enemigos");
        if (!QDir(carpeta).exists()) return false;

        QString rutaQuieto = QDir(carpeta).absoluteFilePath("quieto/0.png");
        QImage imgQ(rutaQuieto);
        if (imgQ.isNull()) return false;
        quieto.append(QPixmap::fromImage(imgQ));

        const char* dirs[] = {"abajo", "arriba", "derecha", "izquierda"};
        caminar.resize(4);
        for (int d = 0; d < 4; d++) {
            QString sub = QDir(carpeta).absoluteFilePath(dirs[d]);
            for (int i = 0; i <= 7; i++) {
                QString ruta = QDir(sub).absoluteFilePath(QString::number(i) + ".png");
                QImage img(ruta);
                if (img.isNull()) return false;
                caminar[d].append(QPixmap::fromImage(img));
            }
        }
        return true;
    }

    int dirToIndex(Direccion d) const {
        switch (d) {
            case Abajo: return 0;
            case Arriba: return 1;
            case Derecha: return 2;
            case Izquierda: return 3;
            default: return 0;
        }
    }

    QPixmap frame(Direccion dir, int frame, bool moviendo) const {
        int di = dirToIndex(dir);
        if (moviendo && di >= 0 && di < caminar.size())
            return caminar[di].value(frame % caminar[di].size());
        return quieto.value(0);
    }

    int maxFrame(Direccion dir) const {
        int di = dirToIndex(dir);
        if (di >= 0 && di < caminar.size())
            return caminar[di].size();
        return 1;
    }
};

class AnimacionBloques {
public:
    QPixmap hielo, nieve, nieve2, bordes;

    bool cargar(const QString& base) {
        QString carpeta = QDir(base).absoluteFilePath("bloques");
        if (!QDir(carpeta).exists()) return false;
        QImage ih(QDir(carpeta).absoluteFilePath("hielo.png"));
        QImage in1(QDir(carpeta).absoluteFilePath("nieve.png"));
        QImage in2(QDir(carpeta).absoluteFilePath("nieve2.png"));
        QImage ib(QDir(carpeta).absoluteFilePath("bordes.png"));
        if (ih.isNull() || in1.isNull() || in2.isNull() || ib.isNull()) return false;
        hielo = QPixmap::fromImage(ih);
        nieve = QPixmap::fromImage(in1);
        nieve2 = QPixmap::fromImage(in2);
        bordes = QPixmap::fromImage(ib);
        return true;
    }
};

class AnimacionFruta {
public:
    QPixmap sprite;

    bool cargar(const QString& base) {
        QString ruta = QDir(base).absoluteFilePath("frutas/0.png");
        QImage img(ruta);
        if (img.isNull()) return false;
        sprite = QPixmap::fromImage(img);
        return true;
    }
};

class WidgetTablero : public QWidget {
    Juego* juego = nullptr;
    QTimer* timer = nullptr;
    int celdaPx = 32;
    AnimacionJugador sprites;
    bool spritesCargados = false;
    AnimacionEnemigo spritesEnemigo;
    bool spritesEnemigosCargados = false;
    AnimacionFruta spriteFruta;
    bool spritesFrutasCargados = false;
    AnimacionBloques spriteBloques;
    bool spritesBloquesCargados = false;
    AnimacionJugador::Tipo estadoAnim = AnimacionJugador::Idle;
    int frameAnim = 0;
    Direccion ultimaDir = Abajo;
    int tickAnim = 0;
    bool animacionFinTerminada = false;

public:
    explicit WidgetTablero(Juego* j, QWidget* parent = nullptr) : QWidget(parent), juego(j) {
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(400, 400);
        setStyleSheet("WidgetTablero { background-color: #2a2635; }");
        qDebug() << "[Sprites] Directorio exe:" << QCoreApplication::applicationDirPath();
        spritesCargados = sprites.cargar(rutaSprites());
        spritesEnemigosCargados = spritesEnemigo.cargar(rutaSprites());
        spritesFrutasCargados = spriteFruta.cargar(rutaSprites());
        spritesBloquesCargados = spriteBloques.cargar(rutaSprites());
        if (!spritesCargados) qDebug() << "[Sprites] Usando fallback (circulos)";
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            if (!juego) return;
            if (juego->estado == Jugando) {
                juego->actualizar();
                if (juego->jugador.vivo == false)
                    estadoAnim = AnimacionJugador::Rip;
            }
            avanceAnimacion();
            update();
        });
    }

    void avanceAnimacion() {
        tickAnim++;
        if (!juego) return;
        if (!spritesCargados) {
            if (juego->jugador.dir != Ninguna) ultimaDir = juego->jugador.dir;
            if ((juego->estado == Ganaste || juego->estado == Perdiste) && tickAnim > 12) animacionFinTerminada = true;
            return;
        }
        if (juego->estado == Ganaste) {
            if (estadoAnim != AnimacionJugador::Ganar) { estadoAnim = AnimacionJugador::Ganar; frameAnim = 0; animacionFinTerminada = false; }
            int mx = sprites.maxFrame(AnimacionJugador::Ganar, Abajo);
            if (frameAnim < mx - 1) frameAnim++; else animacionFinTerminada = true;
            return;
        }
        if (juego->estado == Perdiste) {
            if (estadoAnim != AnimacionJugador::Rip) { estadoAnim = AnimacionJugador::Rip; frameAnim = 0; animacionFinTerminada = false; }
            int mx = sprites.maxFrame(AnimacionJugador::Rip, Abajo);
            if (frameAnim < mx - 1) frameAnim++; else animacionFinTerminada = true;
            return;
        }
        if (estadoAnim == AnimacionJugador::Congelar || estadoAnim == AnimacionJugador::RomperHielo) {
            int mx = (estadoAnim == AnimacionJugador::Congelar)
                ? sprites.maxFrame(AnimacionJugador::Congelar, ultimaDir)
                : sprites.maxFrame(AnimacionJugador::RomperHielo, Abajo);
            if (frameAnim < mx - 1) { if (tickAnim % 2 == 0) frameAnim++; }
            else { estadoAnim = AnimacionJugador::Idle; frameAnim = 0; }
            return;
        }
        if (juego->jugador.dir != Ninguna) {
            ultimaDir = juego->jugador.dir;
            estadoAnim = AnimacionJugador::Caminar;
        } else {
            estadoAnim = AnimacionJugador::Idle;
        }
        int mx = (estadoAnim == AnimacionJugador::Caminar)
            ? sprites.maxFrame(AnimacionJugador::Caminar, ultimaDir)
            : sprites.maxFrame(AnimacionJugador::Idle, Abajo);
        if (tickAnim % 2 == 0) frameAnim = (frameAnim + 1) % mx;
    }

    void iniciarLoop() { timer->start(200); }
    void pararLoop() { timer->stop(); }

    void dibujarSpriteJugador(QPainter& p, int jx, int jy, int lado) {
        if (spritesCargados) {
            AnimacionJugador::Tipo t = estadoAnim;
            if (juego->estado == Ganaste) t = AnimacionJugador::Ganar;
            else if (juego->estado == Perdiste) t = AnimacionJugador::Rip;
            QPixmap pm = sprites.frame(t, ultimaDir, frameAnim);
            int sw = pm.width(), sh = pm.height();
            int drawW = std::min(lado, sw), drawH = std::min(lado, sh);
            if (drawW > 0 && drawH > 0) {
                QRect dest(jx - drawW/2, jy - drawH/2, drawW, drawH);
                p.drawPixmap(dest, pm, pm.rect());
            } else {
                int rj = lado/3;
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(90, 150, 210));
                p.drawEllipse(QPoint(jx, jy), rj, rj);
            }
        } else {
            int rj = lado/3;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(50, 70, 100));
            p.drawEllipse(QPoint(jx + 1, jy + 1), rj, rj);
            p.setBrush(QColor(90, 150, 210));
            p.drawEllipse(QPoint(jx, jy), rj, rj);
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        if (!juego) return;
        const Mapa& m = juego->mapa;
        int w = width(), h = height();
        int lado = std::min(w, h) / TAM_TABLERO;
        celdaPx = lado;
        int offsetX = (w - TAM_TABLERO * lado) / 2;
        int offsetY = (h - TAM_TABLERO * lado) / 2;

        for (int r = 0; r < TAM_TABLERO; r++) {
            for (int c = 0; c < TAM_TABLERO; c++) {
                int x = offsetX + c * lado, y = offsetY + r * lado;
                QRect rect(x + 2, y + 2, lado - 3, lado - 3);
                TipoCelda t = m.obtenerCelda(r, c);
                if (spritesBloquesCargados && !spriteBloques.bordes.isNull()) {
                    if (t == Muro) {
                        p.drawPixmap(rect, spriteBloques.bordes, spriteBloques.bordes.rect());
                    } else if (t == Hielo) {
                        p.drawPixmap(rect, spriteBloques.hielo, spriteBloques.hielo.rect());
                    } else {
                        QPixmap& nievePm = ((r + c) % 2 == 0) ? spriteBloques.nieve : spriteBloques.nieve2;
                        p.drawPixmap(rect, nievePm, nievePm.rect());
                    }
                } else {
                    if (t == Muro) {
                        p.fillRect(rect, QColor(72, 65, 85));
                        p.setPen(QColor(50, 45, 60));
                        for (int b = 0; b < 2; b++) p.drawRect(rect.adjusted(b, b, -b, -b));
                        p.setPen(QColor(95, 88, 110));
                        p.drawLine(rect.left(), rect.top(), rect.right(), rect.top());
                        p.drawLine(rect.left(), rect.top(), rect.left(), rect.bottom());
                    } else if (t == Hielo) {
                        QLinearGradient grad(rect.topLeft(), rect.bottomRight());
                        grad.setColorAt(0, QColor(200, 235, 255));
                        grad.setColorAt(1, QColor(150, 205, 245));
                        p.fillRect(rect, grad);
                        p.setPen(QColor(100, 160, 210));
                        p.drawRect(rect);
                    } else {
                        QLinearGradient grad(rect.topLeft(), rect.bottomRight());
                        grad.setColorAt(0, QColor(252, 250, 245));
                        grad.setColorAt(1, QColor(238, 232, 220));
                        p.fillRect(rect, grad);
                        p.setPen(QColor(210, 202, 190));
                        p.drawRect(rect);
                    }
                }
            }
        }

        for (int i = 0; i < juego->numFrutas; i++) {
            if (juego->frutas[i].recogida) continue;
            int fc = juego->frutas[i].pos.celdaX(), fr = juego->frutas[i].pos.celdaY();
            int fx = offsetX + fc * lado + lado/2, fy = offsetY + fr * lado + lado/2;
            bool congelada = juego->frutas[i].congelada;
            if (spritesFrutasCargados && !spriteFruta.sprite.isNull()) {
                QPixmap& pm = spriteFruta.sprite;
                int drawW = std::min(lado/2, pm.width()), drawH = std::min(lado/2, pm.height());
                if (drawW > 0 && drawH > 0) {
                    QRect dest(fx - drawW/2, fy - drawH/2, drawW, drawH);
                    p.drawPixmap(dest, pm, pm.rect());
                    // Overlay azul para fruta congelada (se distingue de la normal)
                    if (congelada) {
                        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                        p.fillRect(dest, QColor(150, 210, 255, 140));
                    }
                } else {
                    p.setPen(Qt::NoPen);
                    p.setBrush(congelada ? QColor(150, 200, 255) : (juego->frutas[i].tipoFruta == Uva ? QColor(100, 50, 120) : QColor(220, 180, 50)));
                    p.drawEllipse(QPoint(fx, fy), lado/4, lado/4);
                }
            } else {
                if (congelada)
                    p.setBrush(QColor(150, 200, 255));
                else if (juego->frutas[i].tipoFruta == Uva)
                    p.setBrush(QColor(100, 50, 120));
                else
                    p.setBrush(QColor(220, 180, 50));
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(fx, fy), lado/4, lado/4);
            }
        }

        int jx = offsetX + juego->jugador.pos.celdaX() * lado + lado/2;
        int jy = offsetY + juego->jugador.pos.celdaY() * lado + lado/2;
        if ((juego->jugador.vivo || juego->estado == Perdiste) && juego->estado == Jugando) {
            dibujarSpriteJugador(p, jx, jy, lado);
        }

        for (int i = 0; i < juego->numEnemigos; i++) {
            if (!juego->enemigos[i].vivo) continue;
            int ex = offsetX + juego->enemigos[i].pos.celdaX() * lado + lado/2;
            int ey = offsetY + juego->enemigos[i].pos.celdaY() * lado + lado/2;
            Direccion dirE = juego->enemigos[i].dir;
            bool moviendo = (dirE != Ninguna);
            int frameE = (frameAnim + i * 2) % 8; // desfasar por enemigo para variedad
            if (spritesEnemigosCargados) {
                QPixmap pm = spritesEnemigo.frame(dirE, frameE, moviendo);
                int drawW = std::min(lado, pm.width()), drawH = std::min(lado, pm.height());
                if (drawW > 0 && drawH > 0) {
                    QRect dest(ex - drawW/2, ey - drawH/2, drawW, drawH);
                    p.drawPixmap(dest, pm, pm.rect());
                } else {
                    int re = lado/3 - 2;
                    p.setPen(Qt::NoPen);
                    p.setBrush(juego->enemigos[i].tipo == Especial ? QColor(220, 90, 90) : QColor(200, 70, 70));
                    p.drawEllipse(QPoint(ex, ey), re, re);
                }
            } else {
                int re = lado/3 - 2;
                bool esp = juego->enemigos[i].tipo == Especial;
                p.setPen(Qt::NoPen);
                p.setBrush(esp ? QColor(120, 40, 40) : QColor(100, 35, 35));
                p.drawEllipse(QPoint(ex + 1, ey + 1), re, re);
                p.setBrush(esp ? QColor(220, 90, 90) : QColor(200, 70, 70));
                p.drawEllipse(QPoint(ex, ey), re, re);
            }
        }

        if (juego->estado == Ganaste || juego->estado == Perdiste) {
            dibujarSpriteJugador(p, jx, jy, lado);
        }

        if ((juego->estado == Ganaste || juego->estado == Perdiste) && animacionFinTerminada) {
            p.fillRect(0, 0, width(), height(), QColor(0, 0, 0, 180));
            QFont f = font(); f.setPointSize(20); f.setBold(true); p.setFont(f);
            if (juego->estado == Ganaste) {
                p.setPen(QColor(200, 255, 200));
                p.drawText(rect(), Qt::AlignCenter, "¡Ganaste!");
            } else {
                p.setPen(QColor(255, 180, 180));
                p.drawText(rect(), Qt::AlignCenter, "Perdiste\n(Clic en Menú niveles para volver)");
            }
        }
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (!juego || juego->estado != Jugando) return;
        switch (e->key()) {
            case Qt::Key_Up:    juego->moverJugador(Arriba); break;
            case Qt::Key_Down:  juego->moverJugador(Abajo); break;
            case Qt::Key_Left:  juego->moverJugador(Izquierda); break;
            case Qt::Key_Right: juego->moverJugador(Derecha); break;
            case Qt::Key_Space:
                juego->congelar();
                estadoAnim = AnimacionJugador::Congelar;
                frameAnim = 0;
                ultimaDir = (juego->jugador.dir != Ninguna) ? juego->jugador.dir : ultimaDir;
                break;
            case Qt::Key_Shift:
                juego->descongelar();
                estadoAnim = AnimacionJugador::RomperHielo;
                frameAnim = 0;
                break;
            default: break;
        }
        update();
    }
};

class PantallaUnoVsUno;

class PantallaModo : public QWidget {
    QStackedWidget* stack = nullptr;
    QWidget* pantallaNiveles = nullptr;
    QWidget* pantalla1v1 = nullptr;

public:
    explicit PantallaModo(QStackedWidget* s, QWidget* niveles, QWidget* unoVsUno, QWidget* parent = nullptr)
        : QWidget(parent), stack(s), pantallaNiveles(niveles), pantalla1v1(unoVsUno) {
        setStyleSheet(
            "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
            "  stop:0 #120b24, stop:0.5 #241a45, stop:1 #120b24);"
            "QLabel { color: #fdf7ff; }"
            "QPushButton {"
            "  background-color: #ff66aa;"
            "  color: #2b0618;"
            "  border: 3px solid #ffe6ff;"
            "  border-radius: 12px;"
            "  padding: 12px 18px;"
            "  font-weight: bold;"
            "  font-size: 18px;"
            "}"
            "QPushButton:hover { background-color: #ff88c2; border-color: #ffffff; }"
            "QPushButton:pressed { background-color: #ff4d99; }"
        );
        QVBoxLayout* mainL = new QVBoxLayout(this);
        mainL->setSpacing(24);

        QLabel* titulo = new QLabel("Selecciona modo de juego");
        QFont ft = titulo->font();
        ft.setPointSize(22);
        ft.setBold(true);
        titulo->setFont(ft);
        titulo->setAlignment(Qt::AlignCenter);
        mainL->addWidget(titulo);

        QLabel* sub = new QLabel("Elige jugar niveles normales\no un duelo 1 vs 1 contra el bot.");
        sub->setAlignment(Qt::AlignCenter);
        mainL->addWidget(sub);

        QPushButton* btnNiveles = new QPushButton("Niveles");
        QPushButton* btn1v1 = new QPushButton("1 vs 1");
        btnNiveles->setMinimumHeight(56);
        btn1v1->setMinimumHeight(56);

        connect(btnNiveles, &QPushButton::clicked, this, [this]() {
            if (stack && pantallaNiveles) stack->setCurrentWidget(pantallaNiveles);
        });
        connect(btn1v1, &QPushButton::clicked, this, [this]() {
            if (stack && pantalla1v1) stack->setCurrentWidget(pantalla1v1);
        });

        mainL->addWidget(btnNiveles);
        mainL->addWidget(btn1v1);
        mainL->addStretch();
    }
};

class PantallaNiveles : public QWidget {
    QVBoxLayout* layout = nullptr;
    QLabel* titulo = nullptr;
    QWidget* contenedorBotones = nullptr;
    QStackedWidget* stack = nullptr;
    Juego* juego = nullptr;
    WidgetTablero* tablero = nullptr;

public:
    explicit PantallaNiveles(QStackedWidget* s, Juego* j, WidgetTablero* tb, QWidget* parent = nullptr)
        : QWidget(parent), stack(s), juego(j), tablero(tb) {
        setStyleSheet(
            "background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
            "  stop:0 #1b1030, stop:0.5 #241a45, stop:1 #1b1030);"
            "QLabel { color: #fdf7ff; }"
            "QLabel#Subtitulo { color: #ffe8a8; font-size: 14px; }"
            "QPushButton {"
            "  background-color: #ffcc33;"
            "  color: #3b1a0a;"
            "  border: 3px solid #ffef99;"
            "  border-radius: 10px;"
            "  padding: 10px 14px;"
            "  font-weight: bold;"
            "  font-size: 18px;"
            "  text-shadow: 1px 1px 0px rgba(255,255,255,0.6);"
            "}"
            "QPushButton:hover {"
            "  background-color: #ffe066;"
            "  border-color: #ffffff;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #ffb833;"
            "  border-color: #f0d070;"
            "}"
        );
        QVBoxLayout* mainL = new QVBoxLayout(this);
        mainL->setSpacing(28);
        titulo = new QLabel("Elige nivel");
        titulo->setAlignment(Qt::AlignCenter);
        QFont f = titulo->font();
        f.setPointSize(24);
        titulo->setFont(f);
        mainL->addWidget(titulo);
        QLabel* sub = new QLabel("Más nivel = más enemigos.\nRecoge todas las frutas sin que te atrapen.");
        sub->setObjectName("Subtitulo");
        sub->setAlignment(Qt::AlignCenter);
        mainL->addWidget(sub);
        contenedorBotones = new QWidget;
        QHBoxLayout* botonesL = new QHBoxLayout(contenedorBotones);
        botonesL->setSpacing(16);
        for (int n = 1; n <= 6; n++) {
            QPushButton* btn = new QPushButton(QString::number(n));
            btn->setMinimumSize(64, 56);
            btn->setFont(QFont("Sans", 18));
            int nivel = n;
            connect(btn, &QPushButton::clicked, this, [this, nivel]() {
                juego->iniciarNivel(nivel);
                tablero->iniciarLoop();
                // 0: Modo | 1: Menú niveles | 2: Juego | 3: 1vs1
                if (stack->count() > 2) stack->setCurrentIndex(2);
                tablero->setFocus();
            });
            botonesL->addWidget(btn);
        }
        mainL->addWidget(contenedorBotones, 0, Qt::AlignCenter);
        mainL->addStretch();
    }
};

class PantallaUnoVsUno : public QWidget {
    QStackedWidget* stack = nullptr;
    Juego juego1;
    Juego juegoBot;
    WidgetTablero* tablero1 = nullptr;
    WidgetTablero* tableroBot = nullptr;

public:
    explicit PantallaUnoVsUno(QStackedWidget* s, QWidget* parent = nullptr)
        : QWidget(parent), stack(s) {
        juego1.esBot = false;
        juegoBot.esBot = true;

        QVBoxLayout* mainL = new QVBoxLayout(this);
        mainL->setSpacing(8);

        QLabel* titulo = new QLabel("Modo 1 vs 1");
        QFont ft = titulo->font();
        ft.setPointSize(20);
        ft.setBold(true);
        titulo->setFont(ft);
        titulo->setAlignment(Qt::AlignCenter);
        mainL->addWidget(titulo);

        QHBoxLayout* filas = new QHBoxLayout();
        filas->setSpacing(12);

        QVBoxLayout* col1 = new QVBoxLayout();
        QLabel* l1 = new QLabel("Mapa Jugador 1");
        l1->setAlignment(Qt::AlignCenter);
        col1->addWidget(l1);
        tablero1 = new WidgetTablero(&juego1, this);
        tablero1->setMinimumSize(360, 360);
        col1->addWidget(tablero1, 1);

        QVBoxLayout* col2 = new QVBoxLayout();
        QLabel* l2 = new QLabel("Mapa Jugador 2 (Bot)");
        l2->setAlignment(Qt::AlignCenter);
        col2->addWidget(l2);
        tableroBot = new WidgetTablero(&juegoBot, this);
        tableroBot->setMinimumSize(360, 360);
        col2->addWidget(tableroBot, 1);

        filas->addLayout(col1, 1);
        filas->addLayout(col2, 1);
        mainL->addLayout(filas, 1);

        QPushButton* reiniciar = new QPushButton("Reiniciar duelo (nivel 5)");
        connect(reiniciar, &QPushButton::clicked, this, [this]() {
            iniciarPartida();
        });
        mainL->addWidget(reiniciar, 0, Qt::AlignCenter);

        iniciarPartida();
    }

    void iniciarPartida() {
        juego1.esBot = false;
        juegoBot.esBot = true;
        juego1.iniciarNivel(5);
        juegoBot.iniciarNivel(5);
        if (tablero1) tablero1->iniciarLoop();
        if (tableroBot) tableroBot->iniciarLoop();
        if (tablero1) tablero1->setFocus();
    }

    void detener() {
        if (tablero1) tablero1->pararLoop();
        if (tableroBot) tableroBot->pararLoop();
    }
};

class VentanaPrincipal : public QMainWindow {
    QStackedWidget* stack = nullptr;
    Juego juego;
    WidgetTablero* tablero = nullptr;
    PantallaNiveles* pantallaNiveles = nullptr;
    PantallaUnoVsUno* pantalla1v1 = nullptr;
    PantallaModo* pantallaModo = nullptr;

public:
    VentanaPrincipal() : QMainWindow(nullptr) {
        setWindowTitle("Proyecto Ice Cream - Qt6");
        setMinimumSize(900, 520);
        resize(900, 520);

        QWidget* central = new QWidget(this);
        QVBoxLayout* centralL = new QVBoxLayout(central);
        setCentralWidget(central);

        stack = new QStackedWidget(this);
        tablero = new WidgetTablero(&juego, this);
        tablero->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        pantallaNiveles = new PantallaNiveles(stack, &juego, tablero, this);
        pantalla1v1 = new PantallaUnoVsUno(stack, this);
        pantallaModo = new PantallaModo(stack, pantallaNiveles, pantalla1v1, this);
        // 0: Modo | 1: Menú niveles | 2: Juego | 3: 1vs1
        stack->addWidget(pantallaModo);
        stack->addWidget(pantallaNiveles);
        stack->addWidget(tablero);
        stack->addWidget(pantalla1v1);

        QHBoxLayout* topL = new QHBoxLayout();
        QPushButton* volver = new QPushButton("Menú principal");
        connect(volver, &QPushButton::clicked, this, [this]() {
            tablero->pararLoop();
            if (pantalla1v1) pantalla1v1->detener();
            if (stack && pantallaModo) stack->setCurrentWidget(pantallaModo);
        });
        topL->addWidget(volver);
        topL->addStretch();
        QLabel* instrucciones = new QLabel("Flechas: mover | Espacio: congelar | Shift: descongelar");
        topL->addWidget(instrucciones);
        centralL->addLayout(topL);
        centralL->addWidget(stack, 1);
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    VentanaPrincipal v;
    v.show();
    return app.exec();
}