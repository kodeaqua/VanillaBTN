#ifndef HMBTN_MINE_GRID_H
#define HMBTN_MINE_GRID_H

/* The displayed mine grid spans [-2000, 2000) on X and (-2000, 2000]
 * on Z. Check bounds before division, which truncates towards zero in C. */
static inline int hm_mine_grid_cell(int x, int z, int *col, int *row) {
    if (x < -2000 || x >= 2000 || z > 2000 || z <= -2000)
        return 0;
    *col = (x + 2000) / 400;
    *row = (2000 - z) / 400;
    return 1;
}

#endif
