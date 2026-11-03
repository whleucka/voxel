// The one place miniaudio's implementation is compiled. Ogg Vorbis comes
// from stb_vorbis (v1.22, nothings/stb d14d46f), which miniaudio picks up
// when its header is seen first and its body after.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#define STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_FLAC
#include "miniaudio/miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb/stb_vorbis.c"
#pragma GCC diagnostic pop
