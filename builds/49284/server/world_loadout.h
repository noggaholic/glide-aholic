#ifndef GLIDE_WORLD_LOADOUT49284_H
#define GLIDE_WORLD_LOADOUT49284_H

typedef struct Hot49284Loadout {
    unsigned char race, sex, profession;
    unsigned int armor[6], skins[6];
    struct {
        unsigned int item;
        unsigned char slot;
    } weapons[6];
} Hot49284Loadout;
#endif
