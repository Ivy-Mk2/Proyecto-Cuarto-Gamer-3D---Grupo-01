#include <GL/glut.h>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Posición de la cámara
float camX = 4.0f, camY = 2.5f, camZ = 6.0f;

// Ángulos de la cámara
float yaw = -90.0f;
float pitch = -15.0f;

// Velocidades
float moveSpeed = 0.15f;
float rotSpeed = 2.5f;

void getLookAt(float &lookX, float &lookY, float &lookZ) {
    float yawRad = yaw * M_PI / 180.0f;
    float pitchRad = pitch * M_PI / 180.0f;

    lookX = camX + cos(pitchRad) * cos(yawRad);
    lookY = camY + sin(pitchRad);
    lookZ = camZ + cos(pitchRad) * sin(yawRad);
}

void drawCube(float size) {
    glutSolidCube(size);
}

// ===================== LÍNEAS DE NEÓN EN ESQUINAS (CORREGIDO) =====================
void drawNeonEdges() {
    glDisable(GL_LIGHTING);

    // Color del neón
    glColor3f(0.15f, 0.95f, 1.0f); // Cyan más brillante

    float thickness = 0.08f; // ? más grueso

    // ===== TECHO (unión pared-techo) =====
    // Pared trasera
    glPushMatrix();
        glTranslatef(0.0f, 3.96f, -3.92f);
        glScalef(9.9f, thickness, thickness);
        drawCube(1.0f);
    glPopMatrix();

    // Pared izquierda
    glPushMatrix();
        glTranslatef(-4.92f, 3.96f, 0.0f);
        glScalef(thickness, thickness, 7.9f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared derecha
    glPushMatrix();
        glTranslatef(4.92f, 3.96f, 0.0f);
        glScalef(thickness, thickness, 7.9f);
        drawCube(1.0f);
    glPopMatrix();

    // ===== SUELO (unión pared-suelo) =====
    // Pared trasera
    glPushMatrix();
        glTranslatef(0.0f, 0.04f, -3.92f);
        glScalef(9.9f, thickness, thickness);
        drawCube(1.0f);
    glPopMatrix();

    // Pared izquierda
    glPushMatrix();
        glTranslatef(-4.92f, 0.04f, 0.0f);
        glScalef(thickness, thickness, 7.9f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared derecha
    glPushMatrix();
        glTranslatef(4.92f, 0.04f, 0.0f);
        glScalef(thickness, thickness, 7.9f);
        drawCube(1.0f);
    glPopMatrix();

    // ===== ESQUINAS VERTICALES =====
    // Trasera izquierda
    glPushMatrix();
        glTranslatef(-4.92f, 2.0f, -3.92f);
        glScalef(thickness, 3.95f, thickness);
        drawCube(1.0f);
    glPopMatrix();

    // Trasera derecha
    glPushMatrix();
        glTranslatef(4.92f, 2.0f, -3.92f);
        glScalef(thickness, 3.95f, thickness);
        drawCube(1.0f);
    glPopMatrix();

    // Delantera izquierda (opcional)
    glPushMatrix();
        glTranslatef(-4.92f, 2.0f, 3.92f);
        glScalef(thickness, 3.95f, thickness);
        drawCube(1.0f);
    glPopMatrix();

    // Delantera derecha (opcional)
    glPushMatrix();
        glTranslatef(4.92f, 2.0f, 3.92f);
        glScalef(thickness, 3.95f, thickness);
        drawCube(1.0f);
    glPopMatrix();

    glEnable(GL_LIGHTING);
}

// Habitación
void drawRoom() {
    glDisable(GL_LIGHTING);

    // Suelo
    glColor3f(0.25f, 0.18f, 0.12f);
    glPushMatrix();
        glTranslatef(0.0f, -0.05f, 0.0f);
        glScalef(10.0f, 0.1f, 8.0f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared trasera
    glColor3f(0.35f, 0.28f, 0.22f);
    glPushMatrix();
        glTranslatef(0.0f, 2.0f, -4.0f);
        glScalef(10.0f, 4.0f, 0.15f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared izquierda
    glColor3f(0.32f, 0.26f, 0.20f);
    glPushMatrix();
        glTranslatef(-5.0f, 2.0f, 0.0f);
        glScalef(0.15f, 4.0f, 8.0f);
        drawCube(1.0f);
    glPopMatrix();

    // Pared derecha
    glColor3f(0.32f, 0.26f, 0.20f);
    glPushMatrix();
        glTranslatef(5.0f, 2.0f, 0.0f);
        glScalef(0.15f, 4.0f, 8.0f);
        drawCube(1.0f);
    glPopMatrix();

    // Techo
    glColor3f(0.12f, 0.12f, 0.15f);
    glPushMatrix();
        glTranslatef(0.0f, 4.0f, 0.0f);
        glScalef(10.0f, 0.1f, 8.0f);
        drawCube(1.0f);
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

// Monitor
void drawMonitor() {
    glColor3f(0.15f, 0.15f, 0.15f);
    glPushMatrix();
        glTranslatef(0.0f, 0.85f, 0.3f);
        glScalef(0.6f, 0.05f, 0.25f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.10f, 0.10f, 0.12f);
    glPushMatrix();
        glTranslatef(0.0f, 1.25f, 0.15f);
        glScalef(1.4f, 0.8f, 0.08f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.20f, 0.40f, 0.70f);
    glPushMatrix();
        glTranslatef(0.0f, 1.25f, 0.11f);
        glScalef(1.3f, 0.7f, 0.02f);
        drawCube(1.0f);
    glPopMatrix();
}

// PC con luces
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

    glDisable(GL_LIGHTING);

    // Luces del PC
    glColor3f(0.2f, 0.9f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.35f, 1.52f, 0.64f);
        glScalef(0.35f, 0.04f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.9f, 0.3f, 0.9f);
    glPushMatrix();
        glTranslatef(-1.35f, 1.25f, 0.64f);
        glScalef(0.30f, 0.04f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.3f, 0.5f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.35f, 0.98f, 0.64f);
        glScalef(0.28f, 0.04f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.4f, 0.8f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.55f, 1.15f, 0.40f);
        glScalef(0.03f, 0.6f, 0.03f);
        drawCube(1.0f);
    glPopMatrix();

    glEnable(GL_LIGHTING);
}

// Letrero neón
void drawNeonSign() {
    glDisable(GL_LIGHTING);

    glColor3f(0.05f, 0.05f, 0.08f);
    glPushMatrix();
        glTranslatef(0.0f, 3.15f, -3.88f);
        glScalef(2.6f, 0.55f, 0.06f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.1f, 0.85f, 1.0f);
    glPushMatrix();
        glTranslatef(0.0f, 3.15f, -3.85f);
        glScalef(2.7f, 0.65f, 0.04f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.3f, 0.95f, 1.0f);
    glPushMatrix();
        glTranslatef(-1.15f, 3.05f, -3.82f);
        glScalef(0.0028f, 0.0028f, 0.0028f);
        glLineWidth(3.0f);

        const char* text = "GAMING ZONE";
        for (int i = 0; i < strlen(text); i++) {
            glutStrokeCharacter(GLUT_STROKE_ROMAN, text[i]);
        }
    glPopMatrix();

    glLineWidth(1.0f);
    glEnable(GL_LIGHTING);
}

// Silla
void drawChair() {
    glColor3f(0.15f, 0.15f, 0.22f);
    glPushMatrix();
        glTranslatef(1.8f, 0.55f, 1.8f);
        glScalef(0.7f, 0.12f, 0.7f);
        drawCube(1.0f);
    glPopMatrix();

    glPushMatrix();
        glTranslatef(1.8f, 1.1f, 2.1f);
        glScalef(0.7f, 1.0f, 0.12f);
        drawCube(1.0f);
    glPopMatrix();

    glColor3f(0.10f, 0.10f, 0.12f);
    glPushMatrix();
        glTranslatef(1.8f, 0.25f, 1.8f);
        glScalef(0.5f, 0.1f, 0.5f);
        drawCube(1.0f);
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

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    float lookX, lookY, lookZ;
    getLookAt(lookX, lookY, lookZ);

    gluLookAt(camX, camY, camZ,
              lookX, lookY, lookZ,
              0.0f, 1.0f, 0.0f);

    GLfloat lightPos[] = {2.0f, 3.5f, 3.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    drawRoom();
    drawNeonEdges();    // ? Líneas de neón en esquinas
    drawDesk();
    drawMonitor();
    drawPC();
    drawNeonSign();
    drawChair();

    drawShelf(-3.2f, 2.4f, -3.7f);
    drawShelf(-3.2f, 1.8f, -3.7f);
    drawShelf(3.2f, 2.4f, -3.7f);

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    float ratio = (float)w / (float)h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(50.0f, ratio, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    float yawRad = yaw * M_PI / 180.0f;

    switch (tolower(key)) {
        case 'w':
            camX += cos(yawRad) * moveSpeed;
            camZ += sin(yawRad) * moveSpeed;
            break;
        case 's':
            camX -= cos(yawRad) * moveSpeed;
            camZ -= sin(yawRad) * moveSpeed;
            break;
        case 'a':
            camX += sin(yawRad) * moveSpeed;
            camZ -= cos(yawRad) * moveSpeed;
            break;
        case 'd':
            camX -= sin(yawRad) * moveSpeed;
            camZ += cos(yawRad) * moveSpeed;
            break;
        case 'q':
            camY += moveSpeed;
            break;
        case 'e':
            camY -= moveSpeed;
            break;
        case 27:
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_LEFT:  yaw -= rotSpeed; break;
        case GLUT_KEY_RIGHT: yaw += rotSpeed; break;
        case GLUT_KEY_UP:
            pitch += rotSpeed;
            if (pitch > 89.0f) pitch = 89.0f;
            break;
        case GLUT_KEY_DOWN:
            pitch -= rotSpeed;
            if (pitch < -89.0f) pitch = -89.0f;
            break;
    }
    glutPostRedisplay();
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Gaming Room 3D - Dev C++ OpenGL");

    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);

    glutMainLoop();
    return 0;
}
