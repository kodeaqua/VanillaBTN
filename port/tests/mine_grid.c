#include "enhance/gameplay/mine_grid.h"

int main(void) {
    int col, row;
    if (!hm_mine_grid_cell(-2000, 2000, &col, &row) || col != 0 || row != 0)
        return 1;
    if (!hm_mine_grid_cell(1999, -1999, &col, &row) || col != 9 || row != 9)
        return 2;
    if (hm_mine_grid_cell(-2001, 0, &col, &row) ||
        hm_mine_grid_cell(2000, 0, &col, &row) ||
        hm_mine_grid_cell(0, 2001, &col, &row) ||
        hm_mine_grid_cell(0, -2000, &col, &row))
        return 3;
    return 0;
}
