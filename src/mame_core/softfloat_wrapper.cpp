// C++ wrapper for softfloat C functions
// Softfloat headers don't have extern "C" guards, so C++ code gets name mangling issues
// This provides C++ callable wrappers for the C implementations

// Forward declare the C types
struct extFloat80_t;
typedef double float64_t;

// Declare the C function with C linkage
extern "C" {
    float64_t extF80M_to_f64(const extFloat80_t*);
}

// Provide C++ wrapper with the mangled name that dvmemory.cpp expects
struct extFloat80M;
double extF80M_to_f64(const extFloat80M* ptr) {
    return extF80M_to_f64(reinterpret_cast<const extFloat80_t*>(ptr));
}
