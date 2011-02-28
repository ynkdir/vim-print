
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <cairo.h>
#include <cairo-ps.h>
#include <cairo-pdf.h>
#include <cairo-ft.h>


/* FIXME: don't use constant */
#define LINENR_MARGIN 10

struct Options {
    double paper_width;
    double paper_height;
    double margin_left;
    double margin_top;
    double margin_right;
    double margin_bottom;
    char *header_format;
    int header_extraline;
    int number_width;
    double linespace;
    char *font_name;
    double font_size;
};


struct Color {
    double r;
    double g;
    double b;
};


struct Highlight {
    char *name;
    struct Color fg;
    struct Color bg;
    struct Color sp;
    int bold;
    int italic;
    int underline;
    int undercurl;
};


struct PrintContext {
    int pagenum;
    int linenum;
    double font_height;
    double font_descent;
    double numberwidth;
    double y;
    double x;
    struct Highlight hi;
};


static int endswith(const char *haystack, const char *needle);
static int utf8len(const char *str);
static void error(const char *message, ...);
static void skip_space();
static int read_char();
static char *read_command();
static char *read_string();
static int read_integer();
static double read_float();
static struct Color read_color();
static void command_paper();
static void command_margin();
static void command_header();
static void command_number();
static void command_linespace();
static void command_font();
static void command_highlight();
static void command_text();
static void command_line();
static void command_start();
static void command_end();
static int is_white(struct Color color);
static void set_font(const char *name, double size, int bold, int italic);
static void newline();
static void newpage();
static void print_number();
static void print_header();
static void print_text(const char *text, struct Highlight hi);
static void print();


static char *infile;
static char *outfile;
static FILE *in;
static struct Options options;
static struct PrintContext pc;
static cairo_surface_t *surface;
static cairo_t *cr;


static int
endswith(const char *haystack, const char *needle)
{
    const char *a;
    const char *b;

    a = haystack + strlen(haystack) - 1;
    b = needle + strlen(needle) - 1;
    while (haystack <= a && needle <= b && *a == *b) {
        a--;
        b--;
    }
    return (b < needle);
}


static int
utf8len(const char *str)
{
    unsigned char c;

    c = (unsigned char)str[0];
    if ((c & 0x80) == 0) {
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        return 4;
    } else if ((c & 0xFC) == 0xF8) {
        return 5;
    } else if ((c & 0xFE) == 0xFC) {
        return 6;
    }

    error("invalid utf8");
}


static void
error(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    exit(EXIT_FAILURE);
}


static void
skip_space()
{
    fscanf(in, "%*[ \t\r\n]");
}


static int
read_char()
{
    int c;

    c = fgetc(in);
    if (c == EOF) {
        error("unexpected EOF");
    }

    return c;
}


static char *
read_command()
{
    char buf[256];
    int n;

    n = fscanf(in, "%s", (char *)&buf);
    if (n != 1) {
        error("read_command error");
    }

    return strdup(buf);
}


static char *
read_string()
{
    int c;
    char *buf = NULL;
    size_t bufsize = 0;

    skip_space();
    c = read_char();
    if (c != '"') {
        error("unexpected character: %d", c);
    }

    for (;;) {
        bufsize += 1;
        buf = realloc(buf, bufsize);
        c = read_char();
        if (c == '"') {
            buf[bufsize - 1] = '\0';
            break;
        }
        if (c == '\\') {
            c = read_char();
        }
        buf[bufsize - 1] = c;
    }

    return buf;
}


static int
read_integer()
{
    int n;
    int x;

    n = fscanf(in, "%d", &x);
    if (n != 1) {
        error("read_integer error");
    }

    return x;
}


static double
read_float()
{
    int n;
    double x;

    n = fscanf(in, "%lf", &x);
    if (n != 1) {
        error("read_float error");
    }

    return x;
}


static struct Color
read_color()
{
    int n;
    int r, g, b;
    struct Color color;

    skip_space();
    n = fscanf(in, "#%2x%2x%2x", &r, &g, &b);
    if (n != 3) {
        error("read_color error");
    }

    color.r = r / 255.0;
    color.g = g / 255.0;
    color.b = b / 255.0;

    return color;
}


static void
command_paper()
{
    options.paper_width = read_float();
    options.paper_height = read_float();
}


static void
command_margin()
{
    options.margin_left = read_float();
    options.margin_top = read_float();
    options.margin_right = read_float();
    options.margin_bottom = read_float();
}


static void
command_header()
{
    options.header_format = read_string();
    options.header_extraline = read_integer();
}


static void
command_number()
{
    options.number_width = read_integer();
}


static void
command_linespace()
{
    options.linespace = read_float();
}


static void
command_font()
{
    options.font_name = read_string();
    options.font_size = read_float();
}

static void
command_highlight()
{
    struct Highlight hi;

    hi.name = read_string();
    hi.fg = read_color();
    hi.bg = read_color();
    hi.sp = read_color();
    hi.bold = read_integer();
    hi.italic = read_integer();
    hi.underline = read_integer();
    hi.undercurl = read_integer();

    if (pc.hi.name != NULL) {
        free(pc.hi.name);
    }
    pc.hi = hi;
}


static void
command_text()
{
    char *text;

    text = read_string();
    print_text(text, pc.hi);
    free(text);
}


static void
command_line()
{
    newline();
}


static void
command_start()
{
    cairo_font_extents_t fe;
    cairo_text_extents_t te;

    if (endswith(outfile, ".ps")) {
        surface = cairo_ps_surface_create(outfile,
                options.paper_width, options.paper_height);
    } else if (endswith(outfile, ".pdf")) {
        surface = cairo_pdf_surface_create(outfile,
                options.paper_width, options.paper_height);
    } else {
        error("file type is not supported: %s", outfile);
    }

    cr = cairo_create(surface);

    pc.pagenum = 0;
    pc.linenum = 0;

    set_font(options.font_name, options.font_size, 0, 0);

    /* FIXME: How to get line height and baseline offset?
     * Use linespace option for workaround. */
    cairo_font_extents(cr, &fe);
    pc.font_height = fe.height + options.linespace;
    pc.font_descent = fe.descent + options.linespace / 2;

    if (options.number_width > 0) {
        /* FIXME: What is correct way? */
        cairo_text_extents(cr, "0", &te);
        pc.numberwidth = options.number_width * te.x_advance + LINENR_MARGIN;
    } else {
        pc.numberwidth = 0;
    }
}


static void
command_end()
{
    cairo_show_page(cr);

    if (cr != NULL) {
        cairo_destroy(cr);
        cr = NULL;
    }

    if (surface != NULL) {
        cairo_surface_finish(surface);
        cairo_surface_destroy(surface);
        surface = NULL;
    }
}


static int
is_white(struct Color color)
{
    return (color.r == 1 && color.g == 1 && color.b == 1);
}


static void
set_font(const char *name, double size, int bold, int italic)
{
    cairo_font_slant_t slant;
    cairo_font_weight_t weight;

    if (italic) {
        slant = CAIRO_FONT_SLANT_ITALIC;
    } else {
        slant = CAIRO_FONT_SLANT_NORMAL;
    }

    if (bold) {
        weight = CAIRO_FONT_WEIGHT_BOLD;
    } else {
        weight = CAIRO_FONT_WEIGHT_NORMAL;
    }

    if (endswith(name, ".ttf")) {
#if CAIRO_HAS_FT_FONT
        /* FIXME: bold? italic? */
        static char prev_name[512] = {0};
        static FT_Library library = NULL;
        FT_Face face;
        FT_Error err;
        cairo_font_face_t *f;
        int face_index = 0;
        int load_flags = 0;

        if (strcmp(prev_name, name) != 0) {
            if (library != NULL) {
                err = FT_Init_FreeType(&library);
                if (err) {
                    error("FT_Init_FreeType failed");
                }
            }

            err = FT_New_Face(library, name, face_index, &face);
            if (err) {
                error("FT_New_Face failed");
            }

            f = cairo_ft_font_face_create_for_ft_face(face, load_flags);

            cairo_set_font_face(cr, f);
        }

        cairo_set_font_size(cr, size);
#else
        error("ttf is not supported");
#endif
    } else {
        /* FIXME: How to embed? */
        cairo_select_font_face(cr, name, slant, weight);
        cairo_set_font_size(cr, size);
    }
}


static void
newline()
{
    if (pc.linenum == 0) {
        newpage();
    } else {
        pc.y += pc.font_height;
        if (pc.y + pc.font_height > options.paper_height - options.margin_bottom) {
            newpage();
        }
    }

    pc.linenum += 1;

    print_number();

    pc.x = options.margin_left + pc.numberwidth;
}


static void
newpage()
{
    if (pc.pagenum != 0) {
        cairo_show_page(cr);
    }

    pc.pagenum += 1;

    print_header();

    pc.x = options.margin_left + pc.numberwidth;
    pc.y = options.margin_top + pc.font_height * (1 + options.header_extraline);
}


static void
print_number()
{
    char fmt[256];
    char buf[256];
    cairo_text_extents_t te;
    struct Highlight hi;

    if (options.number_width <= 0) {
        return;
    }

    sprintf(fmt, "%%%dd", options.number_width);
    sprintf(buf, fmt, pc.linenum);

    /* FIXME: load from file */
    hi.name = "LineNr";
    hi.fg.r = 0;
    hi.fg.g = 0;
    hi.fg.b = 0;
    hi.bg.r = 1;
    hi.bg.g = 1;
    hi.bg.b = 1;
    hi.sp = hi.fg;
    hi.bold = 0;
    hi.italic = 0;
    hi.underline = 0;
    hi.undercurl = 0;

    cairo_text_extents(cr, buf, &te);
    pc.x = options.margin_left + pc.numberwidth - LINENR_MARGIN - te.x_advance;
    print_text(buf, hi);
}


static void
print_header()
{
    char left[1024];
    char right[1024];
    char *out;
    char *p;
    cairo_text_extents_t te;
    struct Highlight hi;

    if (options.header_format == NULL || options.header_format[0] == '\0') {
        return;
    }

    left[0] = '\0';
    right[0] = '\0';

    out = left;

    for (p = options.header_format; *p != '\0'; ++p) {
        if (*p == '%') {
            ++p;
            if (*p == '%') {
                *out++ = '%';
            } else if (*p == 'N') {
                out += sprintf(out, "%d", pc.pagenum);
            } else if (*p == '=') {
                out = right;
            } else {
                error("unknown header item: %c", *p);
            }
        } else {
            *out++ = *p;
        }
        *out = '\0';
    }

    /* FIXME: load from file */
    hi.name = "PageHeader";
    hi.fg.r = 0;
    hi.fg.g = 0;
    hi.fg.b = 0;
    hi.bg.r = 1;
    hi.bg.g = 1;
    hi.bg.b = 1;
    hi.sp = hi.fg;
    hi.bold = 0;
    hi.italic = 0;
    hi.underline = 0;
    hi.undercurl = 0;

    pc.x = options.margin_left;
    pc.y = options.margin_top;
    print_text(left, hi);

    cairo_text_extents(cr, right, &te);
    pc.x = options.paper_width - options.margin_right - te.x_advance;
    pc.y = options.margin_top;
    print_text(right, hi);
}


static void
print_text(const char *text, struct Highlight hi)
{
    cairo_text_extents_t te;
    const char *p;
    int len;
    char buf[256];

    set_font(options.font_name, options.font_size, hi.bold, hi.italic);

    /* FIXME: Is it better not to write one by one? */
    for (p = text; *p != '\0'; p += len) {
        len = utf8len(p);
        memmove(buf, p, len);
        buf[len] = '\0';

        cairo_text_extents(cr, buf, &te);

        if (pc.x + te.x_advance > options.paper_width - options.margin_right) {
            pc.y += pc.font_height;
            if (pc.y + pc.font_height >
                    options.paper_height - options.margin_bottom) {
                newpage();
            }
            pc.x = options.margin_left + pc.numberwidth;
        }

        if (!is_white(hi.bg)) {
            cairo_set_source_rgb(cr, hi.bg.r, hi.bg.g, hi.bg.b);
            cairo_rectangle(cr, pc.x, pc.y, te.x_advance, pc.font_height);
            cairo_fill(cr);
        }

        cairo_set_source_rgb(cr, hi.fg.r, hi.fg.g, hi.fg.b);
        cairo_move_to(cr, pc.x, pc.y + pc.font_height - pc.font_descent);
        cairo_show_text(cr, buf);

        pc.x += te.x_advance;
    }
}


static void
print()
{
    char *command;

    for (;;) {
        skip_space();
        if (feof(in)) {
            break;
        }
        command = read_command();
        if (strcmp(command, "PAPER") == 0) {
            command_paper();
        } else if (strcmp(command, "MARGIN") == 0) {
            command_margin();
        } else if (strcmp(command, "HEADER") == 0) {
            command_header();
        } else if (strcmp(command, "NUMBER") == 0) {
            command_number();
        } else if (strcmp(command, "LINESPACE") == 0) {
            command_linespace();
        } else if (strcmp(command, "FONT") == 0) {
            command_font();
        } else if (strcmp(command, "HIGHLIGHT") == 0) {
            command_highlight();
        } else if (strcmp(command, "TEXT") == 0) {
            command_text();
        } else if (strcmp(command, "LINE") == 0) {
            command_line();
        } else if (strcmp(command, "START") == 0) {
            command_start();
        } else if (strcmp(command, "END") == 0) {
            command_end();
        } else {
            error("unknown command: %s", command);
        }
        free(command);
    }
}


int
main(int argc, char **argv)
{

    infile = argv[1];
    outfile = argv[2];

    in = fopen(infile, "r");

    print();

    fclose(in);

    return 0;
}

