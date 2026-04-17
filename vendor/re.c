/*
 * tiny-regex-c — public domain
 * Original: https://github.com/kokke/tiny-regex-c
 * UTF-8 patch for basis: codepoint-aware matching via self-contained helpers.
 * See docs/regex_utf8.md for patch strategy.
 *
 * Returns byte offsets/lengths. \w \d \s stay ASCII-only (intentional).
 */

#include "re.h"
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MAX_REGEXP_OBJECTS  30
#define MAX_CHAR_CLASS_LEN  40
#define RE_MAX_STAR_STEPS   (MAX_REGEXP_OBJECTS + 2)

enum {
    UNUSED, DOT, BEGIN, END, QUESTIONMARK, STAR, PLUS,
    CHAR, CHAR_CLASS, INV_CHAR_CLASS,
    DIGIT, NOT_DIGIT, ALPHA, NOT_ALPHA, WHITESPACE, NOT_WHITESPACE
};

typedef struct regex_t {
    unsigned char  type;
    union {
        unsigned char  ch;
        unsigned char* ccl;
    } u;
} regex_t;

/* Private declarations */
static int matchpattern(regex_t* pattern, const char* text, int* matchlength);
static int matchcharclass(const char* text, const char* str);
static int matchstar(regex_t p, regex_t* pattern, const char* text, int* matchlength);
static int matchplus(regex_t p, regex_t* pattern, const char* text, int* matchlength);
static int matchquestion(regex_t p, regex_t* pattern, const char* text, int* matchlength);
static int matchone(regex_t p, const char* text);
static int matchdigit(char c);
static int matchwhitespace(char c);
static int matchalphanum(char c);
static int matchmetachar(char c, const char* str);
static int matchrange(const char* text, const char* str);
static int matchdot(const char* text);
static int ismetachar(char c);

/* UTF-8 helpers — self-contained so re.c has no external dependencies */
static int re_utf8_len(unsigned char c)
{
    if (c < 0x80) return 1;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

static uint32_t re_utf8_decode(const char *s)
{
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) return c;
    if (c < 0xE0) return ((c & 0x1F) << 6)  | ((unsigned char)s[1] & 0x3F);
    if (c < 0xF0) return ((c & 0x0F) << 12) | (((unsigned char)s[1] & 0x3F) << 6)  | ((unsigned char)s[2] & 0x3F);
    return          ((c & 0x07) << 18) | (((unsigned char)s[1] & 0x3F) << 12)
                  | (((unsigned char)s[2] & 0x3F) << 6)  | ((unsigned char)s[3] & 0x3F);
}

/* Public API */
int re_match(const char* pattern, const char* text, int* matchlength)
{
    return re_matchp(re_compile(pattern), text, matchlength);
}

int re_matchp(re_t pattern, const char* text, int* matchlength)
{
    *matchlength = 0;
    if (pattern == 0) return -1;

    if (pattern[0].type == BEGIN)
        return matchpattern(&pattern[1], text, matchlength) ? 0 : -1;

    /* scan text codepoint-by-codepoint; idx tracks byte offset */
    int idx = 0;
    while (1) {
        if (matchpattern(pattern, text, matchlength)) {
            if (text[0] == '\0') return -1;
            return idx;
        }
        if (text[0] == '\0') break;
        int adv = re_utf8_len((unsigned char)text[0]);
        idx  += adv;
        text += adv;
    }
    return -1;
}

re_t re_compile(const char* pattern)
{
    static regex_t re_compiled[MAX_REGEXP_OBJECTS];
    static unsigned char ccl_buf[MAX_CHAR_CLASS_LEN];
    int ccl_bufidx = 1;
    char c;
    int i = 0, j = 0;

    while (pattern[i] != '\0' && (j + 1 < MAX_REGEXP_OBJECTS)) {
        c = pattern[i];
        switch (c) {
            case '^': re_compiled[j].type = BEGIN;        break;
            case '$': re_compiled[j].type = END;          break;
            case '.': re_compiled[j].type = DOT;          break;
            case '*': re_compiled[j].type = STAR;         break;
            case '+': re_compiled[j].type = PLUS;         break;
            case '?': re_compiled[j].type = QUESTIONMARK; break;
            case '\\':
                if (pattern[i+1] != '\0') {
                    i++;
                    switch (pattern[i]) {
                        case 'd': re_compiled[j].type = DIGIT;          break;
                        case 'D': re_compiled[j].type = NOT_DIGIT;      break;
                        case 'w': re_compiled[j].type = ALPHA;          break;
                        case 'W': re_compiled[j].type = NOT_ALPHA;      break;
                        case 's': re_compiled[j].type = WHITESPACE;     break;
                        case 'S': re_compiled[j].type = NOT_WHITESPACE; break;
                        default:
                            re_compiled[j].type = CHAR;
                            re_compiled[j].u.ch = (unsigned char)pattern[i];
                            break;
                    }
                }
                break;
            case '[': {
                int buf_begin = ccl_bufidx;
                if (pattern[i+1] == '^') {
                    re_compiled[j].type = INV_CHAR_CLASS;
                    i++;
                    if (pattern[i+1] == '\0') return 0;
                } else {
                    re_compiled[j].type = CHAR_CLASS;
                }
                while ((pattern[++i] != ']') && (pattern[i] != '\0')) {
                    if (pattern[i] == '\\') {
                        if (ccl_bufidx >= MAX_CHAR_CLASS_LEN - 1) return 0;
                        if (pattern[i+1] == '\0') return 0;
                        ccl_buf[ccl_bufidx++] = (unsigned char)pattern[i++];
                    } else if (ccl_bufidx >= MAX_CHAR_CLASS_LEN) {
                        return 0;
                    }
                    ccl_buf[ccl_bufidx++] = (unsigned char)pattern[i];
                }
                if (ccl_bufidx >= MAX_CHAR_CLASS_LEN) return 0;
                ccl_buf[ccl_bufidx++] = 0;
                re_compiled[j].u.ccl = &ccl_buf[buf_begin];
                break;
            }
            default:
                re_compiled[j].type = CHAR;
                re_compiled[j].u.ch = (unsigned char)c;
                break;
        }
        if (pattern[i] == '\0') return 0;
        i++;
        j++;
    }
    re_compiled[j].type = UNUSED;
    return (re_t)re_compiled;
}

/* Private functions */
static int matchdigit(char c)      { return isdigit((unsigned char)c); }
static int matchalpha(char c)      { return isalpha((unsigned char)c); }
static int matchwhitespace(char c) { return isspace((unsigned char)c); }
static int matchalphanum(char c)   { return c == '_' || matchalpha(c) || matchdigit(c); }

static int ismetachar(char c)
{
    return (c == 's' || c == 'S' || c == 'w' || c == 'W' || c == 'd' || c == 'D');
}

static int matchmetachar(char c, const char* str)
{
    switch (str[0]) {
        case 'd': return  matchdigit(c);
        case 'D': return !matchdigit(c);
        case 'w': return  matchalphanum(c);
        case 'W': return !matchalphanum(c);
        case 's': return  matchwhitespace(c);
        case 'S': return !matchwhitespace(c);
        default:  return  (c == str[0]);
    }
}

static int matchdot(const char *text)
{
#if defined(RE_DOT_MATCHES_NEWLINE) && (RE_DOT_MATCHES_NEWLINE == 1)
    (void)text; return 1;
#else
    return text[0] != '\n' && text[0] != '\r';
#endif
}

/* UTF-8 aware range: str points at "lo-hi" in the ccl buffer (may be multi-byte) */
static int matchrange(const char *text, const char* str)
{
    int lo_len = re_utf8_len((unsigned char)str[0]);
    if (str[lo_len] != '-' || str[lo_len + 1] == '\0') return 0;

    uint32_t cp = re_utf8_decode(text);
    uint32_t lo = re_utf8_decode(str);
    uint32_t hi = re_utf8_decode(str + lo_len + 1);

    return (cp != (uint32_t)'-') && (cp >= lo) && (cp <= hi);
}

/* Walk the ccl buffer understanding multi-byte codepoints */
static int matchcharclass(const char *text, const char* str)
{
    while (str[0] != '\0') {
        if (str[0] == '\\') {
            if (matchmetachar(text[0], &str[1])) return 1;
            if (text[0] == str[1] && !ismetachar(str[1])) return 1;
            str += 2;
        } else {
            int s_len = re_utf8_len((unsigned char)str[0]);
            if (str[s_len] == '-' && str[s_len + 1] != '\0') {
                /* range: lo '-' hi */
                if (matchrange(text, str)) return 1;
                int hi_len = re_utf8_len((unsigned char)str[s_len + 1]);
                str += s_len + 1 + hi_len;
            } else {
                /* literal codepoint */
                int t_len = re_utf8_len((unsigned char)text[0]);
                if (t_len == s_len && memcmp(text, str, (size_t)t_len) == 0) return 1;
                str += s_len;
            }
        }
    }
    return 0;
}

/* matchone now takes a pointer so multi-byte codepoints can be decoded */
static int matchone(regex_t p, const char *text)
{
    char c = text[0];  /* first byte — sufficient for ASCII metaclasses */
    switch (p.type) {
        case DOT:            return  matchdot(text);
        case CHAR_CLASS:     return  matchcharclass(text, (const char*)p.u.ccl);
        case INV_CHAR_CLASS: return !matchcharclass(text, (const char*)p.u.ccl);
        case DIGIT:          return  matchdigit(c);
        case NOT_DIGIT:      return !matchdigit(c);
        case ALPHA:          return  matchalphanum(c);
        case NOT_ALPHA:      return !matchalphanum(c);
        case WHITESPACE:     return  matchwhitespace(c);
        case NOT_WHITESPACE: return !matchwhitespace(c);
        default:             return  ((unsigned char)p.u.ch == (unsigned char)c);
    }
}

/* Store codepoint boundary pointers for correct greedy backtracking */
static int matchstar(regex_t p, regex_t* pattern, const char* text, int* matchlength)
{
    int prelen = *matchlength;
    const char* pts[RE_MAX_STAR_STEPS];
    int n = 0;
    pts[n++] = text;
    while (text[0] != '\0' && matchone(p, text)) {
        text += re_utf8_len((unsigned char)text[0]);
        if (n < RE_MAX_STAR_STEPS) pts[n++] = text;
    }
    for (int i = n - 1; i >= 0; i--) {
        *matchlength = prelen + (int)(pts[i] - pts[0]);
        if (matchpattern(pattern, pts[i], matchlength)) return 1;
    }
    *matchlength = prelen;
    return 0;
}

static int matchplus(regex_t p, regex_t* pattern, const char* text, int* matchlength)
{
    int prelen = *matchlength;
    const char* pts[RE_MAX_STAR_STEPS];
    int n = 0;
    pts[n++] = text;
    while (text[0] != '\0' && matchone(p, text)) {
        text += re_utf8_len((unsigned char)text[0]);
        if (n < RE_MAX_STAR_STEPS) pts[n++] = text;
    }
    for (int i = n - 1; i >= 1; i--) {  /* at least one codepoint consumed */
        *matchlength = prelen + (int)(pts[i] - pts[0]);
        if (matchpattern(pattern, pts[i], matchlength)) return 1;
    }
    return 0;
}

static int matchquestion(regex_t p, regex_t* pattern, const char* text, int* matchlength)
{
    if (p.type == UNUSED) return 1;
    if (matchpattern(pattern, text, matchlength)) return 1;
    if (text[0] && matchone(p, text)) {
        int adv = re_utf8_len((unsigned char)text[0]);
        if (matchpattern(pattern, text + adv, matchlength)) {
            (*matchlength) += adv;
            return 1;
        }
    }
    return 0;
}

static int matchpattern(regex_t* pattern, const char* text, int* matchlength)
{
    int pre = *matchlength;
    while (1) {
        if ((pattern[0].type == UNUSED) || (pattern[1].type == QUESTIONMARK))
            return matchquestion(pattern[0], &pattern[2], text, matchlength);
        if (pattern[1].type == STAR)
            return matchstar(pattern[0], &pattern[2], text, matchlength);
        if (pattern[1].type == PLUS)
            return matchplus(pattern[0], &pattern[2], text, matchlength);
        if ((pattern[0].type == END) && pattern[1].type == UNUSED)
            return (text[0] == '\0');
        if (text[0] == '\0' || !matchone(*pattern, text)) break;
        int adv = re_utf8_len((unsigned char)text[0]);
        (*matchlength) += adv;
        pattern++;
        text += adv;
    }
    *matchlength = pre;
    return 0;
}
