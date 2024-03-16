#ifndef CONFIG_HPP
#define CONFIG_HPP

//#define NDEBUG

namespace defopts {

constexpr int TT_SIZE = 128;
constexpr int TT_SIZE_MIN = 1;
constexpr int TT_SIZE_MAX = 1024;

constexpr int MULTIPV = 1;
constexpr int MULTIPV_MIN = 1;
constexpr int MULTIPV_MAX = 255;

constexpr char NNUE_PATH[] = "saturn.nnue";

constexpr int ASP_INIT_DELTA = 22; //tuned
constexpr int ASP_INIT_MIN = 1;
constexpr int ASP_INIT_MAX = 256;

constexpr int ASP_MIN_DEPTH = 7; //tuned
constexpr int ASP_MIN_DEPTH_MIN = 1;
constexpr int ASP_MIN_DEPTH_MAX = 64;

constexpr float LMR_COEFF_MIN = 0.25f;
constexpr float LMR_COEFF = 0.5f;
constexpr float LMR_COEFF_MAX = 2.f;

}

#endif
