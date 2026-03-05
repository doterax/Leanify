#ifndef MAIN_H_
#define MAIN_H_

bool is_fast;
bool is_verbose;
#ifdef _WIN32
bool is_pause;
#endif  // _WIN32

// iteration of zopfli
int iterations;

// Allow altering hidden colors of fully transparent pixels, only for zopfli
bool zopflipng_lossy_transparent;

// a normal file: depth 1
// file inside zip that is inside another zip: depth 3
int max_depth;
bool parallel_processing = false;


#endif  // MAIN_H_
