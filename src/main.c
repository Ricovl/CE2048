/* Keep these headers */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tice.h>

/* Standard headers - it's recommended to leave them included */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared library headers -- depends on which ones you wish to use */
#include <debug.h>
#include <fileioc.h>
#include <graphx.h>

#include "gfx/numbers_gfx.h"

/* Some public variable initialization */
#define SIZE 4

uint8_t board[SIZE][SIZE], boardOld[SIZE][SIZE];

unsigned score = 0, scoreOld = 0, Hscore = 0;
bool gameOver = false;

gfx_rletsprite_t *num_sprite[11] = {num_2, num_4, num_8, num_16, num_32, num_64, num_128, num_256, num_512, num_1024, num_2048};

void move(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2);
void drawTile(uint24_t x_pos, uint8_t y_pos);
void drawNew(void);
bool canMove(void);
void draw_Screen(void);
void drawAllTiles(void);


void main(void) {
    sk_key_t key = 1;
    ti_var_t file;

    srand(rtc_Time());

    gfx_Begin();
    gfx_SetPalette(numbers_gfx_pal, sizeof_numbers_gfx_pal, 0);

	memset(&board, 0, sizeof(board));
	
    ti_CloseAll();

    file = ti_Open("CE2048", "r");
    if (file) {
        ti_Read(&board, sizeof(uint24_t), sizeof(board) / sizeof(uint8_t), file);
        ti_Read(&score, sizeof(uint24_t), sizeof(score) / sizeof(uint24_t), file);
        ti_Read(&Hscore, sizeof(uint24_t), sizeof(Hscore) / sizeof(uint24_t), file);
    } else
        drawNew();
		
    ti_CloseAll();

    draw_Screen();

    do {
        if (key) {
            // Draw the scores
            gfx_SetTextXY(256, 84);
            gfx_PrintUInt(score, 6);
            gfx_SetTextXY(256, 134);
            gfx_PrintUInt(Hscore, 6);

            if (!canMove())
                gameOver = true;
        }

        key = os_GetCSC();

        if (key == sk_Left)
            move(1, 0, 0, 0);
        else if (key == sk_Right)
            move(0, 1, 0, 0);
        else if (key == sk_Up)
            move(0, 0, 1, 0);
        else if (key == sk_Down)
            move(0, 0, 0, 1);

        // Del key pressed: new game screen
        if (key == sk_Del || gameOver) {
            uint8_t choice = 0;

            gfx_SetColor(0x04);
            gfx_FillRectangle_NoClip(61, 100, 118, 40);
            gfx_SetTextBGColor(0x04);
            gfx_SetTextFGColor(0x06);
            gfx_PrintStringXY("Start a New Game?", 61, 109);

            while (key != sk_Enter) {
                key = os_GetCSC();
                if (key == sk_Right || key == sk_Left)
                    choice ^= 1;
                gfx_SetTextFGColor((choice == 0) ? 0x0C : 0x06);
                gfx_PrintStringXY("Yes", 125, 124);
                gfx_SetTextFGColor((choice == 1) ? 0x0C : 0x06);
                gfx_PrintStringXY("No", 160, 124);
            }
            key = 1;

            if (choice == 0) {
                memset(&board, 0, sizeof(board));
                score = 0;
                drawNew();
            }
            draw_Screen();
        }

        // Graph key pressed: Undo last move
        if (key == sk_Graph) {
            memcpy(board, boardOld, sizeof(board));
            score = scoreOld;
			drawAllTiles();
		}
    } while (key != sk_Clear);

    file = ti_Open("CE2048", "w");
    if (file) {
        ti_Write(&board, sizeof(uint24_t), sizeof(board) / sizeof(uint8_t), file);
        ti_Write(&score, sizeof(uint24_t), sizeof(score) / sizeof(uint24_t), file);
        ti_Write(&Hscore, sizeof(uint24_t), sizeof(Hscore) / sizeof(uint24_t), file);
    }

    ti_SetArchiveStatus(true, file);

    gfx_End();
}

/* Draws a tile on the given position */
void drawTile(uint24_t x_pos, uint8_t y_pos) {
    uint8_t tile = board[y_pos][x_pos];

    // Calculate tile position
    x_pos = x_pos * 58 + 8;
    y_pos = y_pos * 58 + 8;

    // Draw tile background
    gfx_SetColor(tile + 6);

    gfx_HorizLine_NoClip(x_pos + 2, y_pos, 46);
    gfx_HorizLine_NoClip(x_pos + 1, y_pos + 1, 48);
    gfx_FillRectangle_NoClip(x_pos, y_pos + 2, 50, 46);
    gfx_HorizLine_NoClip(x_pos + 1, y_pos + 48, 48);
    gfx_HorizLine_NoClip(x_pos + 2, y_pos + 49, 46);

    // Draw number if position > 0 and <= 2048
    if (tile > 0 && tile < 12) {
        gfx_RLETSprite_NoClip(num_sprite[tile - 1],
                              x_pos + 25 - *(uint8_t *)(num_sprite[tile - 1]) / 2,
                              (uint8_t) ((y_pos + 25) - *((uint8_t*)num_sprite[tile - 1] + 1) / 2));
    }
}

// improved by Adriweb
void move(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2) {
    const int8_t delta = -(x1 + y1) + (x2 + y2);
    bool moved = false;
    bool moved_recent = false;
    unsigned i;
    int8_t x, y;
    uint8_t j;
    uint8_t boardTemp[SIZE][SIZE];

    memcpy(boardTemp, board, sizeof(board));
    scoreOld = score;

    for (j = 0; j <= (SIZE - 1) * 2; j++) {
        if (moved_recent && j != SIZE) {
            delay(35);
            moved_recent = false;
        }
        for (y = (SIZE - (1 + x1 + y2)) * (x2 + y2);
             (x1 == 0 && y1 == 0 ? y >= 0 : y < (SIZE - y1)); y -= delta) {
            for (x = (SIZE - (1 + x2 + y1)) * (x2 + y2);
                 (x1 == 0 && y1 == 0 ? x >= 0 : x < (SIZE - x1)); x -= delta) {
                uint8_t *val1 = &board[y + y1][x + x1];
                uint8_t *val2 = &board[y + y2][x + x2];

                if (*val1 > 0) {
                    if (j != SIZE && *val2 == 0) {
                        *val2 = *val1;
                        *val1 = 0;
                        moved = moved_recent = true;
                        drawTile(x, y);
                        drawTile(x + x2 + x1, y + y2 + y1);
                    } else if (j == SIZE && *val1 == *val2 && *val2) {
                        *val2 += 1;
                        *val1 = 0;
                        moved = moved_recent = true;

                        score += pow(2, *val2);
                        if (score > Hscore) {
                            Hscore = score;
                        }
                        drawTile(x, y);
                        drawTile(x + x2 + x1, y + y2 + y1);
                    }
                }
            }
        }
        if (j == SIZE && !moved_recent) {
            break;
        }
    }

    if (moved) {
        memcpy(boardOld, boardTemp, sizeof(board));
        drawNew();
    }
}

/* Checks if there is a possible move */
bool canMove(void) {
    uint8_t x, y;

    for (y = 0; y < SIZE; y++) {
        for (x = 0; x < SIZE; x++) {
            if ((x < SIZE - 1 && board[y][x] == board[y][x + 1]) ||
                board[y][x] == 0 ||
                (y < SIZE - 1 && board[y][x] == board[y + 1][x])) {
                return true;
            }
        }
    }
    return false;
}

/* Picks a random spot that is available and places a 2 or 4 tile */
void drawNew(void) {
    unsigned xr, yr;

    do {
        xr = rand() & 3;
        yr = rand() & 3;
    } while (board[yr][xr]);

    board[yr][xr] = (rand() % 10) > 8 ? 2 : 1;
    drawTile(xr, yr);
}

/* Draws all things on screen */
void draw_Screen(void) {
    // Draw the background
    gfx_FillScreen(0x01);
	gfx_SetColor(0x02);
    gfx_FillRectangle_NoClip(0, 0, 240, 240);

    // Draw the by Rico text
    gfx_SetTextBGColor(0x01);
    gfx_SetTextFGColor(0x02);
    gfx_PrintStringXY("By Rico", 256, 230);

    // Draw the undo text
    gfx_SetTextFGColor(0x0B);
    gfx_SetTextBGColor(0x02);
    gfx_PrintStringXY("[Graph]-Undo", 145, 0);

    // Draw score and best rectangle
    gfx_SetTextFGColor(0x07);
    gfx_FillRectangle_NoClip(250, 60, 60, 40);
    gfx_PrintStringXY("SCORE", 261, 69);
    gfx_FillRectangle_NoClip(250, 110, 60, 40);
    gfx_PrintStringXY("BEST", 263, 119);

    drawAllTiles();
    gameOver = false;
}

/* Draws all the tiles */
void drawAllTiles(void) {
	uint8_t x, y;
	for (y = 0; y < SIZE; y++) {
		for (x = 0; x < SIZE; x++) {
			drawTile(x, y);
		}
	}
}