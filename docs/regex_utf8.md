---
Project: Basis
Date: 2026-04-15
---

# tiny-regex-c UTF-8 Patch Notes

## Source

- Library: https://github.com/kokke/tiny-regex-c
- Files: `re.c` (~484 lines), `re.h` (~41 lines)
- License: Public domain

## Problem

tiny-regex-c is entirely byte-by-byte. It has no UTF-8 awareness:

- Every `text++` advances one byte, not one codepoint
- `matchone`, `matchrange`, `matchcharclass` all take and compare single `char` values
- `isdigit()`, `isalpha()`, `isspace()` are ASCII-only
- Character ranges like `[a-z]` are raw byte comparisons
- A multi-byte codepoint (e.g. `é` = 0xC3 0xA9) is seen as two separate characters

This means matching silently misbehaves on any non-ASCII input — wrong match
positions, split codepoints, false negatives on ranges.

## Fix: use r2_strings.h

`r2_strings.h` is already vendored and provides exactly what is needed:

```c
int  utf8_len(char ch);           // byte length of codepoint from its first byte
rune to_rune(const char chr[4]);  // decode up to 4 bytes into a uint32_t codepoint
```

The patch falls into three categories:

### 1. Pointer advancement — trivial (~8 places)

Every bare `text++` in `matchstar`, `matchplus`, `matchquestion`, and
`matchpattern` becomes:

```c
text += utf8_len(*text);
```

Non-ASCII bytes all have the high bit set, so null-termination still works
correctly with no other changes.

### 2. `matchone` and `matchrange` — moderate

`matchone` currently takes a single `char`. It needs to decode the current
text position into a `rune` before comparing:

```c
// before
static int matchone(regex_t p, char c) { ... }

// after
static int matchone(regex_t p, const char *text) {
    char tmp[4] = {0};
    int  blen   = utf8_len(*text);
    for (int i = 0; i < blen; i++) tmp[i] = text[i];
    rune cp = to_rune(tmp);
    ...
}
```

`matchrange` stores the low and high bound as bytes at `str[0]` and `str[2]`.
Those bounds need to be decoded to runes first, then compared as `uint32_t`:

```c
// before
return ((c >= str[0]) && (c <= str[2]));

// after — decode bounds from pattern buffer, compare as runes
rune lo = decode_rune_at(str);
rune hi = decode_rune_at(str + stride);
return (cp >= lo && cp <= hi);
```

This makes `[α-ω]` and similar Unicode ranges work correctly.

### 3. Character class storage — fiddly

The `ccl` buffer currently stores single bytes. For multi-byte characters in
a class like `[éàü]`, the simplest fix is to keep raw UTF-8 bytes in the
buffer and decode each entry on comparison (same `utf8_len` + `to_rune` loop).
This avoids changing the buffer format at the cost of a decode on every class
check — acceptable for a small regex engine.

## What stays ASCII-only (intentionally)

`\w`, `\d`, `\s` metaclasses stay ASCII-only. Defining "word character" or
"whitespace" for all of Unicode is a policy decision that varies by standard.
ASCII behaviour is predictable and sufficient for Basis use cases.

## Dot `.` behaviour

After the pointer-advance fix, `.` naturally skips whole codepoints because
`matchpattern` uses `utf8_len` to step. No further change needed.

## Estimated patch size

~60–80 lines changed in `re.c`. The library stays self-contained; r2_strings
slots straight in since it is already vendored. This is a fork+patch, not a
rewrite.

## matchlength return value

`re_match` and `re_matchp` return a byte offset and byte length. After patching,
these still return bytes — callers that need codepoint counts should use
`utf8_len` to convert. Document this clearly in the primitive wrapper.
