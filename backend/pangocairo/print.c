
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <cairo.h>
#include <cairo-ps.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>


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
    char *font_name;
    double font_size;
};


struct Color {
    double r;
    double g;
    double b;
};


struct PrintContext {
    int pagenum;
    int linenum;
    double font_height;
    double font_descent;
    double numberwidth;
    double y;
    double x;
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
static void command_font();
static void command_line();
static void command_start();
static void command_end();
static void textsize(const char *text, double *width, double *height, double *baseline);
static void newline();
static void newpage();
static void print_number();
static void print_header();
static void print_text(const char *text);
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
command_font()
{
    options.font_name = read_string();
    options.font_size = read_float();
}

static void
command_line()
{
    char *text;

    text = read_string();
    newline();
    print_text(text);
    free(text);
}


static void
command_start()
{
    double width;
    double height;
    double baseline;

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

    textsize("MW", &width, &height, &baseline);
    width = width / 2;

    pc.font_height = height;
    pc.font_descent = baseline;

    if (options.number_width > 0) {
        pc.numberwidth = options.number_width * width + LINENR_MARGIN;
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


static void
textsize(const char *text, double *width, double *height, double *baseline)
{
    PangoLayout *layout;
    PangoFontDescription *desc;
    int w, h;

    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_new();
    pango_font_description_set_family(desc, options.font_name);
    pango_font_description_set_size(desc, options.font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_markup(layout, text, -1);
    pango_layout_get_size(layout, &w, &h);
    if (width != NULL) {
        *width = (double)w / PANGO_SCALE;
    }
    if (height != NULL) {
        *height = (double)h / PANGO_SCALE;
    }
    if (baseline != NULL) {
        *baseline = (double)pango_layout_get_baseline(layout) / PANGO_SCALE;
    }
    pango_font_description_free(desc);
    g_object_unref(layout);
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
    double width;

    if (options.number_width <= 0) {
        return;
    }

    sprintf(fmt, "%%%dd", options.number_width);
    sprintf(buf, fmt, pc.linenum);

    textsize(buf, &width, NULL, NULL);
    pc.x = options.margin_left + pc.numberwidth - LINENR_MARGIN - width;
    print_text(buf);
}


static void
print_header()
{
    char left[1024];
    char right[1024];
    char *out;
    char *p;
    double width;

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

    pc.x = options.margin_left;
    pc.y = options.margin_top;
    print_text(left);

    textsize(right, &width, NULL, NULL);
    pc.x = options.paper_width - options.margin_right - width;
    pc.y = options.margin_top;
    print_text(right);
}


static void
print_text(const char *text)
{
    PangoLayout *layout;
    PangoFontDescription *desc;
    PangoLayoutLine *line;
    int i;

    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_new();
    pango_font_description_set_family(desc, options.font_name);
    pango_font_description_set_size(desc, options.font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_markup(layout, text, -1);
    pango_layout_set_width(layout, (options.paper_width - options.margin_left
                - options.margin_right - pc.numberwidth) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);

    for (i = 0; i < pango_layout_get_line_count(layout); ++i) {
        line = pango_layout_get_line_readonly(layout, i);
        if (i != 0) {
            pc.y += pc.font_height;
        }
        if (pc.y + pc.font_height >
                options.paper_height - options.margin_bottom) {
            newpage();
        }
        cairo_move_to(cr, pc.x, pc.y + pc.font_height - pc.font_descent);
        pango_cairo_show_layout_line(cr, line);
    }

    pango_font_description_free(desc);
    g_object_unref(layout);
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
        } else if (strcmp(command, "FONT") == 0) {
            command_font();
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

