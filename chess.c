#include "chess.h"

char board[SIZE][SIZE];
char boardBackup[SIZE][SIZE];
int turnBackup = 1;
int canUndo = 0;
struct player p1 = {"Player 1", "P1-001"};
struct player p2 = {"Player 2", "P2-002"};
int whiteTurn = 1;
int wKingMoved = 0, bKingMoved = 0;
int wRookAMoved = 0, wRookHMoved = 0;
int bRookAMoved = 0, bRookHMoved = 0;
struct moveRecord moves[MAX_MOVES];
int moveCount = 0;

int selectedRow = -1;
int selectedCol = -1;
int lastFromRow = -1, lastFromCol = -1, lastToRow = -1, lastToCol = -1;
char statusMessage[128] = "Match Active - White Turn";
CapturedTracker capturedData = {0};

PieceAnimation anim = {0};

void setupBoard(void) {
    char initial[SIZE][SIZE] = {
        {'r','n','b','q','k','b','n','r'},
        {'p','p','p','p','p','p','p','p'},
        {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY},
        {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY},
        {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY},
        {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY},
        {'P','P','P','P','P','P','P','P'},
        {'R','N','B','Q','K','B','N','R'}
    };
    memcpy(board, initial, sizeof(board));
    whiteTurn = 1;
    wKingMoved = bKingMoved = 0;
    wRookAMoved = wRookHMoved = bRookAMoved = bRookHMoved = 0;
    canUndo = 0;
    moveCount = 0;
    selectedRow = -1;
    selectedCol = -1;
    lastFromRow = lastFromCol = lastToRow = lastToCol = -1;
    capturedData.wCount = 0;
    capturedData.bCount = 0;
    snprintf(statusMessage, sizeof(statusMessage), "Match Active - White Turn");
}

int isWhite(char p) { return p >= 'A' && p <= 'Z'; }
int isBlack(char p) { return p >= 'a' && p <= 'z'; }
int isEmpty(char p) { return p == EMPTY; }

int sameSide(char a, char b) {
    if (isEmpty(a) || isEmpty(b)) return 0;
    return (isWhite(a) && isWhite(b)) || (isBlack(a) && isBlack(b));
}

void backupState(void) {
    memcpy(boardBackup, board, sizeof(board));
    turnBackup = whiteTurn;
    canUndo = 1;
}

void undoMove(void) {
    if (!canUndo) {
        snprintf(statusMessage, sizeof(statusMessage), "No Move Available to Undo");
        return;
    }
    memcpy(board, boardBackup, sizeof(board));
    whiteTurn = turnBackup;
    canUndo = 0;
    if (moveCount > 0) moveCount--;
    selectedRow = selectedCol = -1;
    snprintf(statusMessage, sizeof(statusMessage), "Move Undone - %s's Turn", whiteTurn ? "White" : "Black");
}

void findKing(int white, int *outRow, int *outCol) {
    char target = white ? 'K' : 'k';
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == target) {
                *outRow = r;
                *outCol = c;
                return;
            }
        }
    }
}

int canMove(int fromRow, int fromCol, int toRow, int toCol, int white, int allowCastle) {
    if (fromRow < 0 || fromRow >= SIZE || fromCol < 0 || fromCol >= SIZE) return 0;
    if (toRow < 0 || toRow >= SIZE || toCol < 0 || toCol >= SIZE) return 0;
    if (fromRow == toRow && fromCol == toCol) return 0;

    char p = board[fromRow][fromCol];
    char dest = board[toRow][toCol];

    if (isEmpty(p)) return 0;
    if (white && !isWhite(p)) return 0;
    if (!white && !isBlack(p)) return 0;
    if (sameSide(p, dest)) return 0;

    int dr = toRow - fromRow;
    int dc = toCol - fromCol;
    int absDr = abs(dr);
    int absDc = abs(dc);

    switch (toupper((unsigned char)p)) {
        case 'P': {
            int dir = white ? -1 : 1;
            int startRow = white ? 6 : 1;

            if (dc == 0 && dest == EMPTY) {
                if (dr == dir) return 1;
                if (fromRow == startRow && dr == 2 * dir && board[fromRow + dir][fromCol] == EMPTY) return 1;
            }
            if (absDc == 1 && dr == dir && !isEmpty(dest) && !sameSide(p, dest)) return 1;
            return 0;
        }
        case 'N':
            return (absDr == 1 && absDc == 2) || (absDr == 2 && absDc == 1);
        case 'B':
            if (absDr != absDc) return 0;
            for (int i = 1; i < absDr; i++) {
                if (!isEmpty(board[fromRow + i * (dr > 0 ? 1 : -1)][fromCol + i * (dc > 0 ? 1 : -1)])) return 0;
            }
            return 1;
        case 'R':
            if (dr != 0 && dc != 0) return 0;
            int stepR = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
            int stepC = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);
            int steps = (dr != 0) ? absDr : absDc;
            for (int i = 1; i < steps; i++) {
                if (!isEmpty(board[fromRow + i * stepR][fromCol + i * stepC])) return 0;
            }
            return 1;
        case 'Q': {
            if (absDr == absDc || dr == 0 || dc == 0) {
                int stepR = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
                int stepC = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);
                int steps = (dr != 0) ? absDr : absDc;
                for (int i = 1; i < steps; i++) {
                    if (!isEmpty(board[fromRow + i * stepR][fromCol + i * stepC])) return 0;
                }
                return 1;
            }
            return 0;
        }
        case 'K':
            if (absDr <= 1 && absDc <= 1) return 1;
            if (allowCastle && dr == 0 && absDc == 2) {
                if (white && wKingMoved) return 0;
                if (!white && bKingMoved) return 0;
                if (inCheck(white)) return 0;

                if (dc == 2) {
                    if (white && wRookHMoved) return 0;
                    if (!white && bRookHMoved) return 0;
                    if (!isEmpty(board[fromRow][5]) || !isEmpty(board[fromRow][6])) return 0;
                    if (isAttacked(fromRow, 5, !white) || isAttacked(fromRow, 6, !white)) return 0;
                    return 1;
                }
                if (dc == -2) {
                    if (white && wRookAMoved) return 0;
                    if (!white && bRookAMoved) return 0;
                    if (!isEmpty(board[fromRow][1]) || !isEmpty(board[fromRow][2]) || !isEmpty(board[fromRow][3])) return 0;
                    if (isAttacked(fromRow, 2, !white) || isAttacked(fromRow, 3, !white)) return 0;
                    return 1;
                }
            }
            return 0;
    }
    return 0;
}

int isAttacked(int row, int col, int byWhite) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            char p = board[r][c];
            if (byWhite && isWhite(p)) {
                if (canMove(r, c, row, col, 1, 0)) return 1;
            } else if (!byWhite && isBlack(p)) {
                if (canMove(r, c, row, col, 0, 0)) return 1;
            }
        }
    }
    return 0;
}

int inCheck(int white) {
    int kRow, kCol;
    findKing(white, &kRow, &kCol);
    return isAttacked(kRow, kCol, !white);
}

int isLegalMove(int fromRow, int fromCol, int toRow, int toCol, int white) {
    if (!canMove(fromRow, fromCol, toRow, toCol, white, 1)) return 0;

    char tempBoard[SIZE][SIZE];
    memcpy(tempBoard, board, sizeof(board));

    board[toRow][toCol] = board[fromRow][fromCol];
    board[fromRow][fromCol] = EMPTY;

    int check = inCheck(white);
    memcpy(board, tempBoard, sizeof(board));

    return !check;
}

int hasAnyLegalMove(int white) {
    for (int fr = 0; fr < SIZE; fr++) {
        for (int fc = 0; fc < SIZE; fc++) {
            char p = board[fr][fc];
            if ((white && isWhite(p)) || (!white && isBlack(p))) {
                for (int tr = 0; tr < SIZE; tr++) {
                    for (int tc = 0; tc < SIZE; tc++) {
                        if (isLegalMove(fr, fc, tr, tc, white)) return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void startAnimation(int fromRow, int fromCol, int toRow, int toCol) {
    anim.active = 1;
    anim.piece = board[fromRow][fromCol];
    anim.startPos = (Vector2){ BOARD_OFFSET_X + fromCol * SQUARE_SIZE, BOARD_OFFSET_Y + fromRow * SQUARE_SIZE };
    anim.targetPos = (Vector2){ BOARD_OFFSET_X + toCol * SQUARE_SIZE, BOARD_OFFSET_Y + toRow * SQUARE_SIZE };
    anim.currentPos = anim.startPos;
    anim.progress = 0.0f;
    anim.toRow = toRow;
    anim.toCol = toCol;
}

void makeMove(int fromRow, int fromCol, int toRow, int toCol) {
    backupState();

    lastFromRow = fromRow;
    lastFromCol = fromCol;
    lastToRow = toRow;
    lastToCol = toCol;

    startAnimation(fromRow, fromCol, toRow, toCol);

    char p = board[fromRow][fromCol];
    char captured = board[toRow][toCol];

    if (captured != EMPTY) {
        if (isWhite(captured) && capturedData.wCount < 16) {
            capturedData.whiteCaptured[capturedData.wCount++] = captured;
        } else if (isBlack(captured) && capturedData.bCount < 16) {
            capturedData.blackCaptured[capturedData.bCount++] = captured;
        }
    }

    if (moveCount < MAX_MOVES) {
        moves[moveCount].piece = p;
        moves[moveCount].fromRow = fromRow;
        moves[moveCount].fromCol = fromCol;
        moves[moveCount].toRow = toRow;
        moves[moveCount].toCol = toCol;
        moves[moveCount].captured = captured;
        moves[moveCount].wasPromotion = 0;
    }

    if (toupper((unsigned char)p) == 'K' && abs(toCol - fromCol) == 2) {
        if (toCol == 6) {
            board[fromRow][5] = board[fromRow][7];
            board[fromRow][7] = EMPTY;
        } else if (toCol == 2) {
            board[fromRow][3] = board[fromRow][0];
            board[fromRow][0] = EMPTY;
        }
    }

    board[toRow][toCol] = p;
    board[fromRow][fromCol] = EMPTY;

    if (p == 'K') wKingMoved = 1;
    if (p == 'k') bKingMoved = 1;
    if (p == 'R' && fromRow == 7 && fromCol == 0) wRookAMoved = 1;
    if (p == 'R' && fromRow == 7 && fromCol == 7) wRookHMoved = 1;
    if (p == 'r' && fromRow == 0 && fromCol == 0) bRookAMoved = 1;
    if (p == 'r' && fromRow == 0 && fromCol == 7) bRookHMoved = 1;

    if (p == 'P' && toRow == 0) board[toRow][toCol] = 'Q';
    if (p == 'p' && toRow == 7) board[toRow][toCol] = 'q';

    if (moveCount < MAX_MOVES) moveCount++;
    whiteTurn = !whiteTurn;

    if (inCheck(whiteTurn)) {
        snprintf(statusMessage, sizeof(statusMessage), "CHECK! %s's Turn", whiteTurn ? "White" : "Black");
    } else {
        snprintf(statusMessage, sizeof(statusMessage), "%s's Turn", whiteTurn ? "White" : "Black");
    }

    if (!hasAnyLegalMove(whiteTurn)) {
        if (inCheck(whiteTurn)) {
            snprintf(statusMessage, sizeof(statusMessage), "CHECKMATE! %s Wins!", whiteTurn ? p2.name : p1.name);
            logResult(whiteTurn ? "Black Won by Checkmate" : "White Won by Checkmate");
        } else {
            snprintf(statusMessage, sizeof(statusMessage), "STALEMATE! Game Draw");
            logResult("Draw by Stalemate");
        }
    }
}

void saveGame(void) {
    FILE *fp = fopen(SAVE_FILE, "w");
    if (!fp) {
        snprintf(statusMessage, sizeof(statusMessage), "Error Saving File!");
        return;
    }
    fprintf(fp, "%s\n%s\n%s\n%s\n%d\n", p1.name, p1.roll, p2.name, p2.roll, whiteTurn);
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            fputc(board[r][c], fp);
        }
        fputc('\n', fp);
    }
    fclose(fp);
    snprintf(statusMessage, sizeof(statusMessage), "Game State Saved");
}

int loadGame(void) {
    FILE *fp = fopen(SAVE_FILE, "r");
    if (!fp) return 0;

    if (!fgets(p1.name, sizeof(p1.name), fp)) { fclose(fp); return 0; }
    p1.name[strcspn(p1.name, "\n")] = 0;

    if (!fgets(p1.roll, sizeof(p1.roll), fp)) { fclose(fp); return 0; }
    p1.roll[strcspn(p1.roll, "\n")] = 0;

    if (!fgets(p2.name, sizeof(p2.name), fp)) { fclose(fp); return 0; }
    p2.name[strcspn(p2.name, "\n")] = 0;

    if (!fgets(p2.roll, sizeof(p2.roll), fp)) { fclose(fp); return 0; }
    p2.roll[strcspn(p2.roll, "\n")] = 0;

    fscanf(fp, "%d\n", &whiteTurn);

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            board[r][c] = fgetc(fp);
        }
        fgetc(fp);
    }
    fclose(fp);
    canUndo = 0;
    moveCount = 0;
    snprintf(statusMessage, sizeof(statusMessage), "Saved Match Loaded");
    return 1;
}

void deleteSavedGame(void) { remove(SAVE_FILE); }

void logResult(const char *result) {
    FILE *fp = fopen(HISTORY_FILE, "a");
    if (!fp) return;
    fprintf(fp, "%s (%s) vs %s (%s) -> Result: %s\n", p1.name, p1.roll, p2.name, p2.roll, result);
    fclose(fp);
}

void drawVectorPiece(char piece, int x, int y, int size) {
    if (piece == EMPTY) return;

    int cX = x + size / 2;
    int cY = y + size / 2;
    int isW = isWhite(piece);

    Color primary = isW ? (Color){ 245, 245, 245, 255 } : (Color){ 30, 32, 38, 255 };
    Color secondary = isW ? (Color){ 210, 210, 210, 255 } : (Color){ 50, 54, 64, 255 };
    Color border = isW ? (Color){ 80, 80, 80, 255 } : (Color){ 10, 10, 10, 255 };
    Color symbolCol = isW ? (Color){ 20, 20, 20, 255 } : (Color){ 245, 245, 245, 255 };

    /* Base Shadow & Pedestal */
    DrawEllipse(cX, cY + size * 0.32f, size * 0.32f, size * 0.12f, (Color){ 0, 0, 0, 80 });
    DrawRectangleRounded((Rectangle){ cX - size * 0.30f, cY + size * 0.22f, size * 0.60f, size * 0.14f }, 0.4f, 4, border);
    DrawRectangleRounded((Rectangle){ cX - size * 0.28f, cY + size * 0.24f, size * 0.56f, size * 0.10f }, 0.4f, 4, primary);

    char type = toupper((unsigned char)piece);

    switch (type) {
        case 'P': /* Pawn */
            DrawTriangle((Vector2){ cX, cY - size * 0.05f }, (Vector2){ cX - size * 0.22f, cY + size * 0.22f }, (Vector2){ cX + size * 0.22f, cY + size * 0.22f }, border);
            DrawTriangle((Vector2){ cX, cY - size * 0.03f }, (Vector2){ cX - size * 0.19f, cY + size * 0.20f }, (Vector2){ cX + size * 0.19f, cY + size * 0.20f }, primary);
            DrawCircle(cX, cY - size * 0.12f, size * 0.16f, border);
            DrawCircle(cX, cY - size * 0.12f, size * 0.13f, primary);
            break;

        case 'R': /* Rook */
            DrawRectangle(cX - size * 0.20f, cY - size * 0.18f, size * 0.40f, size * 0.40f, border);
            DrawRectangle(cX - size * 0.17f, cY - size * 0.15f, size * 0.34f, size * 0.37f, primary);
            /* Battlements */
            DrawRectangle(cX - size * 0.22f, cY - size * 0.28f, size * 0.44f, size * 0.12f, border);
            DrawRectangle(cX - size * 0.19f, cY - size * 0.26f, size * 0.38f, size * 0.10f, secondary);
            DrawRectangle(cX - size * 0.06f, cY - size * 0.28f, size * 0.12f, size * 0.08f, (Color){ 18, 18, 24, 255 });
            break;

        case 'N': /* Knight */
            DrawCircle(cX - size * 0.05f, cY - size * 0.08f, size * 0.22f, border);
            DrawCircle(cX - size * 0.05f, cY - size * 0.08f, size * 0.19f, primary);
            DrawTriangle((Vector2){ cX - size * 0.22f, cY + size * 0.20f }, (Vector2){ cX - size * 0.10f, cY - size * 0.24f }, (Vector2){ cX + size * 0.18f, cY + size * 0.20f }, border);
            DrawTriangle((Vector2){ cX - size * 0.19f, cY + size * 0.18f }, (Vector2){ cX - size * 0.08f, cY - size * 0.21f }, (Vector2){ cX + size * 0.15f, cY + size * 0.18f }, primary);
            /* Eye */
            DrawCircle(cX - size * 0.08f, cY - size * 0.10f, size * 0.04f, symbolCol);
            break;

        case 'B': /* Bishop */
            DrawEllipse(cX, cY - size * 0.02f, size * 0.18f, size * 0.26f, border);
            DrawEllipse(cX, cY - size * 0.02f, size * 0.15f, size * 0.23f, primary);
            DrawCircle(cX, cY - size * 0.28f, size * 0.06f, border);
            DrawCircle(cX, cY - size * 0.28f, size * 0.04f, primary);
            /* Mitre Cut */
            DrawLineEx((Vector2){ cX - size * 0.08f, cY - size * 0.12f }, (Vector2){ cX + size * 0.08f, cY - size * 0.02f }, 3.0f, symbolCol);
            break;

        case 'Q': /* Queen */
            DrawTriangle((Vector2){ cX - size * 0.24f, cY - size * 0.12f }, (Vector2){ cX, cY + size * 0.20f }, (Vector2){ cX + size * 0.24f, cY - size * 0.12f }, border);
            DrawTriangle((Vector2){ cX - size * 0.21f, cY - size * 0.10f }, (Vector2){ cX, cY + size * 0.18f }, (Vector2){ cX + size * 0.21f, cY - size * 0.10f }, primary);
            /* Crown Orbs */
            DrawCircle(cX - size * 0.22f, cY - size * 0.18f, size * 0.05f, GOLD);
            DrawCircle(cX, cY - size * 0.26f, size * 0.06f, GOLD);
            DrawCircle(cX + size * 0.22f, cY - size * 0.18f, size * 0.05f, GOLD);
            break;

        case 'K': /* King */
            DrawRectangle(cX - size * 0.20f, cY - size * 0.10f, size * 0.40f, size * 0.32f, border);
            DrawRectangle(cX - size * 0.17f, cY - size * 0.08f, size * 0.34f, size * 0.29f, primary);
            /* Royal Cross */
            DrawRectangle(cX - size * 0.04f, cY - size * 0.30f, size * 0.08f, size * 0.20f, GOLD);
            DrawRectangle(cX - size * 0.10f, cY - size * 0.24f, size * 0.20f, size * 0.08f, GOLD);
            break;
    }
}

void renderGUI(void) {
    BeginDrawing();
    ClearBackground((Color){ 18, 18, 24, 255 });

    /* Board Frame */
    DrawRectangle(BOARD_OFFSET_X - 12, BOARD_OFFSET_Y - 12, SIZE * SQUARE_SIZE + 24, SIZE * SQUARE_SIZE + 24, (Color){ 35, 39, 48, 255 });
    DrawRectangleLines(BOARD_OFFSET_X - 12, BOARD_OFFSET_Y - 12, SIZE * SQUARE_SIZE + 24, SIZE * SQUARE_SIZE + 24, (Color){ 60, 66, 80, 255 });

    /* Draw Board Grid */
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            int posX = BOARD_OFFSET_X + c * SQUARE_SIZE;
            int posY = BOARD_OFFSET_Y + r * SQUARE_SIZE;

            Color sqColor = ((r + c) % 2 == 0) ? (Color){ 238, 238, 210, 255 } : (Color){ 118, 150, 86, 255 };

            /* Highlight Last Move */
            if ((r == lastFromRow && c == lastFromCol) || (r == lastToRow && c == lastToCol)) {
                sqColor = (Color){ 245, 246, 130, 255 };
            }

            /* Highlight Selected Piece Square */
            if (r == selectedRow && c == selectedCol) {
                sqColor = (Color){ 186, 202, 68, 255 };
            }

            DrawRectangle(posX, posY, SQUARE_SIZE, SQUARE_SIZE, sqColor);

            /* Render Move Indicators */
            if (selectedRow != -1 && isLegalMove(selectedRow, selectedCol, r, c, whiteTurn)) {
                if (isEmpty(board[r][c])) {
                    DrawCircle(posX + SQUARE_SIZE / 2, posY + SQUARE_SIZE / 2, 12, (Color){ 0, 0, 0, 45 });
                } else {
                    DrawCircleLines(posX + SQUARE_SIZE / 2, posY + SQUARE_SIZE / 2, SQUARE_SIZE / 2 - 4, (Color){ 230, 80, 80, 200 });
                }
            }

            /* Draw Static Piece */
            if (!(anim.active && r == anim.toRow && c == anim.toCol)) {
                drawVectorPiece(board[r][c], posX, posY, SQUARE_SIZE);
            }
        }
    }

    /* Draw Animated Sliding Piece */
    if (anim.active) {
        anim.progress += 0.14f;
        anim.currentPos.x = anim.startPos.x + (anim.targetPos.x - anim.startPos.x) * anim.progress;
        anim.currentPos.y = anim.startPos.y + (anim.targetPos.y - anim.startPos.y) * anim.progress;

        drawVectorPiece(anim.piece, (int)anim.currentPos.x, (int)anim.currentPos.y, SQUARE_SIZE);

        if (anim.progress >= 1.0f) anim.active = 0;
    }

    /* Board Rank/File Labels */
    for (int i = 0; i < SIZE; i++) {
        char fileStr[2] = { 'a' + i, '\0' };
        char rankStr[2] = { '8' - i, '\0' };
        DrawText(fileStr, BOARD_OFFSET_X + i * SQUARE_SIZE + 38, BOARD_OFFSET_Y + 8 * SQUARE_SIZE + 15, 16, GRAY);
        DrawText(rankStr, BOARD_OFFSET_X - 28, BOARD_OFFSET_Y + i * SQUARE_SIZE + 34, 16, GRAY);
    }

    /* Modern Dark Studio Sidebar */
    int pX = 800;
    DrawRectangle(pX, 40, 350, 712, (Color){ 26, 29, 38, 255 });
    DrawRectangleLines(pX, 40, 350, 712, (Color){ 50, 56, 70, 255 });

    DrawText("GRANDMASTER CHESS", pX + 25, 60, 22, GOLD);
    DrawLine(pX + 25, 95, pX + 325, 95, (Color){ 50, 56, 70, 255 });

    /* Player Cards */
    DrawRectangle(pX + 25, 110, 300, 55, (Color){ 35, 40, 52, 255 });
    DrawText("WHITE PLAYER", pX + 35, 118, 14, RAYWHITE);
    DrawText(p1.name, pX + 35, 136, 16, GOLD);

    DrawRectangle(pX + 25, 175, 300, 55, (Color){ 35, 40, 52, 255 });
    DrawText("BLACK PLAYER", pX + 35, 183, 14, RAYWHITE);
    DrawText(p2.name, pX + 35, 201, 16, GOLD);

    /* Game Status Bar */
    DrawRectangle(pX + 25, 250, 300, 45, (Color){ 45, 52, 68, 255 });
    DrawText(statusMessage, pX + 35, 264, 15, RAYWHITE);

    /* Action Control Buttons */
    int mouseX = GetMouseX();
    int mouseY = GetMouseY();

    Rectangle undoBtn = { pX + 25, 320, 140, 40 };
    Rectangle saveBtn = { pX + 185, 320, 140, 40 };
    Rectangle resetBtn = { pX + 25, 375, 300, 40 };

    bool hoverUndo = CheckCollisionPointRec((Vector2){ mouseX, mouseY }, undoBtn);
    bool hoverSave = CheckCollisionPointRec((Vector2){ mouseX, mouseY }, saveBtn);
    bool hoverReset = CheckCollisionPointRec((Vector2){ mouseX, mouseY }, resetBtn);

    DrawRectangleRec(undoBtn, hoverUndo ? (Color){ 70, 80, 105, 255 } : (Color){ 45, 52, 68, 255 });
    DrawText("UNDO MOVE", pX + 50, 332, 14, RAYWHITE);

    DrawRectangleRec(saveBtn, hoverSave ? (Color){ 70, 80, 105, 255 } : (Color){ 45, 52, 68, 255 });
    DrawText("SAVE GAME", pX + 210, 332, 14, RAYWHITE);

    DrawRectangleRec(resetBtn, hoverReset ? (Color){ 180, 60, 60, 255 } : (Color){ 140, 45, 45, 255 });
    DrawText("RESET MATCH", pX + 120, 387, 14, RAYWHITE);

    EndDrawing();
}

void handleMouseInput(void) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mousePos = GetMousePosition();
        int pX = 800;

        /* Check Button Clicks */
        Rectangle undoBtn = { pX + 25, 320, 140, 40 };
        Rectangle saveBtn = { pX + 185, 320, 140, 40 };
        Rectangle resetBtn = { pX + 25, 375, 300, 40 };

        if (CheckCollisionPointRec(mousePos, undoBtn)) {
            undoMove();
            return;
        }
        if (CheckCollisionPointRec(mousePos, saveBtn)) {
            saveGame();
            return;
        }
        if (CheckCollisionPointRec(mousePos, resetBtn)) {
            setupBoard();
            return;
        }

        /* Board Click Coordinate Translation */
        int col = (mousePos.x - BOARD_OFFSET_X) / SQUARE_SIZE;
        int row = (mousePos.y - BOARD_OFFSET_Y) / SQUARE_SIZE;

        if (row >= 0 && row < SIZE && col >= 0 && col < SIZE) {
            if (selectedRow == -1) {
                /* First Click: Select Piece */
                char piece = board[row][col];
                if (!isEmpty(piece) && ((whiteTurn && isWhite(piece)) || (!whiteTurn && isBlack(piece)))) {
                    selectedRow = row;
                    selectedCol = col;
                }
            } else {
                /* Second Click: Execute Move or Change Selection */
                if (isLegalMove(selectedRow, selectedCol, row, col, whiteTurn)) {
                    makeMove(selectedRow, selectedCol, row, col);
                    selectedRow = -1;
                    selectedCol = -1;
                } else if (!isEmpty(board[row][col]) && ((whiteTurn && isWhite(board[row][col])) || (!whiteTurn && isBlack(board[row][col])))) {
                    selectedRow = row;
                    selectedCol = col;
                } else {
                    selectedRow = -1;
                    selectedCol = -1;
                }
            }
        }
    }
}

void initGameApp(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "C Chess Engine - Raylib GUI");
    SetTargetFPS(60);

    setupBoard();

    while (!WindowShouldClose()) {
        handleMouseInput();
        renderGUI();
    }

    CloseWindow();
}
