#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

// Mac OS X
#ifdef DARWIN
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#endif

// The Rest of the World
#ifdef POSIX
#include <GL/gl.h>
#include <GL/glut.h>
#endif

#include "canvas.h"
#include "render.h"

#include "scene1.h"

#define USE_MT  (1)

GLint win_width = 512;
GLint win_height = 512;

#define TEX_WIDTH  256
#define TEX_HEIGHT 256


typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pixel_t;

float delta_al = M_PI / 6;
Point3d camera_point = { X_CAM, Y_CAM, Z_CAM };
Scene *scene = NULL;

GLuint tex;
pixel_t canvas[TEX_WIDTH][TEX_HEIGHT] = { 0 };

#if USE_MT
#include "mt_render.h"

mt_tasks_t *tasks = NULL;

static int render_task(mt_worker_t *w) {
    pixel_t px;

    GLint height = TEX_HEIGHT / w->state->n_cpu;
    GLint h1 = height * w->num;
    GLint h2 = h1 + height;

    GLint i, j;

    for (j = h1; j < h2; ++j)
        for (i = 0; i < TEX_WIDTH; ++i) {
            Vector3d ray = { i - TEX_WIDTH / 2, j - TEX_HEIGHT / 2, PROJ_PLANE_Z };
            trace(scene, camera_point, ray, (Color *)&px);
            canvas[j][i] = px;
        }

    return 0;
}
#endif

static int render_seq() {
    pixel_t px;
    GLint i, j;

    for (j = 0; j < TEX_HEIGHT; ++j)
        for (i = 0; i < TEX_WIDTH; ++i) {
            Vector3d ray = { i - TEX_WIDTH / 2, j - TEX_HEIGHT / 2, PROJ_PLANE_Z };
            trace(scene, camera_point, ray, (Color *)&px);
            canvas[j][i] = px;
        }
}

void prepare_canvas(void) {
#if USE_MT
    mt_render_start(tasks);
    mt_render_wait(tasks);
#else
    render_seq();
#endif
}


void display(void) {
    static int frames = 0;
    static struct timespec last_time = { 0 };
    struct timespec this_time = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &this_time);

    if (this_time.tv_sec >= last_time.tv_sec + 10) {
        printf("FPS: %f\n", frames / 10.0);

        last_time = this_time;
        frames = 0;
    }
    //printf("%d) time: %d.%09d\n", frames, (int)this_time.tv_sec, (int)this_time.tv_nsec);

    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
     glTexCoord2f(0.0, 0.0);
     glVertex2f( 1.0,  1.0);
     glTexCoord2f(1.0, 0.0);
     glVertex2f(-1.0,  1.0);
     glTexCoord2f(1.0, 1.0);
     glVertex2f(-1.0, -1.0);
     glTexCoord2f(0.0, 1.0);
     glVertex2f( 1.0, -1.0);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    glFlush();
    ++frames;
}

void reshape(GLint w, GLint h) {
    win_width = w;
    win_height = h;
    glViewport(0, 0, w, h);
    glutPostRedisplay();
}

static inline uint8_t toGLubyte(GLfloat clampf) {
    return (uint8_t)(256 * clampf - 1);
}


void animate() {

    delta_al += 0.05;
    rotate_scene(scene, delta_al, M_PI * 3 / 5, ROTATE_LIGHT_SOURCES);
    prepare_canvas();

    glEnable(GL_TEXTURE_2D);
    /// TODO: Change to glSubImage2D
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, canvas);
    glDisable(GL_TEXTURE_2D);

    glutPostRedisplay();
}

int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitWindowSize(win_width, win_height);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);

    glutCreateWindow("Raytracing");
    
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glViewport(0, 0, win_width, win_height);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glutDisplayFunc(display);
    glutIdleFunc(animate);
   
    scene = makeScene();

#if USE_MT
    tasks = mt_new_pool(render_task);
#endif

    rotate_scene(scene, delta_al, M_PI * 3 / 5, ROTATE_LIGHT_SOURCES);
    prepare_canvas();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, canvas);

    glutMainLoop();
}