#include <GL/freeglut.h>
#include <GL/glext.h>
#include <mmsystem.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ======================================================================
//  CONFIGURACION Y ESTADO GLOBAL
// ======================================================================

// Camara en primera persona (posicion de los ojos)
float camX = 0.0f, camY = 1.6f, camZ = 3.2f;
float yaw = -90.0f;
float pitch = -8.0f;

// Movimiento suavizado: la velocidad se acerca gradualmente a la deseada
float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
const float MAX_SPEED      = 2.4f;   // unidades por segundo
const float ACCELERATION   = 10.0f;  // rapidez con la que se alcanza la velocidad deseada
const float ROT_SPEED      = 90.0f;  // grados por segundo (flechas)
const float MOUSE_SENS     = 0.12f;  // grados por pixel
const float PLAYER_RADIUS  = 0.25f;
const float EYE_HEIGHT     = 1.6f;
const float MIN_Y = 0.6f, MAX_Y = 3.5f;
const float INTERACT_RANGE = 2.5f;

// Cara interna de las paredes
const float ROOM_MIN_X = -4.925f, ROOM_MAX_X = 4.925f;
const float ROOM_MIN_Z = -3.925f, ROOM_MAX_Z = 3.925f;

const float DESK_TOP   = 0.79f;
const float SWITCH_X   = -2.0f, SWITCH_Y = 1.3f;
const float TROPHY_X   = 1.1f,  TROPHY_Z = 0.55f;
const float SEAT_X     = 0.0f,  SEAT_Z   = 1.75f;   // silla frente al monitor
const float SEATED_EYE = 1.3f;

// Hueco de la ventana en la pared derecha y nivel de la calle exterior
const float WIN_Z0 = -1.4f, WIN_Z1 = 1.4f, WIN_Y0 = 0.9f, WIN_Y1 = 3.2f;
const float STREET_Y = -4.0f;    // la habitacion esta en un primer piso

// Entrada
bool keys[256];
bool arrows[4];              // izquierda, derecha, arriba, abajo
bool mouseCaptured = false;
int winW = 1280, winH = 720;
int lastTime = 0;

// Dispositivos: estado (encendido/apagado) y nivel animado 0..1
bool pcOn = false, monitorOn = false, neonOn = false;
float pcLevel = 0.0f, monitorLevel = 0.0f, neonLevel = 0.0f;
float neonOnTime = 0.0f;
float fanAngle = 0.0f, trophyAngle = 0.0f;
float animTime = 0.0f;

// Silla y transicion al sentarse / levantarse
float chairX = 1.8f, chairZ = 1.8f;
enum SeatState { STANDING, SITTING_DOWN, SEATED, STANDING_UP };
SeatState seatState = STANDING;
float seatT = 0.0f;
struct Pose { float x, y, z, yaw, pitch, chairX, chairZ; };
Pose poseFrom, poseTo;

// Objetos interactivos y mision
enum Target { T_NONE = -1, T_PC, T_MONITOR, T_SWITCH, T_CHAIR, T_COUNT };
const int MISSION_ORDER[3] = { T_PC, T_MONITOR, T_SWITCH };
int currentTarget = T_NONE;
int missionStep = 0;
bool missionDone = false;
float missionTime = 0.0f;

// HUD
char hudMsg[128] = "";
float hudMsgTime = 0.0f;
float hudMsgColor[3] = { 1.0f, 1.0f, 1.0f };
bool showHelp = true;

// Colores de los libros
const float bookColors[6][3] = {
    {0.4f, 0.9f, 0.4f}, {1.0f, 0.3f, 0.6f}, {0.2f, 0.8f, 1.0f},
    {1.0f, 0.85f, 0.2f}, {0.6f, 0.4f, 1.0f}, {1.0f, 0.5f, 0.2f}
};

// ======================================================================
//  UTILIDADES: camara, cajas (AABB), colisiones y raycasting
// ======================================================================

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }

float approach(float v, float target, float step) {
    if (v < target) return (v + step > target) ? target : v + step;
    return (v - step < target) ? target : v - step;
}

void lookDir(float d[3]) {
    float yawRad = yaw * M_PI / 180.0f;
    float pitchRad = pitch * M_PI / 180.0f;
    d[0] = cosf(pitchRad) * cosf(yawRad);
    d[1] = sinf(pitchRad);
    d[2] = cosf(pitchRad) * sinf(yawRad);
}

void getLookAt(float &lookX, float &lookY, float &lookZ) {
    float d[3];
    lookDir(d);
    lookX = camX + d[0];
    lookY = camY + d[1];
    lookZ = camZ + d[2];
}

void clampLook() {
    pitch = clampf(pitch, -89.0f, 89.0f);
    if (yaw > 180.0f)  yaw -= 360.0f;
    if (yaw < -180.0f) yaw += 360.0f;
}

struct Box { float min[3], max[3]; };

Box makeBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    Box b = { { cx - sx / 2, cy - sy / 2, cz - sz / 2 },
              { cx + sx / 2, cy + sy / 2, cz + sz / 2 } };
    return b;
}

Box chairBox() { return makeBox(chairX, 0.8f, chairZ, 0.7f, 1.6f, 0.72f); }

// Se arman en cada consulta porque la silla puede moverse
int getObstacles(Box out[]) {
    int n = 0;
    out[n++] = makeBox( 0.0f, 0.4f,   0.5f, 3.2f, 0.8f,  1.4f);   // escritorio
    out[n++] = makeBox(-4.7f, 1.15f,  0.0f, 0.4f, 2.3f,  2.4f);   // librero izquierdo
    out[n++] = makeBox( 1.25f, 1.15f, 3.68f, 2.4f, 2.3f, 0.4f);   // librero de la pared frontal
    out[n++] = makeBox(-3.2f, 2.33f, -3.7f, 1.8f, 1.12f, 0.35f);  // estantes de pared con libros
    out[n++] = makeBox( 3.2f, 2.63f, -3.7f, 1.8f, 0.52f, 0.35f);
    out[n++] = chairBox();
    return n;
}

// El jugador es un cilindro: circulo en XZ que va del suelo hasta la cabeza
bool collides(float x, float y, float z) {
    if (x < ROOM_MIN_X + PLAYER_RADIUS || x > ROOM_MAX_X - PLAYER_RADIUS ||
        z < ROOM_MIN_Z + PLAYER_RADIUS || z > ROOM_MAX_Z - PLAYER_RADIUS)
        return true;

    Box obs[8];
    int n = getObstacles(obs);
    float head = y + 0.1f;
    for (int i = 0; i < n; i++) {
        if (head < obs[i].min[1]) continue;   // el obstaculo queda por encima
        float dx = x - clampf(x, obs[i].min[0], obs[i].max[0]);
        float dz = z - clampf(z, obs[i].min[2], obs[i].max[2]);
        if (dx * dx + dz * dz < PLAYER_RADIUS * PLAYER_RADIUS) return true;
    }
    return false;
}

// Si ya se esta dentro de un obstaculo se permite moverse para poder salir
bool canMoveTo(float x, float y, float z) {
    return !collides(x, y, z) || collides(camX, camY, camZ);
}

// Interseccion rayo-caja (metodo de slabs); tHit = distancia de entrada
bool rayBox(const float o[3], const float d[3], const Box& b, float& tHit) {
    float tmin = 0.0f, tmax = 1e9f;
    for (int a = 0; a < 3; a++) {
        if (fabsf(d[a]) < 1e-6f) {
            if (o[a] < b.min[a] || o[a] > b.max[a]) return false;
            continue;
        }
        float t1 = (b.min[a] - o[a]) / d[a];
        float t2 = (b.max[a] - o[a]) / d[a];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    tHit = tmin;
    return true;
}

Box targetBox(int t) {
    switch (t) {
        case T_PC:      return makeBox(-1.34f, 1.15f, 0.365f, 0.48f, 0.86f, 0.59f);
        case T_MONITOR: return makeBox( 0.0f,  1.25f, 0.16f,  1.44f, 0.84f, 0.14f);
        case T_SWITCH:  return makeBox(SWITCH_X, SWITCH_Y, ROOM_MIN_Z + 0.03f, 0.26f, 0.36f, 0.08f);
        default:        return chairBox();
    }
}

// Objeto interactivo mas cercano en la direccion de la mirada
int findTarget() {
    float o[3] = { camX, camY, camZ };
    float d[3];
    lookDir(d);
    int best = T_NONE;
    float bestT = INTERACT_RANGE;
    for (int t = 0; t < T_COUNT; t++) {
        if (t == T_CHAIR && seatState != STANDING) continue;
        float th;
        if (rayBox(o, d, targetBox(t), th) && th < bestT) {
            bestT = th;
            best = t;
        }
    }
    return best;
}

// ======================================================================
//  SHADER GLSL DEL NEON
//  opengl32.lib de Windows solo trae OpenGL 1.1, por eso las funciones
//  de shaders (OpenGL 2.0) se cargan en tiempo de ejecucion.
// ======================================================================

PFNGLCREATESHADERPROC       pglCreateShader;
PFNGLSHADERSOURCEPROC       pglShaderSource;
PFNGLCOMPILESHADERPROC      pglCompileShader;
PFNGLGETSHADERIVPROC        pglGetShaderiv;
PFNGLGETSHADERINFOLOGPROC   pglGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC      pglCreateProgram;
PFNGLATTACHSHADERPROC       pglAttachShader;
PFNGLLINKPROGRAMPROC        pglLinkProgram;
PFNGLGETPROGRAMIVPROC       pglGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC  pglGetProgramInfoLog;
PFNGLUSEPROGRAMPROC         pglUseProgram;
PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation;
PFNGLUNIFORM1FPROC          pglUniform1f;
PFNGLUNIFORMMATRIX4FVPROC   pglUniformMatrix4fv;

GLuint neonProgram = 0;
GLint uTimeLoc = -1, uPowerLoc = -1, uViewInvLoc = -1;
float viewInv[16];
float glowPower = 1.0f;

// Convierte la posicion a coordenadas de mundo para que la onda de brillo
// recorra los tubos sin depender de donde este la camara
const char* NEON_VS =
    "uniform mat4 uViewInv;\n"
    "varying vec3 vWorld;\n"
    "void main() {\n"
    "    vWorld = (uViewInv * gl_ModelViewMatrix * gl_Vertex).xyz;\n"
    "    gl_FrontColor = gl_Color;\n"
    "    gl_Position = ftransform();\n"
    "}\n";

// uPower: 0 = tubo apagado, 1 = encendido
const char* NEON_FS =
    "uniform float uTime;\n"
    "uniform float uPower;\n"
    "varying vec3 vWorld;\n"
    "void main() {\n"
    "    float wave = 0.5 + 0.5 * sin(uTime * 3.0 - (vWorld.x + vWorld.y + vWorld.z) * 2.0);\n"
    "    vec3 on  = gl_Color.rgb * (0.6 + 0.5 * wave + 0.6 * pow(wave, 12.0));\n"
    "    vec3 off = gl_Color.rgb * 0.15;\n"
    "    gl_FragColor = vec4(mix(off, on, uPower), gl_Color.a);\n"
    "}\n";

#define LOAD_GL_FUNC(var, type, name) (var = (type)glutGetProcAddress(name))

bool loadShaderFunctions() {
    return LOAD_GL_FUNC(pglCreateShader,       PFNGLCREATESHADERPROC,       "glCreateShader") &&
           LOAD_GL_FUNC(pglShaderSource,       PFNGLSHADERSOURCEPROC,       "glShaderSource") &&
           LOAD_GL_FUNC(pglCompileShader,      PFNGLCOMPILESHADERPROC,      "glCompileShader") &&
           LOAD_GL_FUNC(pglGetShaderiv,        PFNGLGETSHADERIVPROC,        "glGetShaderiv") &&
           LOAD_GL_FUNC(pglGetShaderInfoLog,   PFNGLGETSHADERINFOLOGPROC,   "glGetShaderInfoLog") &&
           LOAD_GL_FUNC(pglCreateProgram,      PFNGLCREATEPROGRAMPROC,      "glCreateProgram") &&
           LOAD_GL_FUNC(pglAttachShader,       PFNGLATTACHSHADERPROC,       "glAttachShader") &&
           LOAD_GL_FUNC(pglLinkProgram,        PFNGLLINKPROGRAMPROC,        "glLinkProgram") &&
           LOAD_GL_FUNC(pglGetProgramiv,       PFNGLGETPROGRAMIVPROC,       "glGetProgramiv") &&
           LOAD_GL_FUNC(pglGetProgramInfoLog,  PFNGLGETPROGRAMINFOLOGPROC,  "glGetProgramInfoLog") &&
           LOAD_GL_FUNC(pglUseProgram,         PFNGLUSEPROGRAMPROC,         "glUseProgram") &&
           LOAD_GL_FUNC(pglGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC, "glGetUniformLocation") &&
           LOAD_GL_FUNC(pglUniform1f,          PFNGLUNIFORM1FPROC,          "glUniform1f") &&
           LOAD_GL_FUNC(pglUniformMatrix4fv,   PFNGLUNIFORMMATRIX4FVPROC,   "glUniformMatrix4fv");
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = pglCreateShader(type);
    pglShaderSource(s, 1, &src, NULL);
    pglCompileShader(s);
    GLint ok = 0;
    pglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        pglGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Error al compilar shader:\n%s\n", log);
        return 0;
    }
    return s;
}

// Si algo falla, neonProgram queda en 0 y el neon se anima por CPU
void initNeonShader() {
    if (!loadShaderFunctions()) {
        fprintf(stderr, "GLSL no disponible: se usa el neon sin shader\n");
        return;
    }
    GLuint vs = compileShader(GL_VERTEX_SHADER, NEON_VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, NEON_FS);
    if (!vs || !fs) return;

    GLuint prog = pglCreateProgram();
    pglAttachShader(prog, vs);
    pglAttachShader(prog, fs);
    pglLinkProgram(prog);
    GLint ok = 0;
    pglGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        pglGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "Error al enlazar shader:\n%s\n", log);
        return;
    }
    neonProgram = prog;
    uTimeLoc    = pglGetUniformLocation(prog, "uTime");
    uPowerLoc   = pglGetUniformLocation(prog, "uPower");
    uViewInvLoc = pglGetUniformLocation(prog, "uViewInv");
}

// La vista es rigida (rotacion R + traslacion t): su inversa es R^T y -R^T*t
void computeViewInverse() {
    float v[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, v);
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 3; r++)
            viewInv[c * 4 + r] = v[r * 4 + c];
    for (int r = 0; r < 3; r++)
        viewInv[12 + r] = -(v[r * 4] * v[12] + v[r * 4 + 1] * v[13] + v[r * 4 + 2] * v[14]);
    viewInv[3] = viewInv[7] = viewInv[11] = 0.0f;
    viewInv[15] = 1.0f;
}

// Todo lo que se dibuje entre beginGlow/endGlow brilla con intensidad "power"
void beginGlow(float power) {
    glDisable(GL_LIGHTING);
    glowPower = power;
    if (!neonProgram) return;
    pglUseProgram(neonProgram);
    pglUniform1f(uTimeLoc, animTime);
    pglUniform1f(uPowerLoc, power);
    pglUniformMatrix4fv(uViewInvLoc, 1, GL_FALSE, viewInv);
}

void endGlow() {
    if (neonProgram) pglUseProgram(0);
    glEnable(GL_LIGHTING);
}

void glowColor(float r, float g, float b) {
    if (neonProgram) {
        glColor3f(r, g, b);
        return;
    }
    float k = 0.15f + glowPower * (0.75f + 0.2f * sinf(animTime * 3.0f));
    glColor3f(r * k, g * k, b * k);
}

// ======================================================================
//  CARGADOR DE MODELOS OBJ
//  Lee v, vn y caras f (v, v/vt, v//vn, v/vt/vn) de cualquier numero de
//  lados. El modelo se centra en X/Z, se apoya en y=0 y se escala a
//  altura 1 para poder ubicarlo facilmente en la escena.
// ======================================================================

struct Vec3 { float x, y, z; };

GLuint trophyList = 0;

// Indices OBJ empiezan en 1; los negativos cuentan desde el final
int objIndex(int i, int count) { return i > 0 ? i - 1 : count + i; }

Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 u = { b.x - a.x, b.y - a.y, b.z - a.z };
    Vec3 v = { c.x - a.x, c.y - a.y, c.z - a.z };
    Vec3 n = { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
    float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 0.0f) { n.x /= len; n.y /= len; n.z /= len; }
    return n;
}

bool loadOBJ(const char* path, GLuint& list) {
    FILE* f = fopen(path, "r");
    if (!f) return false;

    std::vector<Vec3> pos, nor, triPos, triNor;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        Vec3 v;
        if (strncmp(line, "v ", 2) == 0) {
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3) pos.push_back(v);
        } else if (strncmp(line, "vn ", 3) == 0) {
            if (sscanf(line + 3, "%f %f %f", &v.x, &v.y, &v.z) == 3) nor.push_back(v);
        } else if (strncmp(line, "f ", 2) == 0) {
            std::vector<int> vi, ni;
            bool valid = true;
            for (char* tok = strtok(line + 2, " \t\r\n"); tok; tok = strtok(NULL, " \t\r\n")) {
                int a = 0, b = 0, c = 0;
                if (sscanf(tok, "%d/%d/%d", &a, &b, &c) != 3 && sscanf(tok, "%d//%d", &a, &c) != 2) {
                    c = 0;
                    if (sscanf(tok, "%d", &a) != 1) { valid = false; break; }
                }
                int p = objIndex(a, pos.size());
                int n = c ? objIndex(c, nor.size()) : -1;
                if (p < 0 || p >= (int)pos.size() || n >= (int)nor.size()) { valid = false; break; }
                vi.push_back(p);
                ni.push_back(n);
            }
            if (!valid) continue;

            // Triangulacion en abanico: (0, k, k+1)
            for (size_t k = 1; k + 1 < vi.size(); k++) {
                int id[3] = { 0, (int)k, (int)k + 1 };
                Vec3 flat = faceNormal(pos[vi[0]], pos[vi[k]], pos[vi[k + 1]]);
                for (int j = 0; j < 3; j++) {
                    triPos.push_back(pos[vi[id[j]]]);
                    triNor.push_back(ni[id[j]] >= 0 ? nor[ni[id[j]]] : flat);
                }
            }
        }
    }
    fclose(f);
    if (triPos.empty()) return false;

    Vec3 lo = triPos[0], hi = triPos[0];
    for (size_t i = 1; i < triPos.size(); i++) {
        lo.x = std::min(lo.x, triPos[i].x); hi.x = std::max(hi.x, triPos[i].x);
        lo.y = std::min(lo.y, triPos[i].y); hi.y = std::max(hi.y, triPos[i].y);
        lo.z = std::min(lo.z, triPos[i].z); hi.z = std::max(hi.z, triPos[i].z);
    }
    float s = (hi.y > lo.y) ? 1.0f / (hi.y - lo.y) : 1.0f;
    float cx = (lo.x + hi.x) / 2.0f, cz = (lo.z + hi.z) / 2.0f;

    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < triPos.size(); i++) {
        glNormal3f(triNor[i].x, triNor[i].y, triNor[i].z);
        glVertex3f((triPos[i].x - cx) * s, (triPos[i].y - lo.y) * s, (triPos[i].z - cz) * s);
    }
    glEnd();
    glEndList();

    printf("Modelo cargado: %s (%d triangulos)\n", path, (int)(triPos.size() / 3));
    return true;
}

std::string exeDir;   // carpeta del .exe, para encontrar models/ y audio/

// Prefiere el archivo junto al .exe; si no existe, usa la ruta relativa al directorio actual
std::string assetPath(const char* rel) {
    std::string p = exeDir + rel;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return rel;
    fclose(f);
    return p;
}

void loadTrophy() {
    if (!loadOBJ(assetPath("models/trofeo.obj").c_str(), trophyList))
        fprintf(stderr, "Aviso: no se encontro models/trofeo.obj\n");
}

// ======================================================================
//  AUDIO
//  MCI de Windows (winmm) reproduce MP3/WAV sin librerias externas.
//  Si un archivo no existe, el juego sigue sin ese sonido.
// ======================================================================

bool gameMusicReady = false, victoryReady = false;

bool openSound(const char* rel, const char* alias) {
    std::string cmd = "open \"" + assetPath(rel) + "\" type mpegvideo alias " + alias;
    if (mciSendStringA(cmd.c_str(), NULL, 0, NULL) == 0) return true;
    fprintf(stderr, "Aviso: no se pudo abrir %s (se omite ese sonido)\n", rel);
    return false;
}

void initAudio() {
    gameMusicReady = openSound("audio/DJVI - Dry Out.mp3", "juego");
    victoryReady   = openSound("audio/super mario - soundtrak monedas.mp3", "victoria");
}

// Musica del minijuego del monitor (en bucle)
void playGameMusic() {
    if (victoryReady)   mciSendStringA("stop victoria", NULL, 0, NULL);
    if (gameMusicReady) mciSendStringA("play juego from 0 repeat", NULL, 0, NULL);
}

void stopAllMusic() {
    if (gameMusicReady) mciSendStringA("stop juego", NULL, 0, NULL);
    if (victoryReady)   mciSendStringA("stop victoria", NULL, 0, NULL);
}

// Al completar la mision se corta la musica del juego y suenan las monedas
void playVictoryMusic() {
    if (gameMusicReady) mciSendStringA("stop juego", NULL, 0, NULL);
    if (victoryReady)   mciSendStringA("play victoria from 0", NULL, 0, NULL);
}

// ======================================================================
//  ESCENA
// ======================================================================

void drawCube(float size) {
    glutSolidCube(size);
}

void drawBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    glPushMatrix();
        glTranslatef(cx, cy, cz);
        glScalef(sx, sy, sz);
        glutSolidCube(1.0);
    glPopMatrix();
}

// Los muros no usan iluminacion; con el neon encendido toman un tinte violeta
void roomColor(float r, float g, float b) {
    float n = neonLevel;
    glColor3f(r + 0.04f * n, g + 0.02f * n, b + 0.09f * n);
}

// Tubos de neon en las aristas (dentro de beginGlow/endGlow)
void drawNeonEdges() {
    const float t = 0.08f;
    // {x, y, z, ancho, alto, profundidad}
    const float bars[12][6] = {
        {  0.0f,  3.96f, -3.92f, 9.9f, t,     t    },   // techo: fondo
        { -4.92f, 3.96f,  0.0f,  t,    t,     7.9f },   // techo: izquierda
        {  4.92f, 3.96f,  0.0f,  t,    t,     7.9f },   // techo: derecha
        {  0.0f,  3.96f,  3.92f, 9.9f, t,     t    },   // techo: frente
        {  0.0f,  0.04f, -3.92f, 9.9f, t,     t    },   // suelo: fondo
        { -4.92f, 0.04f,  0.0f,  t,    t,     7.9f },   // suelo: izquierda
        {  4.92f, 0.04f,  0.0f,  t,    t,     7.9f },   // suelo: derecha
        {  0.0f,  0.04f,  3.92f, 9.9f, t,     t    },   // suelo: frente
        { -4.92f, 2.0f,  -3.92f, t,    3.95f, t    },   // esquinas verticales
        {  4.92f, 2.0f,  -3.92f, t,    3.95f, t    },
        { -4.92f, 2.0f,   3.92f, t,    3.95f, t    },
        {  4.92f, 2.0f,   3.92f, t,    3.95f, t    }
    };

    glowColor(0.15f, 0.95f, 1.0f);
    for (int i = 0; i < 12; i++)
        drawBox(bars[i][0], bars[i][1], bars[i][2], bars[i][3], bars[i][4], bars[i][5]);
}

// Habitacion
void drawRoom() {
    glDisable(GL_LIGHTING);

    // Suelo
    roomColor(0.25f, 0.18f, 0.12f);
    glPushMatrix();
        glTranslatef(0.0f, -0.05f, 0.0f);
        glScalef(10.0f, 0.1f, 8.0f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared trasera
    roomColor(0.35f, 0.28f, 0.22f);
    glPushMatrix();
        glTranslatef(0.0f, 2.0f, -4.0f);
        glScalef(10.0f, 4.0f, 0.15f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared frontal (cierra la habitacion para el recorrido)
    glPushMatrix();
        glTranslatef(0.0f, 2.0f, 4.0f);
        glScalef(10.0f, 4.0f, 0.15f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared izquierda
    roomColor(0.32f, 0.26f, 0.20f);
    glPushMatrix();
        glTranslatef(-5.0f, 2.0f, 0.0f);
        glScalef(0.15f, 4.0f, 8.0f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared derecha, armada en 4 piezas alrededor del hueco de la ventana
    roomColor(0.32f, 0.26f, 0.20f);
    drawBox(5.0f, WIN_Y0 / 2, 0.0f, 0.15f, WIN_Y0, 8.0f);                       // debajo
    drawBox(5.0f, (WIN_Y1 + 4.0f) / 2, 0.0f, 0.15f, 4.0f - WIN_Y1, 8.0f);      // encima
    drawBox(5.0f, (WIN_Y0 + WIN_Y1) / 2, (WIN_Z0 - 4.0f) / 2,
            0.15f, WIN_Y1 - WIN_Y0, WIN_Z0 + 4.0f);                             // hacia el fondo
    drawBox(5.0f, (WIN_Y0 + WIN_Y1) / 2, (WIN_Z1 + 4.0f) / 2,
            0.15f, WIN_Y1 - WIN_Y0, 4.0f - WIN_Z1);                             // hacia el frente

    // Techo
    roomColor(0.12f, 0.12f, 0.15f);
    glPushMatrix();
        glTranslatef(0.0f, 4.0f, 0.0f);
        glScalef(10.0f, 0.1f, 8.0f);
        drawCube(1.0f);
    glPopMatrix();

    glEnable(GL_LIGHTING);
}

// Puerta en la pared frontal
void drawDoor() {
    const float z = ROOM_MAX_Z;
    glDisable(GL_LIGHTING);
    glColor3f(0.18f, 0.12f, 0.09f);                       // marco
    drawBox(3.2f, 1.13f, z - 0.01f, 1.16f, 2.26f, 0.02f);
    glColor3f(0.42f, 0.29f, 0.18f);                       // hoja
    drawBox(3.2f, 1.08f, z - 0.03f, 1.0f, 2.1f, 0.03f);
    glColor3f(0.36f, 0.24f, 0.15f);                       // paneles
    drawBox(3.2f, 1.55f, z - 0.05f, 0.7f, 0.75f, 0.01f);
    drawBox(3.2f, 0.55f, z - 0.05f, 0.7f, 0.75f, 0.01f);
    glColor3f(0.85f, 0.75f, 0.45f);                       // manija
    glPushMatrix();
        glTranslatef(2.82f, 1.05f, z - 0.07f);
        glutSolidSphere(0.04, 12, 12);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

// Escritorio
void drawDesk() {
    glColor3f(0.45f, 0.32f, 0.20f);
    glPushMatrix();
        glTranslatef(0.0f, 0.75f, 0.5f);
        glScalef(3.2f, 0.08f, 1.4f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.35f, 0.25f, 0.15f);
    float legPos[4][2] = {{-1.4f, 0.5f}, {1.4f, 0.5f}, {-1.4f, -0.5f}, {1.4f, -0.5f}};
    for(int i = 0; i < 4; i++) {
        glPushMatrix();
            glTranslatef(legPos[i][0], 0.35f, legPos[i][1] + 0.5f);
            glScalef(0.08f, 0.7f, 0.08f);
            drawCube(1.0f);
        glPopMatrix();
    }
}

void screenQuad(float x0, float y0, float x1, float y1, float z) {
    glBegin(GL_QUADS);
        glVertex3f(x0, y0, z);
        glVertex3f(x1, y0, z);
        glVertex3f(x1, y1, z);
        glVertex3f(x0, y1, z);
    glEnd();
}

// Pantalla: negra apagada; encendida muestra un mini juego animado
void drawScreen() {
    const float x0 = -0.65f, x1 = 0.65f, y0 = 0.90f, y1 = 1.60f, z = 0.20f;
    float k = monitorLevel;

    glBegin(GL_QUADS);
        glColor3f(0.02f + 0.35f * k, 0.02f + 0.08f * k, 0.03f + 0.45f * k);
        glVertex3f(x0, y0, z);
        glVertex3f(x1, y0, z);
        glColor3f(0.02f + 0.03f * k, 0.02f + 0.03f * k, 0.03f + 0.15f * k);
        glVertex3f(x1, y1, z);
        glVertex3f(x0, y1, z);
    glEnd();
    if (k < 0.05f) return;

    const float ground = y0 + 0.10f;
    glColor3f(0.1f * k, 0.7f * k, 0.8f * k);                      // suelo
    screenQuad(x0, y0, x1, ground, z + 0.002f);

    float jump = fabsf(sinf(animTime * 3.5f)) * 0.2f;             // personaje saltando
    glColor3f(1.0f * k, 0.85f * k, 0.2f * k);
    screenQuad(-0.44f, ground + jump, -0.36f, ground + jump + 0.08f, z + 0.004f);

    glColor3f(0.95f * k, 0.25f * k, 0.55f * k);                   // obstaculos
    for (int i = 0; i < 3; i++) {
        float x = 0.60f - fmodf(animTime * 0.45f + i * 0.42f, 1.20f);
        screenQuad(x - 0.025f, ground, x + 0.025f, ground + 0.09f, z + 0.004f);
    }
}

// Monitor (la pantalla mira hacia la silla, +Z)
void drawMonitor() {
    glColor3f(0.15f, 0.15f, 0.15f);
    glPushMatrix();
        glTranslatef(0.0f, 0.815f, 0.3f);
        glScalef(0.6f, 0.05f, 0.25f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.10f, 0.10f, 0.12f);
    glPushMatrix();
        glTranslatef(0.0f, 1.25f, 0.15f);
        glScalef(1.4f, 0.8f, 0.08f);
        drawCube(1.0f);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    drawScreen();
    if (monitorOn) glColor3f(0.2f, 1.0f, 0.4f);                   // LED de encendido
    else           glColor3f(1.0f, 0.25f, 0.2f);
    drawBox(0.6f, 0.875f, 0.195f, 0.03f, 0.015f, 0.01f);
    glEnable(GL_LIGHTING);
}

// Ventilador RGB del lateral del PC; gira segun fanAngle
void drawPCFan() {
    glPushMatrix();
        glTranslatef(-1.108f, 1.15f, 0.35f);
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f);      // el toro de GLUT mira a +Z; se orienta a +X

        float h = animTime * 0.8f;                // anillo que recorre el arcoiris
        glowColor(0.5f + 0.5f * sinf(h), 0.5f + 0.5f * sinf(h + 2.09f), 0.5f + 0.5f * sinf(h + 4.19f));
        glutSolidTorus(0.012, 0.16, 8, 32);

        glRotatef(fanAngle, 0.0f, 0.0f, 1.0f);
        glowColor(0.55f, 0.6f, 0.75f);
        for (int i = 0; i < 5; i++) {
            glPushMatrix();
                glRotatef(i * 72.0f, 0.0f, 0.0f, 1.0f);
                glTranslatef(0.075f, 0.0f, 0.0f);
                glRotatef(25.0f, 1.0f, 0.0f, 0.0f);   // inclinacion del aspa
                glScalef(0.12f, 0.045f, 0.006f);
                drawCube(1.0f);
            glPopMatrix();
        }
        glutSolidSphere(0.03, 12, 12);
    glPopMatrix();
}

// PC con luces RGB y ventilador lateral
void drawPC() {
    glColor3f(0.12f, 0.12f, 0.14f);
    glPushMatrix();
        glTranslatef(-1.35f, 1.15f, 0.35f);
        glScalef(0.45f, 0.85f, 0.55f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.08f, 0.08f, 0.10f);
    glPushMatrix();
        glTranslatef(-1.35f, 1.15f, 0.63f);
        glScalef(0.42f, 0.80f, 0.04f);
        drawCube(1.0f);
    glPopMatrix();

    // Panel lateral de vidrio oscuro (lado que mira al monitor)
    glColor3f(0.04f, 0.04f, 0.07f);
    drawBox(-1.12f, 1.15f, 0.35f, 0.01f, 0.72f, 0.46f);

    beginGlow(pcLevel);

    // Luces del PC
    glowColor(0.2f, 0.9f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.35f, 1.52f, 0.64f);
        glScalef(0.35f, 0.04f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glowColor(0.9f, 0.3f, 0.9f);
    glPushMatrix();
        glTranslatef(-1.35f, 1.25f, 0.64f);
        glScalef(0.30f, 0.04f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glowColor(0.3f, 0.5f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.35f, 0.98f, 0.64f);
        glScalef(0.28f, 0.04f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glowColor(0.4f, 0.8f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.55f, 1.15f, 0.645f);
        glScalef(0.03f, 0.6f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    drawPCFan();
    endGlow();
}

// Letrero neon (dentro de beginGlow/endGlow)
void drawNeonSign() {
    glowColor(0.1f, 0.85f, 1.0f);                         // marco, detras del panel
    drawBox(0.0f, 3.15f, -3.895f, 2.7f, 0.65f, 0.04f);
    glowColor(0.05f, 0.05f, 0.08f);                       // panel oscuro
    drawBox(0.0f, 3.15f, -3.86f, 2.6f, 0.55f, 0.03f);

    const char* text = "GAMING ZONE";
    float len = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)text);
    float s = 2.3f / len;                                 // el texto ocupa 2.3 de ancho
    glowColor(0.3f, 0.95f, 1.0f);
    glLineWidth(3.0f);
    glPushMatrix();
        glTranslatef(-len * s / 2.0f, 3.0f, -3.84f);
        glScalef(s, s, s);
        for (const char* c = text; *c; c++)
            glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix();
    glLineWidth(1.0f);
}

// Silla (se dibuja en chairX/chairZ porque se desliza al sentarse)
void drawChair() {
    glPushMatrix();
    glTranslatef(chairX, 0.0f, chairZ);

    glColor3f(0.15f, 0.15f, 0.22f);
    glPushMatrix();                                       // asiento
        glTranslatef(0.0f, 0.55f, 0.0f);
        glScalef(0.7f, 0.12f, 0.7f);
        drawCube(1.0f);
    glPopMatrix();

    glPushMatrix();                                       // respaldo
        glTranslatef(0.0f, 1.1f, 0.3f);
        glScalef(0.7f, 1.0f, 0.12f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.10f, 0.10f, 0.12f);
    glPushMatrix();                                       // base
        glTranslatef(0.0f, 0.25f, 0.0f);
        glScalef(0.5f, 0.1f, 0.5f);
        drawCube(1.0f);
    glPopMatrix();

    drawBox(0.0f, 0.395f, 0.0f, 0.08f, 0.19f, 0.08f);     // piston
    for (int sx = -1; sx <= 1; sx += 2) {                 // ruedas
        for (int sz = -1; sz <= 1; sz += 2) {
            drawBox(sx * 0.2f, 0.15f, sz * 0.2f, 0.04f, 0.1f, 0.04f);
            glPushMatrix();
                glTranslatef(sx * 0.2f, 0.05f, sz * 0.2f);
                glutSolidSphere(0.05, 10, 10);
            glPopMatrix();
        }
    }
    glPopMatrix();
}

// Estantes
void drawShelf(float x, float y, float z) {
    glColor3f(0.40f, 0.28f, 0.18f);
    glPushMatrix();
        glTranslatef(x, y, z);
        glScalef(1.8f, 0.06f, 0.35f);
        drawCube(1.0f);
    glPopMatrix();
}

// Foco con su base en el techo
void drawSpotFixture(float x, float z) {
    glColor3f(0.9f, 0.9f, 0.95f);
    glPushMatrix();
        glTranslatef(x, 3.9f, z);
        glScalef(0.3f, 0.12f, 0.3f);
        drawCube(1.0f);
    glPopMatrix();
}

// Cono de luz
void drawLightCone(float x, float z) {
    glPushMatrix();
        glTranslatef(x, 1.4f, z);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);  // punta hacia arriba, base abajo
        glColor4f(1.0f, 1.0f, 0.85f, 0.12f);
        glutSolidCone(0.7, 2.5, 24, 1);
    glPopMatrix();
}

void drawSpotlights() {
    float xs[3] = {-2.0f, 0.0f, 2.0f};
    for (int i = 0; i < 3; i++) drawSpotFixture(xs[i], -2.8f);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (int i = 0; i < 3; i++) drawLightCone(xs[i], -2.8f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// Fila de libros de alturas y grosores variados
void drawBookRow(float x, float y, float z, float length, bool alongZ) {
    float pos = 0.0f;
    int i = 0;
    while (true) {
        float w = 0.10f + 0.03f * ((i * 5) % 3);
        if (pos + w > length) break;
        float h = 0.30f + 0.05f * ((i * 7) % 4);

        glColor3fv(bookColors[i % 6]);
        glPushMatrix();
            if (alongZ) {
                glTranslatef(x, y + h / 2, z + pos + w / 2);
                glScalef(0.3f, h, w * 0.9f);
            } else {
                glTranslatef(x + pos + w / 2, y + h / 2, z);
                glScalef(w * 0.9f, h, 0.25f);
            }
            drawCube(1.0f);
        glPopMatrix();

        pos += w;
        i++;
    }
}

// Librero
void drawSideBookshelf(float side, float zc) {
    float x = side * 4.7f;
    float len = 2.4f;

    glColor3f(0.40f, 0.28f, 0.18f);
    for (int k = 0; k < 5; k++) {
        glPushMatrix();
            glTranslatef(x, 0.05f + k * 0.55f, zc);
            glScalef(0.4f, 0.06f, len);
            drawCube(1.0f);
        glPopMatrix();
    }
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
            glTranslatef(x, 1.15f, zc + s * len / 2);
            glScalef(0.4f, 2.3f, 0.06f);
            drawCube(1.0f);
        glPopMatrix();
    }
    for (int k = 0; k < 4; k++)
        drawBookRow(x, 0.05f + k * 0.55f + 0.03f, zc - len / 2 + 0.05f, len - 0.1f, true);
}

// Libros
void drawShelfBooks() {
    drawBookRow(-4.05f, 2.43f, -3.7f, 1.7f, false);
    drawBookRow(-4.05f, 1.83f, -3.7f, 1.7f, false);
    drawBookRow( 2.35f, 2.43f, -3.7f, 1.7f, false);
}

// Posters
void drawPoster(float side, float y, float z, float w, float h,
                float fr, float fg, float fb,   // color del marco
                float pr, float pg, float pb) { // color del fondo
    glPushMatrix();
        glTranslatef(side * 4.89f, y, z);
        glColor3f(fr, fg, fb);
        glPushMatrix();
            glScalef(0.05f, h + 0.1f, w + 0.1f);
            drawCube(1.0f);
        glPopMatrix();

        glTranslatef(-side * 0.03f, 0.0f, 0.0f);
        glColor3f(pr, pg, pb);
        glPushMatrix();
            glScalef(0.03f, h, w);
            drawCube(1.0f);
        glPopMatrix();

        // detalles
        glColor3f(1.0f, 0.85f, 0.2f);
        glTranslatef(-side * 0.02f, 0.2f * h, 0.0f);
        glPushMatrix();
            glScalef(0.02f, h * 0.15f, w * 0.6f);
            drawCube(1.0f);
        glPopMatrix();
        glColor3f(0.2f, 0.9f, 1.0f);
        glTranslatef(0.0f, -0.3f * h, 0.0f);
        glPushMatrix();
            glScalef(0.02f, h * 0.15f, w * 0.4f);
            drawCube(1.0f);
        glPopMatrix();
    glPopMatrix();
}

void drawPosters() {
    glDisable(GL_LIGHTING);
    drawPoster(-1.0f, 2.0f, -2.6f, 1.0f, 1.4f,  1.0f, 0.2f, 0.6f,  0.4f, 0.2f, 0.7f);
    drawPoster(-1.0f, 2.0f,  2.6f, 1.0f, 1.4f,  0.2f, 0.9f, 1.0f,  0.6f, 0.2f, 0.5f);
    drawPoster( 1.0f, 2.0f, -2.6f, 1.0f, 1.4f,  0.2f, 0.9f, 1.0f,  0.2f, 0.2f, 0.6f);
    drawPoster( 1.0f, 2.0f,  2.6f, 1.0f, 1.4f,  1.0f, 0.2f, 0.6f,  0.5f, 0.3f, 0.8f);
    glEnable(GL_LIGHTING);
}

// Skate naranja
void drawSkate(float x, float z) {
    glPushMatrix();
        glTranslatef(x, 0.0f, z);

        glColor3f(1.0f, 0.45f, 0.1f);
        glPushMatrix();
            glTranslatef(0.0f, 0.13f, 0.0f);
            glScalef(0.9f, 0.04f, 0.25f);
            drawCube(1.0f);
        glPopMatrix();

        glColor3f(0.05f, 0.05f, 0.05f);
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                glPushMatrix();
                    glTranslatef(sx * 0.3f, 0.06f, sz * 0.13f);
                    glutSolidSphere(0.06, 12, 12);
                glPopMatrix();
            }
        }
    glPopMatrix();
}

// Interruptor de pared que controla las luces neon
void drawWallSwitch() {
    const float z = ROOM_MIN_Z;
    glColor3f(0.85f, 0.85f, 0.9f);
    drawBox(SWITCH_X, SWITCH_Y, z + 0.01f, 0.18f, 0.28f, 0.02f);

    glColor3f(0.25f, 0.25f, 0.3f);                         // palanca arriba/abajo
    glPushMatrix();
        glTranslatef(SWITCH_X, SWITCH_Y, z + 0.025f);
        glRotatef(neonOn ? 20.0f : -20.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.05f, 0.1f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glDisable(GL_LIGHTING);
    if (neonOn) glColor3f(0.2f, 1.0f, 0.4f);
    else        glColor3f(1.0f, 0.2f, 0.2f);
    glPushMatrix();
        glTranslatef(SWITCH_X, SWITCH_Y + 0.1f, z + 0.025f);
        glutSolidSphere(0.012, 8, 8);
    glPopMatrix();
    glEnable(GL_LIGHTING);
}

// Trofeo cargado desde OBJ: gris mientras la mision no se completa, dorado y girando despues
void drawTrophy() {
    if (!trophyList) return;
    glPushMatrix();
        glTranslatef(TROPHY_X, DESK_TOP, TROPHY_Z);
        glRotatef(trophyAngle, 0.0f, 1.0f, 0.0f);
        glScalef(0.34f, 0.34f, 0.34f);
        if (missionDone) {
            GLfloat spec[] = { 1.0f, 0.9f, 0.6f, 1.0f };
            glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
            glMaterialf(GL_FRONT, GL_SHININESS, 40.0f);
            glColor3f(0.95f, 0.72f, 0.18f);
        } else {
            glColor3f(0.28f, 0.28f, 0.32f);
        }
        glCallList(trophyList);
        GLfloat noSpec[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT, GL_SPECULAR, noSpec);
    glPopMatrix();
}

// Contorno parpadeante del objeto al que se apunta
void drawHighlight() {
    if (currentTarget == T_NONE) return;
    Box b = targetBox(currentTarget);
    float p = 0.5f + 0.5f * sinf(animTime * 6.0f);

    glDisable(GL_LIGHTING);
    glColor3f(0.6f + 0.4f * p, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glPushMatrix();
        glTranslatef((b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, (b.min[2] + b.max[2]) / 2);
        glScalef(b.max[0] - b.min[0] + 0.03f, b.max[1] - b.min[1] + 0.03f, b.max[2] - b.min[2] + 0.03f);
        glutWireCube(1.0);
    glPopMatrix();
    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// Luz principal + luz azul del monitor + relleno cian del neon
void setupLights() {
    GLfloat lightPos[] = {2.0f, 3.5f, 3.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    GLfloat monPos[] = {0.0f, 1.25f, 0.5f, 1.0f};
    GLfloat monCol[] = {0.25f * monitorLevel, 0.45f * monitorLevel, 1.0f * monitorLevel, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, monPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, monCol);

    GLfloat neonPos[] = {0.0f, 3.5f, 0.0f, 1.0f};
    GLfloat neonCol[] = {0.06f * neonLevel, 0.22f * neonLevel, 0.30f * neonLevel, 1.0f};
    glLightfv(GL_LIGHT2, GL_POSITION, neonPos);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, neonCol);
}

// ======================================================================
//  VENTANA Y LIBRERO REUBICADO
// ======================================================================

void drawWindowFrame() {
    const float zc = (WIN_Z0 + WIN_Z1) / 2, yc = (WIN_Y0 + WIN_Y1) / 2;
    const float w = WIN_Z1 - WIN_Z0, h = WIN_Y1 - WIN_Y0;
    glDisable(GL_LIGHTING);
    glColor3f(0.10f, 0.10f, 0.14f);
    drawBox(5.0f, WIN_Y1 - 0.03f, zc, 0.17f, 0.06f, w);           // dintel
    drawBox(5.0f, yc, WIN_Z0 + 0.03f, 0.17f, h, 0.06f);           // jambas
    drawBox(5.0f, yc, WIN_Z1 - 0.03f, 0.17f, h, 0.06f);
    drawBox(5.0f, yc, zc, 0.06f, h, 0.04f);                       // division central
    glColor3f(0.16f, 0.16f, 0.20f);
    drawBox(4.96f, WIN_Y0 + 0.03f, zc, 0.26f, 0.06f, w + 0.1f);   // alfeizar
    glEnable(GL_LIGHTING);
}

// Contorno neon magenta de la ventana (dentro de beginGlow/endGlow)
void drawWindowNeon() {
    const float x = ROOM_MAX_X - 0.01f, t = 0.04f;
    const float zc = (WIN_Z0 + WIN_Z1) / 2, yc = (WIN_Y0 + WIN_Y1) / 2;
    glowColor(1.0f, 0.25f, 0.75f);
    drawBox(x, WIN_Y1 + t / 2, zc, t, t, WIN_Z1 - WIN_Z0 + 2 * t);
    drawBox(x, yc, WIN_Z0 - t / 2, t, WIN_Y1 - WIN_Y0, t);
    drawBox(x, yc, WIN_Z1 + t / 2, t, WIN_Y1 - WIN_Y0, t);
}

// Vidrio translucido (pasada transparente)
void drawWindowGlass() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
        glColor4f(0.55f, 0.75f, 1.0f, 0.08f);
        glVertex3f(5.0f, WIN_Y0, WIN_Z0);
        glVertex3f(5.0f, WIN_Y0, WIN_Z1);
        glColor4f(0.75f, 0.85f, 1.0f, 0.16f);   // mas reflejante arriba
        glVertex3f(5.0f, WIN_Y1, WIN_Z1);
        glVertex3f(5.0f, WIN_Y1, WIN_Z0);
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// El librero de la pared derecha paso a la pared frontal para dejar sitio a la ventana
void drawFrontBookshelf() {
    glPushMatrix();
        glTranslatef(1.25f, 0.0f, 3.68f);
        glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
        glTranslatef(-4.7f, 0.0f, 0.0f);   // drawSideBookshelf dibuja en x = 4.7
        drawSideBookshelf(1.0f, 0.0f);
    glPopMatrix();
}

// ======================================================================
//  CALLE EXTERIOR NEOFUTURISTA (se ve por la ventana)
//  Lo estatico (cielo, calle, edificios) va en display lists;
//  lo animado (autos flotantes, tren, holograma, balizas) se dibuja cada cuadro.
// ======================================================================

GLuint skyList = 0, cityList = 0;
std::vector<Vec3> beacons;   // luces rojas en las azoteas mas altas

// Pseudoaleatorio determinista: la ciudad sale igual en cada ejecucion
float hash01(unsigned n) {
    n = (n << 13) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return (n & 0x7fffffffu) / 2147483647.0f;
}

// Caja sin iluminacion con sombreado falso por cara (la cara -X mira hacia la habitacion)
void shadedBox(float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b) {
    glBegin(GL_QUADS);
        glColor3f(r, g, b);
        glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
        glColor3f(r * 0.6f, g * 0.6f, b * 0.6f);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
        glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
        glColor3f(r * 1.3f, g * 1.3f, b * 1.3f);
        glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
        glColor3f(r * 0.4f, g * 0.4f, b * 0.4f);
        glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glEnd();
}

const float CITY_COLORS[4][3] = {
    {0.2f, 0.9f, 1.0f}, {1.0f, 0.3f, 0.8f}, {1.0f, 0.8f, 0.35f}, {0.6f, 0.5f, 1.0f}
};

// Torre con ventanas encendidas al azar y, a veces, franjas de neon
void buildTower(float x0, float z0, float x1, float z1, float top, unsigned seed, float litRatio) {
    float s = 0.04f + 0.04f * hash01(seed);
    shadedBox(x0, STREET_Y, z0, x1, top, z1, s, s, s + 0.04f);

    glBegin(GL_QUADS);
    for (float y = STREET_Y + 1.5f; y < top - 1.0f; y += 1.2f) {
        for (float z = z0 + 0.4f; z + 0.8f < z1; z += 0.9f) {
            unsigned h = seed * 7919u + (unsigned)((y + 100.0f) * 13.0f) * 31u + (unsigned)((z + 300.0f) * 17.0f);
            if (hash01(h) > litRatio) continue;
            const float* c = CITY_COLORS[(unsigned)(hash01(h + 1) * 4.0f) % 4];
            float k = 0.35f + 0.65f * hash01(h + 2);
            glColor3f(c[0] * k, c[1] * k, c[2] * k);
            glVertex3f(x0 - 0.02f, y, z);
            glVertex3f(x0 - 0.02f, y, z + 0.5f);
            glVertex3f(x0 - 0.02f, y + 0.6f, z + 0.5f);
            glVertex3f(x0 - 0.02f, y + 0.6f, z);
        }
    }
    glEnd();

    if (hash01(seed + 5) < 0.6f) {
        const float* c = CITY_COLORS[(unsigned)(hash01(seed + 6) * 2.0f) % 2];   // cian o magenta
        shadedBox(x0 - 0.06f, top - 0.25f, z0, x0, top, z1, c[0], c[1], c[2]);
        shadedBox(x0 - 0.06f, STREET_Y, z0, x0, top, z0 + 0.08f, c[0], c[1], c[2]);
        shadedBox(x0 - 0.06f, STREET_Y, z1 - 0.08f, x0, top, z1, c[0], c[1], c[2]);
    }
}

void streetLamp(float x, float z, float dir) {
    const float top = STREET_Y + 3.5f, armEnd = x + dir * 0.9f;
    shadedBox(x - 0.05f, STREET_Y, z - 0.05f, x + 0.05f, top, z + 0.05f, 0.15f, 0.15f, 0.2f);
    shadedBox(std::min(x, armEnd), top - 0.05f, z - 0.04f, std::max(x, armEnd), top + 0.03f, z + 0.04f,
              0.15f, 0.15f, 0.2f);
    shadedBox(armEnd - 0.2f, top - 0.12f, z - 0.12f, armEnd + 0.2f, top - 0.05f, z + 0.12f,
              0.4f, 1.0f, 1.0f);
}

// Cielo: caja con degradado (horizonte magenta -> violeta -> azul noche), luna y estrellas
void drawSky() {
    const float X = 150.0f, Z = 200.0f;
    const float yb[3] = { -30.0f, 40.0f, 160.0f };
    const float col[3][3] = { {0.30f, 0.07f, 0.30f}, {0.10f, 0.04f, 0.20f}, {0.02f, 0.02f, 0.06f} };

    glBegin(GL_QUADS);
    for (int i = 0; i < 2; i++) {
        glColor3fv(col[i]);     glVertex3f(X, yb[i], -Z);     glVertex3f(X, yb[i], Z);
        glColor3fv(col[i + 1]); glVertex3f(X, yb[i + 1], Z);  glVertex3f(X, yb[i + 1], -Z);
        for (int side = -1; side <= 1; side += 2) {
            glColor3fv(col[i]);     glVertex3f(0.0f, yb[i], side * Z);     glVertex3f(X, yb[i], side * Z);
            glColor3fv(col[i + 1]); glVertex3f(X, yb[i + 1], side * Z);    glVertex3f(0.0f, yb[i + 1], side * Z);
        }
    }
    glColor3fv(col[2]);
    glVertex3f(0.0f, yb[2], -Z); glVertex3f(X, yb[2], -Z); glVertex3f(X, yb[2], Z); glVertex3f(0.0f, yb[2], Z);
    glEnd();

    glColor3f(0.85f, 0.82f, 1.0f);
    glPushMatrix();
        glTranslatef(140.0f, 55.0f, 25.0f);
        glutSolidSphere(6.0, 24, 24);
    glPopMatrix();

    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (unsigned i = 0; i < 80; i++) {
        float b = 0.5f + 0.5f * hash01(i * 3 + 900);
        glColor3f(b, b, b);
        glVertex3f(148.0f, 35.0f + 110.0f * hash01(i * 3 + 901), -190.0f + 380.0f * hash01(i * 3 + 902));
    }
    glEnd();
    glPointSize(1.0f);
}

void buildCity() {
    skyList = glGenLists(1);
    glNewList(skyList, GL_COMPILE);
    drawSky();
    glEndList();

    cityList = glGenLists(1);
    glNewList(cityList, GL_COMPILE);

    // Veredas, pista (un poco mas baja) y suelo detras de las torres
    shadedBox(4.9f,  STREET_Y - 0.4f, -200.0f, 8.5f,   STREET_Y,         200.0f, 0.10f, 0.10f, 0.14f);
    shadedBox(8.5f,  STREET_Y - 0.4f, -200.0f, 18.5f,  STREET_Y - 0.15f, 200.0f, 0.03f, 0.03f, 0.06f);
    shadedBox(18.5f, STREET_Y - 0.4f, -200.0f, 21.5f,  STREET_Y,         200.0f, 0.10f, 0.10f, 0.14f);
    shadedBox(21.5f, STREET_Y - 0.4f, -200.0f, 150.0f, STREET_Y - 0.05f, 200.0f, 0.05f, 0.05f, 0.08f);

    // Bordillos luminosos y linea central discontinua
    shadedBox(8.45f,  STREET_Y - 0.15f, -200.0f, 8.55f,  STREET_Y + 0.02f, 200.0f, 0.2f, 0.9f, 1.0f);
    shadedBox(18.45f, STREET_Y - 0.15f, -200.0f, 18.55f, STREET_Y + 0.02f, 200.0f, 1.0f, 0.3f, 0.8f);
    for (float z = -200.0f; z < 200.0f; z += 4.0f)
        shadedBox(13.45f, STREET_Y - 0.15f, z, 13.55f, STREET_Y - 0.13f, z + 2.0f, 0.9f, 0.9f, 1.0f);

    for (float z = -96.0f; z <= 96.0f; z += 12.0f) {
        streetLamp(8.1f, z, 1.0f);
        streetLamp(18.9f, z + 6.0f, -1.0f);
    }

    // Via elevada del tren con sus pilares
    const float railX = 19.6f, railY = STREET_Y + 13.0f;
    shadedBox(railX - 0.3f, railY - 0.6f, -200.0f, railX + 0.3f, railY - 0.4f, 200.0f, 0.12f, 0.12f, 0.16f);
    shadedBox(railX - 0.31f, railY - 0.63f, -200.0f, railX + 0.31f, railY - 0.6f, 200.0f, 0.2f, 0.9f, 1.0f);
    for (float z = -180.0f; z <= 180.0f; z += 20.0f)
        shadedBox(railX - 0.15f, STREET_Y, z - 0.15f, railX + 0.15f, railY - 0.6f, z + 0.15f, 0.1f, 0.1f, 0.13f);

    // Torres al otro lado de la calle
    float z = -96.0f;
    for (unsigned i = 0; z < 96.0f; i++) {
        float w  = 4.0f + 5.0f * hash01(i * 3 + 1);
        float h  = 12.0f + 34.0f * hash01(i * 3 + 2);
        if (z < 10.0f && z + w > -10.0f)
            h = 6.0f + 6.0f * hash01(i * 3 + 2);   // bajas frente a la ventana para ver el skyline
        float x0 = 21.8f + 1.5f * hash01(i * 3 + 3);
        float top = STREET_Y + h;
        buildTower(x0, z, x0 + 7.0f, z + w, top, i * 101 + 7, 0.55f);
        if (h > 34.0f) {   // antena con baliza
            float zc = z + w / 2;
            shadedBox(x0 + 3.45f, top, zc - 0.05f, x0 + 3.55f, top + 2.0f, zc + 0.05f, 0.2f, 0.2f, 0.25f);
            Vec3 b = { x0 + 3.5f, top + 2.0f, zc };
            beacons.push_back(b);
        }
        z += w + 0.8f;
    }

    // Skyline lejano: mas alto y con menos luces (la niebla lo difumina)
    z = -110.0f;
    for (unsigned i = 0; z < 110.0f; i++) {
        float w  = 6.0f + 8.0f * hash01(i * 5 + 11);
        float h  = 30.0f + 50.0f * hash01(i * 5 + 12);
        float x0 = 42.0f + 10.0f * hash01(i * 5 + 13);
        buildTower(x0, z, x0 + 10.0f, z + w, STREET_Y + h, i * 211 + 3, 0.25f);
        z += w + 1.5f;
    }
    glEndList();
}

struct Lane { float x, y, speed, offset, r, g, b; };

// Carriles de autos flotantes: tres en el aire y dos sobre la pista
const Lane LANES[5] = {
    { 11.0f, 1.2f,            7.0f,  0.0f, 1.0f, 0.3f, 0.8f },
    { 15.5f, 3.8f,           -9.0f, 20.0f, 0.2f, 0.9f, 1.0f },
    { 13.0f, 7.0f,            5.0f, 50.0f, 1.0f, 0.8f, 0.3f },
    { 10.5f, STREET_Y + 0.2f, 6.0f, 10.0f, 0.9f, 0.2f, 0.3f },
    { 16.5f, STREET_Y + 0.2f,-7.5f, 35.0f, 0.3f, 0.6f, 1.0f }
};

void drawHoverCar(float x, float y, float z, float dir, const Lane& l) {
    shadedBox(x - 0.35f, y - 0.12f, z - 0.9f, x + 0.35f, y + 0.12f, z + 0.9f, 0.12f, 0.12f, 0.16f);   // carroceria
    shadedBox(x - 0.28f, y + 0.12f, z - 0.4f, x + 0.28f, y + 0.30f, z + 0.4f, 0.05f, 0.09f, 0.16f);   // cabina
    shadedBox(x - 0.30f, y - 0.16f, z - 0.8f, x + 0.30f, y - 0.12f, z + 0.8f, l.r, l.g, l.b);         // luz inferior
    float front = z + dir * 0.9f, back = z - dir * 0.9f;
    shadedBox(x - 0.3f, y - 0.05f, front - 0.03f, x + 0.3f, y + 0.05f, front + 0.03f, 1.0f, 1.0f, 0.9f);
    shadedBox(x - 0.3f, y - 0.05f, back - 0.03f,  x + 0.3f, y + 0.05f, back + 0.03f,  1.0f, 0.1f, 0.2f);
}

void drawTraffic() {
    for (int i = 0; i < 5; i++) {
        const Lane& l = LANES[i];
        float dir = (l.speed > 0.0f) ? 1.0f : -1.0f;
        for (int k = 0; k < 3; k++) {
            float d = fmodf(animTime * fabsf(l.speed) + l.offset + k * 66.0f, 200.0f);
            float z = (dir > 0.0f) ? -100.0f + d : 100.0f - d;
            float bob = 0.05f * sinf(animTime * 2.0f + k + i);   // flotan suavemente
            drawHoverCar(l.x, l.y + bob, z, dir, l);
        }
    }
}

// Tren maglev de 4 vagones que pasa cada ~22 s
void drawTrain() {
    const float x = 19.6f, y = STREET_Y + 13.0f;
    float head = 200.0f - fmodf(animTime * 18.0f, 400.0f);
    for (int i = 0; i < 4; i++) {
        float z0 = head + i * 6.3f;
        shadedBox(x - 0.6f, y - 0.35f, z0, x + 0.6f, y + 0.85f, z0 + 6.0f, 0.75f, 0.78f, 0.85f);
        shadedBox(x - 0.62f, y + 0.25f, z0 + 0.3f, x - 0.6f, y + 0.6f, z0 + 5.7f, 0.3f, 0.95f, 1.0f);
    }
}

void drawBeacons() {
    if (fmodf(animTime, 1.6f) > 0.5f) return;   // parpadeo
    glColor3f(1.0f, 0.15f, 0.15f);
    for (size_t i = 0; i < beacons.size(); i++) {
        glPushMatrix();
            glTranslatef(beacons[i].x, beacons[i].y, beacons[i].z);
            glutSolidSphere(0.18, 8, 8);
        glPopMatrix();
    }
}

void drawCity() {
    glDisable(GL_LIGHTING);
    glCallList(skyList);
    glEnable(GL_FOG);   // la niebla da profundidad a la ciudad (el cielo no la usa)
    glCallList(cityList);
    drawTraffic();
    drawTrain();
    glDisable(GL_FOG);
    drawBeacons();
    glEnable(GL_LIGHTING);
}

// Holograma flotante frente a las torres (pasada transparente, mezcla aditiva)
void drawHologram() {
    const float x = 21.0f, yc = 6.0f, zc = 0.0f, hw = 4.5f, hh = 2.2f;
    float g = sinf(animTime * 1.7f) * sinf(animTime * 11.0f);
    float flicker = (g > 0.9f) ? 0.35f : 1.0f;   // falla ocasional de la proyeccion

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    float sy = yc + hh - fmodf(animTime * 1.5f, 2.0f * hh);   // linea de barrido que baja
    glBegin(GL_QUADS);
        glColor4f(1.0f, 0.2f, 0.8f, 0.22f * flicker);
        glVertex3f(x, yc - hh, zc - hw);
        glVertex3f(x, yc - hh, zc + hw);
        glColor4f(0.2f, 0.8f, 1.0f, 0.22f * flicker);
        glVertex3f(x, yc + hh, zc + hw);
        glVertex3f(x, yc + hh, zc - hw);

        glColor4f(0.6f, 1.0f, 1.0f, 0.35f * flicker);
        glVertex3f(x - 0.01f, sy - 0.08f, zc - hw);
        glVertex3f(x - 0.01f, sy - 0.08f, zc + hw);
        glVertex3f(x - 0.01f, sy + 0.08f, zc + hw);
        glVertex3f(x - 0.01f, sy + 0.08f, zc - hw);
    glEnd();

    glLineWidth(2.0f);
    glColor4f(0.3f, 0.95f, 1.0f, 0.9f * flicker);
    glBegin(GL_LINE_LOOP);
        glVertex3f(x - 0.02f, yc - hh, zc - hw);
        glVertex3f(x - 0.02f, yc - hh, zc + hw);
        glVertex3f(x - 0.02f, yc + hh, zc + hw);
        glVertex3f(x - 0.02f, yc + hh, zc - hw);
    glEnd();

    const char* text = "NEO CITY";
    float len = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)text);
    float s = 6.5f / len;
    glLineWidth(3.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 0.85f * flicker);
    glPushMatrix();
        glTranslatef(x - 0.03f, yc - 0.55f, zc - len * s / 2.0f);
        glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);   // el texto mira hacia la habitacion (-X)
        glScalef(s, s, s);
        for (const char* c = text; *c; c++)
            glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
    glPopMatrix();
    glLineWidth(1.0f);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ======================================================================
//  LOGICA: interaccion, mision, silla y movimiento
// ======================================================================

void showMessage(const char* text, float r, float g, float b) {
    strncpy(hudMsg, text, sizeof(hudMsg) - 1);
    hudMsg[sizeof(hudMsg) - 1] = '\0';
    hudMsgTime = 2.5f;
    hudMsgColor[0] = r; hudMsgColor[1] = g; hudMsgColor[2] = b;
}

bool* deviceState(int t) {
    if (t == T_PC) return &pcOn;
    if (t == T_MONITOR) return &monitorOn;
    return &neonOn;
}

const char* targetPrompt(int t) {
    if (t == T_CHAIR) return "[F] Sentarse";
    bool on = *deviceState(t);
    if (!missionDone && on) return "Encendido";
    switch (t) {
        case T_PC:      return on ? "[F] Apagar PC" : "[F] Encender PC";
        case T_MONITOR: return on ? "[F] Apagar monitor" : "[F] Encender monitor";
        default:        return on ? "[F] Apagar luces neon" : "[F] Encender luces neon";
    }
}

// Al encenderse, el neon parpadea como un tubo real antes de estabilizarse
float neonPower() {
    if (neonOn && neonOnTime < 0.7f && sinf(neonOnTime * 60.0f) < -0.3f) return 0.2f;
    return neonLevel;
}

Pose currentPose() {
    Pose p = { camX, camY, camZ, yaw, pitch, chairX, chairZ };
    return p;
}

void startSeatTransition(SeatState state, Pose to) {
    poseFrom = currentPose();
    to.yaw = poseFrom.yaw + fmodf(to.yaw - poseFrom.yaw + 540.0f, 360.0f) - 180.0f;  // giro mas corto
    poseTo = to;
    seatState = state;
    seatT = 0.0f;
    velX = velY = velZ = 0.0f;
}

// La silla se desliza frente al monitor y la camara baja a la altura de sentado
void sitDown() {
    Pose p = { SEAT_X, SEATED_EYE, SEAT_Z, -90.0f, -8.0f, SEAT_X, SEAT_Z };
    startSeatTransition(SITTING_DOWN, p);
}

// Se levanta hacia el costado para no atravesar el respaldo
void standUp() {
    Pose p = { chairX + 0.75f, EYE_HEIGHT, chairZ, yaw, pitch, chairX, chairZ };
    startSeatTransition(STANDING_UP, p);
}

void updateSeatTransition(float dt) {
    if (seatState != SITTING_DOWN && seatState != STANDING_UP) return;
    seatT = std::min(1.0f, seatT + dt / 0.8f);
    float s = seatT * seatT * (3.0f - 2.0f * seatT);   // smoothstep
    camX   = lerpf(poseFrom.x, poseTo.x, s);
    camY   = lerpf(poseFrom.y, poseTo.y, s);
    camZ   = lerpf(poseFrom.z, poseTo.z, s);
    yaw    = lerpf(poseFrom.yaw, poseTo.yaw, s);
    pitch  = lerpf(poseFrom.pitch, poseTo.pitch, s);
    chairX = lerpf(poseFrom.chairX, poseTo.chairX, s);
    chairZ = lerpf(poseFrom.chairZ, poseTo.chairZ, s);
    if (seatT >= 1.0f) {
        seatState = (seatState == SITTING_DOWN) ? SEATED : STANDING;
        clampLook();
    }
}

// Mision: encender PC -> monitor -> neon en ese orden
void interact() {
    if (currentTarget == T_NONE) return;
    if (currentTarget == T_CHAIR) {
        sitDown();
        return;
    }

    bool* dev = deviceState(currentTarget);
    if (!missionDone) {
        if (*dev) return;
        if (currentTarget == MISSION_ORDER[missionStep]) {
            *dev = true;
            missionStep++;
            char buf[96];
            if (missionStep == 3) {
                missionDone = true;
                sprintf(buf, "SETUP COMPLETO en %.1f s! Trofeo desbloqueado", missionTime);
                showMessage(buf, 1.0f, 0.85f, 0.2f);
                playVictoryMusic();
            } else {
                sprintf(buf, "Correcto! (%d/3)", missionStep);
                showMessage(buf, 0.4f, 1.0f, 0.5f);
            }
        } else {
            pcOn = monitorOn = neonOn = false;
            missionStep = 0;
            showMessage("Orden incorrecto: todo se apago. Intenta de nuevo", 1.0f, 0.35f, 0.35f);
        }
        return;
    }

    // Mision completada: los dispositivos se pueden prender y apagar libremente
    if (currentTarget == T_MONITOR && !pcOn) {
        showMessage("El PC esta apagado", 1.0f, 0.6f, 0.3f);
        return;
    }
    *dev = !*dev;
    if (!pcOn) monitorOn = false;
}

void resetMission() {
    pcOn = monitorOn = neonOn = false;
    missionStep = 0;
    missionDone = false;
    missionTime = 0.0f;
    trophyAngle = 0.0f;
    showMessage("Mision reiniciada", 1.0f, 1.0f, 1.0f);
    stopAllMusic();
}

void updateArrowLook(float dt) {
    yaw   += ((arrows[1] ? 1.0f : 0.0f) - (arrows[0] ? 1.0f : 0.0f)) * ROT_SPEED * dt;
    pitch += ((arrows[2] ? 1.0f : 0.0f) - (arrows[3] ? 1.0f : 0.0f)) * ROT_SPEED * dt;
    clampLook();
}

void updateMovement(float dt) {
    float yawRad = yaw * M_PI / 180.0f;
    float fx = cosf(yawRad), fz = sinf(yawRad);   // adelante
    float rx = -fz, rz = fx;                      // derecha

    float f = (keys['w'] ? 1.0f : 0.0f) - (keys['s'] ? 1.0f : 0.0f);
    float r = (keys['d'] ? 1.0f : 0.0f) - (keys['a'] ? 1.0f : 0.0f);
    float u = (keys['q'] ? 1.0f : 0.0f) - (keys['e'] ? 1.0f : 0.0f);

    float wx = fx * f + rx * r, wz = fz * f + rz * r;
    float len = sqrtf(wx * wx + wz * wz);
    if (len > 1.0f) { wx /= len; wz /= len; }    // en diagonal no se va mas rapido

    float k = std::min(1.0f, ACCELERATION * dt);
    velX += (wx * MAX_SPEED - velX) * k;
    velZ += (wz * MAX_SPEED - velZ) * k;
    velY += (u * MAX_SPEED * 0.6f - velY) * k;

    // Eje por eje, asi el jugador se desliza a lo largo de paredes y muebles
    float nx = camX + velX * dt;
    if (canMoveTo(nx, camY, camZ)) camX = nx; else velX = 0.0f;
    float nz = camZ + velZ * dt;
    if (canMoveTo(camX, camY, nz)) camZ = nz; else velZ = 0.0f;
    float ny = clampf(camY + velY * dt, MIN_Y, MAX_Y);
    if (canMoveTo(camX, ny, camZ)) camY = ny; else velY = 0.0f;
}

void update(float dt) {
    animTime += dt;
    if (!missionDone) missionTime += dt;
    if (hudMsgTime > 0.0f) hudMsgTime -= dt;

    updateSeatTransition(dt);
    if (seatState == STANDING) updateMovement(dt);
    if (seatState == STANDING || seatState == SEATED) updateArrowLook(dt);

    pcLevel      = approach(pcLevel,      pcOn      ? 1.0f : 0.0f, dt * 2.5f);
    monitorLevel = approach(monitorLevel, monitorOn ? 1.0f : 0.0f, dt * 3.0f);
    neonLevel    = approach(neonLevel,    neonOn    ? 1.0f : 0.0f, dt * 2.0f);
    neonOnTime   = neonOn ? neonOnTime + dt : 0.0f;

    // La musica del minijuego acompana al monitor: empieza al encenderlo y se corta al apagarlo
    static bool lastMonitorOn = false;
    if (monitorOn != lastMonitorOn) {
        lastMonitorOn = monitorOn;
        if (monitorOn) playGameMusic();
        else           stopAllMusic();
    }
    fanAngle     = fmodf(fanAngle + pcLevel * 900.0f * dt, 360.0f);
    if (missionDone) trophyAngle = fmodf(trophyAngle + 60.0f * dt, 360.0f);

    bool canInteract = (seatState == STANDING || seatState == SEATED);
    currentTarget = canInteract ? findTarget() : T_NONE;
}

// ======================================================================
//  HUD (texto 2D sobre la escena)
// ======================================================================

void drawText(float x, float y, void* font, const char* s) {
    glRasterPos2f(x, y);   // el color debe fijarse antes de esta llamada
    for (; *s; s++) glutBitmapCharacter(font, *s);
}

int textWidth(void* font, const char* s) {
    return glutBitmapLength(font, (const unsigned char*)s);
}

void drawPanel(float x, float y, float w, float h) {
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
    glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();
}

void drawCenteredLabel(float y, void* font, const char* s, float r, float g, float b) {
    int w = textWidth(font, s);
    float x = (winW - w) / 2.0f;
    drawPanel(x - 10, y - 8, w + 20, 28);
    glColor3f(r, g, b);
    drawText(x, y, font, s);
}

void drawHUD() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float cx = winW / 2.0f, cy = winH / 2.0f;
    float top = winH - 12.0f;

    // Mira central
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
        glVertex2f(cx - 8, cy); glVertex2f(cx + 8, cy);
        glVertex2f(cx, cy - 8); glVertex2f(cx, cy + 8);
    glEnd();

    if (currentTarget != T_NONE)
        drawCenteredLabel(cy - 50, GLUT_BITMAP_HELVETICA_18, targetPrompt(currentTarget), 0.4f, 1.0f, 1.0f);
    if (hudMsgTime > 0.0f)
        drawCenteredLabel(cy + 90, GLUT_BITMAP_HELVETICA_18, hudMsg, hudMsgColor[0], hudMsgColor[1], hudMsgColor[2]);

    // Mision (arriba a la derecha)
    char l1[96], l2[96];
    if (missionDone) {
        sprintf(l1, "SETUP COMPLETO en %.1f s", missionTime);
        strcpy(l2, "Trofeo desbloqueado  |  R: jugar de nuevo");
    } else {
        sprintf(l1, "MISION: enciende tu setup en orden (%d/3)", missionStep);
        sprintf(l2, "Pista: energia -> imagen -> ambiente  |  %.1f s", missionTime);
    }
    int w = std::max(textWidth(GLUT_BITMAP_HELVETICA_18, l1), textWidth(GLUT_BITMAP_HELVETICA_12, l2));
    float px = winW - w - 32.0f;
    drawPanel(px, top - 58, w + 20, 58);
    if (missionDone) glColor3f(1.0f, 0.85f, 0.2f);
    else             glColor3f(0.4f, 1.0f, 1.0f);
    drawText(px + 10, top - 26, GLUT_BITMAP_HELVETICA_18, l1);
    glColor3f(0.85f, 0.85f, 0.9f);
    drawText(px + 10, top - 48, GLUT_BITMAP_HELVETICA_12, l2);

    // Controles (arriba a la izquierda)
    if (showHelp) {
        static const char* help[] = {
            "CONTROLES",
            "WASD: mover    Q/E: subir/bajar",
            "Mouse o flechas: mirar",
            "F o click: interactuar",
            "Espacio: levantarse de la silla",
            "M: liberar/capturar el mouse",
            "R: reiniciar mision    H: ocultar ayuda",
            "Esc: salir"
        };
        const int n = sizeof(help) / sizeof(help[0]);
        drawPanel(12, top - 20 - n * 18, 280, n * 18 + 20);
        glColor3f(0.9f, 0.9f, 0.95f);
        for (int i = 0; i < n; i++)
            drawText(22, top - 22 - i * 18, GLUT_BITMAP_HELVETICA_12, help[i]);
    }

    if (!mouseCaptured)
        drawCenteredLabel(40, GLUT_BITMAP_HELVETICA_18, "Click en la ventana para controlar la camara", 1.0f, 1.0f, 1.0f);
    else if (seatState == SEATED)
        drawCenteredLabel(40, GLUT_BITMAP_HELVETICA_18, "[Espacio] Levantarse", 1.0f, 1.0f, 1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ======================================================================
//  CALLBACKS DE GLUT
// ======================================================================

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    float lookX, lookY, lookZ;
    getLookAt(lookX, lookY, lookZ);

    gluLookAt(camX, camY, camZ,
              lookX, lookY, lookZ,
              0.0f, 1.0f, 0.0f);
    computeViewInverse();
    setupLights();

    drawRoom();
    drawWindowFrame();
    drawCity();
    drawDoor();
    drawDesk();
    drawMonitor();
    drawPC();
    drawChair();

    drawShelf(-3.2f, 2.4f, -3.7f);
    drawShelf(-3.2f, 1.8f, -3.7f);
    drawShelf(3.2f, 2.4f, -3.7f);

    drawShelfBooks();
    drawSideBookshelf(-1.0f, 0.0f);   // izquierda
    drawFrontBookshelf();
    drawPosters();
    drawSkate(-1.5f, 2.5f);
    drawWallSwitch();
    drawTrophy();

    beginGlow(neonPower());
    drawNeonEdges();
    drawNeonSign();
    drawWindowNeon();
    endGlow();

    drawHighlight();

    // Transparentes despues de lo opaco, de lo mas lejano a lo mas cercano
    drawHologram();
    drawWindowGlass();
    drawSpotlights();
    drawHUD();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    winW = w;
    winH = h;
    float ratio = (float)w / (float)h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, ratio, 0.1f, 400.0f);   // lejano para alcanzar el cielo exterior
    glMatrixMode(GL_MODELVIEW);
}

void captureMouse(bool on) {
    mouseCaptured = on;
    glutSetCursor(on ? GLUT_CURSOR_NONE : GLUT_CURSOR_LEFT_ARROW);
    if (on) glutWarpPointer(winW / 2, winH / 2);
}

// Mira con el mouse: se mide el desplazamiento desde el centro y se recentra el cursor
void mouseMove(int x, int y) {
    if (!mouseCaptured) return;
    int dx = x - winW / 2, dy = y - winH / 2;
    if (dx == 0 && dy == 0) return;

    // Un salto grande indica que el cursor volvio a entrar a la ventana: solo se recentra
    bool canLook = (seatState == STANDING || seatState == SEATED);
    if (canLook && abs(dx) < winW / 4 && abs(dy) < winH / 4) {
        yaw   += dx * MOUSE_SENS;
        pitch -= dy * MOUSE_SENS;
        clampLook();
    }
    glutWarpPointer(winW / 2, winH / 2);
}

void mouseButton(int button, int state, int, int) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    if (!mouseCaptured) captureMouse(true);
    else interact();
}

void keyDown(unsigned char key, int, int) {
    key = tolower(key);
    keys[key] = true;
    switch (key) {
        case 'f':
        case 13:  interact(); break;
        case ' ': if (seatState == SEATED) standUp(); break;
        case 'm': captureMouse(!mouseCaptured); break;
        case 'r': resetMission(); break;
        case 'h': showHelp = !showHelp; break;
        case 27:  exit(0);
    }
}

void keyUp(unsigned char key, int, int) {
    keys[(unsigned char)tolower(key)] = false;
}

void setArrow(int key, bool down) {
    switch (key) {
        case GLUT_KEY_LEFT:  arrows[0] = down; break;
        case GLUT_KEY_RIGHT: arrows[1] = down; break;
        case GLUT_KEY_UP:    arrows[2] = down; break;
        case GLUT_KEY_DOWN:  arrows[3] = down; break;
    }
}

void specialDown(int key, int, int) { setArrow(key, true); }
void specialUp(int key, int, int)   { setArrow(key, false); }

// Bucle de ~60 FPS; se usa el tiempo real para que la velocidad no dependa de la PC
void onTimer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = std::min(0.05f, (now - lastTime) / 1000.0f);
    lastTime = now;
    update(dt);
    glutPostRedisplay();
    glutTimerFunc(16, onTimer, 0);
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);   // mantiene las normales unitarias al usar glScalef
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 1.5f);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.15f);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);

    GLfloat fogColor[] = { 0.20f, 0.06f, 0.25f, 1.0f };
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, 30.0f);
    glFogf(GL_FOG_END, 140.0f);

    initNeonShader();
    buildCity();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Gaming Room 3D - Dev C++ OpenGL");

    std::string self(argv[0]);
    size_t cut = self.find_last_of("\\/");
    exeDir = (cut == std::string::npos) ? "" : self.substr(0, cut + 1);

    init();
    loadTrophy();
    initAudio();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIgnoreKeyRepeat(1);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseButton);
    glutPassiveMotionFunc(mouseMove);
    glutMotionFunc(mouseMove);

    captureMouse(true);
    lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, onTimer, 0);

    glutMainLoop();
    return 0;
}
