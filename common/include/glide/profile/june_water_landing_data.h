#pragma once

namespace glide {
namespace beta {
struct WaterLandingPatch {
    unsigned build;
    const wchar_t* name;
    unsigned inputBytes, outputBytes, childOffset, insertOffset, poolOffset, children;
    unsigned char record[38], inputHash[32], outputHash[32];
};
inline const WaterLandingPatch* FindWaterLandingPatch(unsigned build) {
    static const WaterLandingPatch patches[] = {
        {14422,
         L"/5vYx",
         132828,
         132866,
         9385,
         33804,
         99508,
         6,
         {4,  0, 88, 119, 0,   0, 86, 0,   0,   0, 110, 131, 0, 0, 176, 119, 0,   0, 112,
          15, 0, 0,  206, 119, 0, 0,  220, 119, 0, 0,   246, 1, 0, 0,   252, 186, 0, 0},
         {75, 130, 57,  209, 237, 248, 96, 216, 7,   85, 155, 127, 167, 202, 8,   108,
          45, 136, 144, 50,  58,  240, 54, 177, 205, 20, 161, 178, 151, 70,  154, 133},
         {36,  16, 149, 173, 191, 15,  169, 108, 137, 205, 66,  153, 196, 166, 23,  51,
          180, 55, 92,  24,  49,  238, 87,  164, 77,  224, 210, 32,  164, 200, 157, 167}},
        {14505,
         L"/5vYx",
         132807,
         132845,
         9385,
         33804,
         99494,
         6,
         {4,  0, 88, 119, 0,   0, 86, 0,   0,   0, 110, 131, 0, 0, 176, 119, 0,   0, 112,
          15, 0, 0,  206, 119, 0, 0,  220, 119, 0, 0,   246, 1, 0, 0,   252, 186, 0, 0},
         {114, 235, 81, 241, 133, 165, 55,  131, 192, 223, 17,  11,  69,  125, 97,  110,
          120, 252, 95, 124, 240, 156, 252, 223, 117, 179, 209, 155, 107, 132, 146, 214},
         {175, 126, 105, 151, 90,  215, 190, 217, 232, 67, 91, 252, 71,  171, 96,  215,
          252, 0,   69,  106, 140, 90,  162, 126, 145, 80, 31, 190, 157, 2,   176, 104}},
    };
    for (const auto& patch : patches)
        if (patch.build == build)
            return &patch;
    return nullptr;
}
} // namespace beta
} // namespace glide
