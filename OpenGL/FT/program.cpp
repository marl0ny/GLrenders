#include "gl_wrappers.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <ctime>
#include "fft.hpp"
#include "fft_gl.hpp"
#ifndef __clang__
#include <omp.h>
#endif

struct FloatRGBA {
    union {
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        struct {
            float re;
            float im;
            float _pad[2];
        };
    };
    inline double real() {
        return re;
    }
    inline double imag() {
        return im;
    }
    inline void real(double re) {
        this->re = re;
    }
    inline void imag(double im) {
        this->im = im;
    }
};


void call_inplace_fft(FloatRGBA *arr, int w, int i) {
    inplace_fft<FloatRGBA>((FloatRGBA *)arr + i*w, w);
}

template <typename T>
void button_update(GLFWwindow *window, int key,
                   T &param, T new_val) {
    if (glfwGetKey(window, key) == GLFW_PRESS) param = new_val;
}

template <typename T>
void button_released_update(GLFWwindow *window, int key,
                   T &param, T new_val) {
    if (glfwGetKey(window, key) == GLFW_RELEASE) param = new_val;
}


int main() {
    int w = 512, h = 512;
    uint8_t *image = new uint8_t[h*w*4];
    bool key_pressed = false;
    int view_mode = 0;
    double x0 = 0.5, y0 = 0.5;
    double k = 50, j = 50;
    double sigma = 0.025;
    for (int i = 0; i < w*h; ++i) {
        image[i*4] = (uint8_t)i;
        image[i*4 + 1] = (uint8_t)i;
        image[i*4 + 2] = (uint8_t)i;
        image[i*4 + 3] = (uint8_t)1;
    }
    for (int i = 0; i < h; i++) {
        bitreverse2<int>((int *)&image[w*i*4], w);
    }
    GLFWwindow *window = init_window(3*w, h);
    glViewport(0, 0, w, h);
    init_glew();
    GLuint vert_shader = get_shader("./shaders/vertices.vert", GL_VERTEX_SHADER);
    // GLuint r2_vert_shader = get_shader("./shaders/reverse2-vertices.vert", GL_VERTEX_SHADER);
    GLuint make_tex_shader = get_shader("./shaders/make-tex.frag", GL_FRAGMENT_SHADER);
    GLuint view_shader = get_shader("./shaders/view.frag", GL_FRAGMENT_SHADER);
    GLuint sort_shader = get_shader("./shaders/rearrange.frag", GL_FRAGMENT_SHADER);
    GLuint hpas_shader = get_shader("./shaders/fft-iter.frag", GL_FRAGMENT_SHADER);
    GLuint fftshift_shader = get_shader("./shaders/fftshift.frag", GL_FRAGMENT_SHADER);
    GLuint quad_vert_shader = get_shader("./shaders/quad.vert", GL_VERTEX_SHADER);
    GLuint quad_frag_shader = get_shader("./shaders/quad.frag", GL_FRAGMENT_SHADER);
    GLuint copy2program = make_program(quad_vert_shader, quad_frag_shader);
    GLuint make_tex_program = make_program(vert_shader, make_tex_shader);
    GLuint view_program = make_program(vert_shader, view_shader);
    GLuint sort_program = make_program(vert_shader, sort_shader);
    GLuint hpas_program = make_program(vert_shader, hpas_shader);
    GLuint fftshift_program = make_program(vert_shader, fftshift_shader);


    auto copy_tex = [&](Quad *dest, Quad *src) {
        dest->set_program(copy2program);
        dest->bind();
        dest->set_int_uniform("tex", src->get_value());
        dest->draw();
        unbind();
    };

    auto rev_bit_sort = [&](Quad *dest, Quad *src, int lookupTex) {
        dest->set_program(sort_program);
        dest->bind();
        dest->set_float_uniforms({{"width", w}, {"height", h}});
        dest->set_int_uniforms({
            {"tex", src->get_value()},
            {"lookupTex", lookupTex},
        });
        dest->draw();
        unbind();
    };

    Quad q0 = Quad::make_frame(3.0*w, h);

    GLint tex = make_texture(image, w, h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    Quad q1 = Quad::make_float_frame(w, h);
    Quad q2 = Quad::make_float_frame(w, h);
    Quad q3 = Quad::make_float_frame(w, h);
    Quad q4 = Quad::make_float_frame(w, h);
    Quad q5 = Quad::make_float_frame(w, h);
    Quad q6 = Quad::make_float_frame(w, h);
    int premade_tex = Quad::get_blank();

    FloatRGBA *arr = new FloatRGBA[w*h];

    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            arr[j*w + i].r = (float)i/(float)w;
            arr[j*w + i].g = (float)j/(float)h;
            arr[j*w + i].b = 0.0;
            arr[j*w + i].a = 1.0;
        } 
    }
    /*for (int i = 0; i < w; i++) {
        for (int j = 0; j <= w/2 - 1; j++) {
            arr[i*w + j] = arr[i*w + j*2];
            arr[i*w + w/2 + j] = arr[i*w + j*2 + 1];
        }
    }*/
    square_bitreverse2<FloatRGBA>(arr, w);
    q5.substitute_array(w, h, GL_FLOAT, arr);
    unbind();


    auto gpu_fft = [&] () {
        q2.set_program(make_tex_program);
        q2.bind();
        q2.set_int_uniform("type", view_mode);
        q2.set_float_uniforms({
                {"k", k}, {"j", j},
                {"sigma", sigma}, {"a", 5.0},
                {"x0", x0}, {"y0", y0}, {"r", 1.0}
        });
        q2.set_int_uniform("tex", premade_tex);
        q2.draw();
        clock_t t1 = clock();
        rev_bit_sort(&q3, &q2, q5.get_value());
        Quad *qs[2] = {&q3, &q1};
        horizontal_fft(hpas_program, qs, w);
        Quad *q = vertical_fft(hpas_program, qs, w);
        if (q == &q1) copy_tex(&q3, &q1);
        vertical_fft_shift(&q1, &q3, fftshift_program);
        horizontal_fft_shift(&q3, &q1, fftshift_program);
        copy_tex(&q4, &q3);
        vertical_fft_shift(&q1, &q4, fftshift_program);
        horizontal_fft_shift(&q4, &q1, fftshift_program);
        rev_bit_sort(&q1, &q4, q5.get_value());
        copy_tex(&q4, &q1);
        qs[0] = &q4, qs[1] = &q1;
        vertical_ifft(hpas_program, qs, w);
        q = horizontal_ifft(hpas_program, qs, w);
        if (q == &q1) copy_tex(&q4, &q1);
        clock_t t2 = clock();
        double time_taken = (double)(t2 - t1)/(double)CLOCKS_PER_SEC;
        std::cout << "Time taken (gpu): " << time_taken << " s\n";
    };

    auto cpu_fft = [&] () {
        q2.set_program(make_tex_program);
        q2.bind();
        q2.set_int_uniform("type", view_mode);
        q2.set_float_uniforms({
                {"k", k}, {"j", j},
                {"sigma", sigma}, {"a", 5.0},
                {"x0", x0}, {"y0", y0}, {"r", 1.0}
        });
        q2.set_int_uniform("tex", Quad::get_blank());
        q2.draw();
        #ifndef __clang__
        double t1 = omp_get_wtime();
        #else
        clock_t t1 = clock();
        #endif
        q2.get_texture_array(arr, 0, 0, w, h, GL_FLOAT);
        inplace_fft2<FloatRGBA>(arr, w);
        inplace_fftshift2(arr, w);
        q3.substitute_array(w, h, GL_FLOAT, arr);
        inplace_fftshift2(arr, w);
        inplace_ifft2<FloatRGBA>(arr, w);
        q4.substitute_array(w, h, GL_FLOAT, arr);
        #ifndef __clang__
        double t2 = omp_get_wtime();
        double time_taken = t2 - t1;
        std::cout << "Time taken (cpu): " << time_taken << " s\n";
        #else
        clock_t t2 = clock();
        double time_taken = (double)(t2 - t1)/(double)CLOCKS_PER_SEC;
        std::cout << "Time taken (cpu): " << time_taken << " s\n";
        #endif
        unbind();
    };

    cpu_fft();

    Quad *quads[3] = {&q2, &q3, &q4};
    double brightness = 1.0;
    bool use_cpu = false;

    for (int i = 0; !glfwWindowShouldClose(window); i++) {
        if (key_pressed) {
            if (use_cpu) {
                cpu_fft();
            } else {
                gpu_fft();
            }
            key_pressed = false;
        }
        glViewport(0, 0, 3*w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        q0.set_program(view_program);
        q0.bind();
        q0.set_int_uniforms({
            {"tex", quads[0]->get_value()},
            {"tex2", quads[1]->get_value()},
            {"tex3", quads[2]->get_value()},
            // {"tex", q3.get_value()}
        });
        q0.set_float_uniform("gridSize", w);
        q0.set_float_uniform("brightness", brightness);
        q0.draw();
        unbind();
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            brightness *= 1.1;
        } else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            brightness *= 0.9;
        }
        glfwSwapBuffers(window);
        button_update(window, GLFW_KEY_TAB, use_cpu, !use_cpu);
        button_update(window, GLFW_KEY_Q, sigma, sigma - 0.0025);
        button_update(window, GLFW_KEY_E, sigma, sigma + 0.0025);
        button_update(window, GLFW_KEY_W, y0, y0+0.01);
        button_update(window, GLFW_KEY_A, x0, x0-0.01);
        button_update(window, GLFW_KEY_S, y0, y0-0.01);
        button_update(window, GLFW_KEY_D, x0, x0+0.01);
        button_update(window, GLFW_KEY_R, k, k + 1.0);
        button_update(window, GLFW_KEY_F, k, k - 1.0);
        button_update(window, GLFW_KEY_T, j, j + 1.0);
        button_update(window, GLFW_KEY_G, j, j - 1.0);
        for (int i = 0; i < 8; i++) {
            button_update(window, GLFW_KEY_0+i, view_mode, i);
        }
        for (int i = 0; i < 8; i++) {
            button_update(window, GLFW_KEY_0+i, key_pressed, true);
        }
        button_update(window, GLFW_KEY_7, premade_tex, q5.get_value());
        button_update(window, GLFW_KEY_Q, key_pressed, true);
        button_update(window, GLFW_KEY_E, key_pressed, true);
        button_update(window, GLFW_KEY_W, key_pressed, true);
        button_update(window, GLFW_KEY_A, key_pressed, true);
        button_update(window, GLFW_KEY_S, key_pressed, true);
        button_update(window, GLFW_KEY_D, key_pressed, true);
        button_update(window, GLFW_KEY_R, key_pressed, true);
        button_update(window, GLFW_KEY_F, key_pressed, true);
        button_update(window, GLFW_KEY_T, key_pressed, true);
        button_update(window, GLFW_KEY_G, key_pressed, true);
        glViewport(0, 0, w, h);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}