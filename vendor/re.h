/*
 * tiny-regex-c — public domain
 * Original: https://github.com/kokke/tiny-regex-c
 * UTF-8 patch applied — see docs/regex_utf8.md
 *
 * re_match / re_matchp return byte offsets and byte lengths.
 * \w \d \s metaclasses are ASCII-only (intentional).
 */

#ifndef _TINY_REGEX_C
#define _TINY_REGEX_C

#ifndef RE_DOT_MATCHES_NEWLINE
#define RE_DOT_MATCHES_NEWLINE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct regex_t* re_t;

re_t re_compile(const char* pattern);
int  re_matchp(re_t pattern, const char* text, int* matchlength);
int  re_match(const char* pattern, const char* text, int* matchlength);

#ifdef __cplusplus
}
#endif

#endif /* _TINY_REGEX_C */
