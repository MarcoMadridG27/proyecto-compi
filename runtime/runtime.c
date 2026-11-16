/* Runtime del mini-compilador C++: string dinamico, salida y entrada.
 * No usa libstdc++. Se enlaza junto al ensamblador generado:
 *   gcc programa.s runtime/runtime.c -o programa
 *
 * Representacion de string (16 bytes, alineacion 8), igual que el layout del compilador:
 *   offset 0: char* data   (NULL mientras no se haya usado; si no, siempre termina en '\0')
 *   offset 8: int size
 *   offset 12: int capacity (incluye el '\0')
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    int size;
    int capacity;
} String;

static void str_reserve(String *s, int needed) {
    /* needed = bytes necesarios sin contar el '\0' */
    if (s->data && s->capacity >= needed + 1) return;
    int cap = s->capacity > 0 ? s->capacity : 8;
    while (cap < needed + 1) cap *= 2;
    char *p = (char *)realloc(s->data, (size_t)cap);
    if (!p) {
        fputs("runtime: sin memoria\n", stderr);
        exit(1);
    }
    if (!s->data) p[0] = '\0';
    s->data = p;
    s->capacity = cap;
}

void rt_str_reset(String *s) {
    free(s->data);
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

void rt_str_init(String *s) {
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

void rt_str_init_cstr(String *s, const char *text);
void rt_str_copy_init(String *dst, const String *src);

void rt_str_free(String *s) {
    rt_str_reset(s);
}

void rt_str_clear(String *s) {
    s->size = 0;
    if (s->data) s->data[0] = '\0';
}

void rt_str_append_buf(String *s, const char *text, int len) {
    str_reserve(s, s->size + len);
    memcpy(s->data + s->size, text, (size_t)len);
    s->size += len;
    s->data[s->size] = '\0';
}

void rt_str_append_cstr(String *s, const char *text) {
    rt_str_append_buf(s, text, (int)strlen(text));
}

void rt_str_append_char(String *s, char c) {
    rt_str_append_buf(s, &c, 1);
}

void rt_str_append_str(String *s, const String *src) {
    int len = src->size;
    if (len == 0) return;
    str_reserve(s, s->size + len);
    memmove(s->data + s->size, src->data, (size_t)len);
    s->size += len;
    s->data[s->size] = '\0';
}

void rt_str_assign_cstr(String *dst, const char *text) {
    /* text puede apuntar dentro de dst->data (por ejemplo s = s.c_str()) */
    int len = (int)strlen(text);
    str_reserve(dst, len);
    memmove(dst->data, text, (size_t)len);
    dst->size = len;
    dst->data[len] = '\0';
}

void rt_str_assign(String *dst, const String *src) {
    if (dst == src) return;
    if (src->size == 0) {
        rt_str_clear(dst);
        return;
    }
    rt_str_assign_cstr(dst, src->data);
}

void rt_str_init_cstr(String *s, const char *text) {
    rt_str_init(s);
    rt_str_assign_cstr(s, text);
}

void rt_str_copy_init(String *dst, const String *src) {
    rt_str_init(dst);
    rt_str_assign(dst, src);
}

int rt_str_size(const String *s) {
    return s->size;
}

int rt_str_empty(const String *s) {
    return s->size == 0;
}

const char *rt_str_c_str(String *s) {
    str_reserve(s, s->size);
    return s->data;
}

int rt_str_eq(const String *a, const String *b) {
    if (a->size != b->size) return 0;
    if (a->size == 0) return 1;
    return memcmp(a->data, b->data, (size_t)a->size) == 0;
}

int rt_str_eq_cstr(const String *a, const char *b) {
    size_t n = strlen(b);
    if ((size_t)a->size != n) return 0;
    if (n == 0) return 1;
    return memcmp(a->data, b, n) == 0;
}

void rt_print_int(int x) { printf("%d", x); }
void rt_print_char(char c) { putchar(c); }
void rt_print_bool(int b) { putchar(b ? '1' : '0'); }
void rt_print_double(double x) { printf("%g", x); }
void rt_print_cstr(const char *s) { fputs(s, stdout); }

void rt_print_str(const String *s) {
    if (s->size > 0) fwrite(s->data, 1, (size_t)s->size, stdout);
}

void rt_print_endl(void) {
    putchar('\n');
    fflush(stdout);
}

void rt_read_int(int *p) {
    if (scanf(" %d", p) != 1) *p = 0;
}

void rt_read_char(char *p) {
    if (scanf(" %c", p) != 1) *p = '\0';
}

void rt_read_float(float *p) {
    if (scanf(" %f", p) != 1) *p = 0.0f;
}

void rt_read_double(double *p) {
    if (scanf(" %lf", p) != 1) *p = 0.0;
}

void rt_read_string(String *s) {
    rt_str_clear(s);
    int c;
    do {
        c = getchar();
    } while (c != EOF && isspace(c));
    while (c != EOF && !isspace(c)) {
        rt_str_append_char(s, (char)c);
        c = getchar();
    }
    if (c != EOF) ungetc(c, stdin);
}

/* ------------------------------------------------------------------------------------
 * Soporte de seguridad (solo se usa cuando el programa se compila con --sec=...)
 * ------------------------------------------------------------------------------------ */
#include <stdint.h>
#include <unistd.h>

/* Canario de pila: valor aleatorio por ejecucion; el byte bajo es 0 para cortar cadenas */
unsigned long rt_stack_guard = 0x2f5a91c37d00UL;
/* Profundidad actual de recursion */
int rt_depth = 0;

__attribute__((constructor)) static void rt_init_guard(void) {
    unsigned long v = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        if (fread(&v, sizeof v, 1, f) != 1) v = 0;
        fclose(f);
    }
    if (v) rt_stack_guard = v & ~0xffUL;
}

static const char *rt_kind_name(int kind) {
    switch (kind) {
        case 1: return "acceso fuera de limites en arreglo";
        case 2: return "desreferencia de puntero nulo";
        case 3: return "division entre cero";
        case 4: return "desbordamiento aritmetico de entero";
        case 5: return "desbordamiento de pila detectado por el canario";
        case 6: return "limite de profundidad de recursion excedido";
        case 7: return "doble liberacion (double free)";
        case 8: return "uso despues de liberar (use-after-free)";
        case 9: return "desbordamiento de heap detectado al liberar";
        case 10: return "liberacion de un puntero que no proviene de new";
        default: return "violacion de seguridad";
    }
}

/* Termina el programa de forma controlada indicando el tipo de error y su ubicacion */
void rt_fail(int kind, int line, int col) {
    fflush(stdout);
    if (kind == 5) {
        fprintf(stderr, "Error de seguridad: %s (funcion declarada en linea %d, columna %d)\n", rt_kind_name(kind), line,
                col);
    } else {
        fprintf(stderr, "Error de seguridad: %s en linea %d, columna %d\n", rt_kind_name(kind), line, col);
    }
    fflush(stderr);
    _exit(70);
}

/* Asignador instrumentado: cabecera con tamano y marca, zona roja al final, cuarentena
 * de bloques liberados y relleno con 0xDD para que un uso posterior se note. */
#define RT_HDR 16
#define RT_RZ 16
#define RT_MAGIC_LIVE 0x4c49564541525445UL
#define RT_MAGIC_FREE 0x4652454544424c4bUL
#define RT_QUAR 64

typedef struct {
    size_t size;
    unsigned long magic;
} RtHdr;

static char *rt_heap_lo = NULL;
static char *rt_heap_hi = NULL;
static char *rt_quar_ptr[RT_QUAR];
static size_t rt_quar_size[RT_QUAR];
static int rt_quar_n = 0;
static int rt_quar_next = 0;
static char *rt_quar_lo = NULL;   /* rango de direcciones de la cuarentena: filtro rapido */
static char *rt_quar_hi = NULL;

void *rt_malloc(size_t n) {
    char *base = (char *)malloc(RT_HDR + n + RT_RZ);
    if (!base) {
        fputs("runtime: sin memoria\n", stderr);
        exit(1);
    }
    RtHdr *h = (RtHdr *)base;
    h->size = n;
    h->magic = RT_MAGIC_LIVE;
    memset(base + RT_HDR + n, 0xFD, RT_RZ);
    if (!rt_heap_lo || base < rt_heap_lo) rt_heap_lo = base;
    if (!rt_heap_hi || base + RT_HDR + n + RT_RZ > rt_heap_hi) rt_heap_hi = base + RT_HDR + n + RT_RZ;
    return base + RT_HDR;
}

void rt_free(void *p, int line, int col) {
    if (!p) return;
    char *u = (char *)p;
    if (!rt_heap_lo || u - RT_HDR < rt_heap_lo || u >= rt_heap_hi) rt_fail(10, line, col);
    RtHdr *h = (RtHdr *)(u - RT_HDR);
    if (h->magic == RT_MAGIC_FREE) rt_fail(7, line, col);
    if (h->magic != RT_MAGIC_LIVE) rt_fail(10, line, col);
    const unsigned char *rz = (const unsigned char *)u + h->size;
    for (int i = 0; i < RT_RZ; i++) {
        if (rz[i] != 0xFD) rt_fail(9, line, col);
    }
    h->magic = RT_MAGIC_FREE;
    memset(u, 0xDD, h->size);
    /* cuarentena circular: al llenarse se devuelve a libc el bloque mas antiguo */
    if (rt_quar_n == RT_QUAR) {
        free(rt_quar_ptr[rt_quar_next] - RT_HDR);
    } else {
        rt_quar_n++;
    }
    rt_quar_ptr[rt_quar_next] = u;
    rt_quar_size[rt_quar_next] = h->size;
    if (!rt_quar_lo || u < rt_quar_lo) rt_quar_lo = u;
    if (!rt_quar_hi || u + h->size + 1 > rt_quar_hi) rt_quar_hi = u + h->size + 1;
    rt_quar_next = (rt_quar_next + 1) % RT_QUAR;
}

/* Comprueba una direccion que se va a usar: nula o dentro de un bloque ya liberado */
void *rt_check_ptr(void *p, int line, int col) {
    char *a = (char *)p;
    if (!a) rt_fail(2, line, col);
    if (rt_quar_n > 0 && a >= rt_quar_lo && a < rt_quar_hi) {
        for (int i = 0; i < rt_quar_n; i++) {
            if (a >= rt_quar_ptr[i] && a < rt_quar_ptr[i] + (rt_quar_size[i] ? rt_quar_size[i] : 1)) rt_fail(8, line, col);
        }
    }
    return p;
}
