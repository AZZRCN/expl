#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <windows.h>
#include <winuser.h>

#define ARR_LENGTH 100
#define ARR_SIZE 10
#define MIN_CHARS_FOR_ELLIPSIS 4
#define INPUT_BUFFER_SIZE 128

typedef enum {
    ERR_VIRTUAL_KEY_INPUT = -1,
    LEFT = -129,
    RIGHT = -130,
    UP = -131,
    DOWN = -132,
    BACKSPACE = -133,
    CTRL_A = -134,
    CTRL_B = -135,
    CTRL_C = -136,
    CTRL_D = -137,
    CTRL_E = -138,
    CTRL_F = -139,
    CTRL_G = -140,
    CTRL_H = -141,
    CTRL_I = -142,
    CTRL_J = -143,
    CTRL_K = -144,
    CTRL_L = -145,
    CTRL_M = -146,
    CTRL_N = -147,
    CTRL_O = -148,
    CTRL_P = -149,
    CTRL_Q = -150,
    CTRL_R = -151,
    CTRL_S = -152,
    CTRL_T = -153,
    CTRL_U = -154,
    CTRL_V = -155,
    CTRL_W = -156,
    CTRL_X = -157,
    CTRL_Y = -158,
    CTRL_Z = -159,
    LEFT_ASCII = 75,
    RIGHT_ASCII = 77,
    UP_ASCII = 72,
    DOWN_ASCII = 80,
    CTRL_A_ASCII = 1,
    CTRL_B_ASCII = 2,
    CTRL_C_ASCII = 3,
    CTRL_D_ASCII = 4,
    CTRL_E_ASCII = 5,
    CTRL_F_ASCII = 6,
    CTRL_G_ASCII = 7,
    CTRL_H_ASCII = 127,
    CTRL_I_ASCII = 9,
    CTRL_J_ASCII = 10,
    CTRL_K_ASCII = 11,
    CTRL_L_ASCII = 12,
    CTRL_M_ASCII = 13,
    CTRL_N_ASCII = 14,
    CTRL_O_ASCII = 15,
    CTRL_P_ASCII = 16,
    CTRL_Q_ASCII = 17,
    CTRL_R_ASCII = 18,
    CTRL_S_ASCII = 19,
    CTRL_T_ASCII = 20,
    CTRL_U_ASCII = 21,
    CTRL_V_ASCII = 22,
    CTRL_W_ASCII = 23,
    CTRL_X_ASCII = 24,
    CTRL_Y_ASCII = 25,
    CTRL_Z_ASCII = 26,
    BACKSPACE_ASCII = 8,
    VIRTUAL_KEY = 224
} KeyCode;

typedef struct {
    HANDLE hInput;
    INPUT_RECORD* ibufs;
} ReadStruct;

typedef struct {
    void* data;
    int count;
} Vec;

typedef struct {
    char arr[ARR_SIZE][ARR_LENGTH];
    char* write[ARR_SIZE];
    int pos;
    int x;
    int y;
} EditorState;

int fcase() {
    int c1;
    switch (c1 = getch()) {
    case VIRTUAL_KEY:
        switch (getch()) {
        case LEFT_ASCII: return LEFT;
        case RIGHT_ASCII: return RIGHT;
        case UP_ASCII: return UP;
        case DOWN_ASCII: return DOWN;
        default: return ERR_VIRTUAL_KEY_INPUT;
        }
    case BACKSPACE_ASCII: return BACKSPACE;
    case CTRL_A_ASCII: return CTRL_A;
    case CTRL_B_ASCII: return CTRL_B;
    case CTRL_C_ASCII: return CTRL_C;
    case CTRL_D_ASCII: return CTRL_D;
    case CTRL_E_ASCII: return CTRL_E;
    case CTRL_F_ASCII: return CTRL_F;
    case CTRL_G_ASCII: return CTRL_G;
    case CTRL_H_ASCII: return CTRL_H;
    case CTRL_I_ASCII: return CTRL_I;
    case CTRL_J_ASCII: return CTRL_J;
    case CTRL_K_ASCII: return CTRL_K;
    case CTRL_L_ASCII: return CTRL_L;
    case CTRL_M_ASCII: return CTRL_M;
    case CTRL_N_ASCII: return CTRL_N;
    case CTRL_O_ASCII: return CTRL_O;
    case CTRL_P_ASCII: return CTRL_P;
    case CTRL_Q_ASCII: return CTRL_Q;
    case CTRL_R_ASCII: return CTRL_R;
    case CTRL_S_ASCII: return CTRL_S;
    case CTRL_T_ASCII: return CTRL_T;
    case CTRL_U_ASCII: return CTRL_U;
    case CTRL_V_ASCII: return CTRL_V;
    case CTRL_W_ASCII: return CTRL_W;
    case CTRL_X_ASCII: return CTRL_X;
    case CTRL_Y_ASCII: return CTRL_Y;
    case CTRL_Z_ASCII: return CTRL_Z;
    default: return c1;
    }
}

COORD consolesize() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    return info.dwSize;
}

void line_clear(EditorState* state) {
    state->x = consolesize().X;
    putchar('\r');
    for (int i = 1; i <= state->x; i++) {
        putchar(' ');
    }
    putchar('\r');
}

__forceinline int dec_len(int x) {
    int ans = 0;
    do {
        ans++;
        x /= 10;
    } while (x);
    return ans;
}

void reload(EditorState* state) {
    line_clear(state);
    int used = 0;
    if (state->pos > 0) {
        putchar('<');
        used++;
    }
    printf("%d", state->pos);
    used += dec_len(state->pos);
    if (state->pos < (ARR_SIZE - 1)) {
        putchar('>');
        used++;
    }
    putchar('|');
    char* t = state->arr[state->pos];
    while (*t && state->x - used > MIN_CHARS_FOR_ELLIPSIS) {
        putchar(*(t++));
        used++;
    }
    if (*t) {
        putchar('.');
        putchar('.');
        putchar('.');
    }
}

void add(EditorState* state, char c) {
    if (state->write[state->pos] - state->arr[state->pos] < ARR_LENGTH) {
        *(state->write[state->pos]++) = c;
        *state->write[state->pos] = '\0';
    }
}

void sub(EditorState* state) {
    if (state->write[state->pos] > state->arr[state->pos]) {
        *(--state->write[state->pos]) = '\0';
    }
}

#define is_digit(x) ((x >= '0') && (x <= '9'))
#define is_char(x) ((x >= 32) && (x <= 126))

int init(ReadStruct* rs) {
    rs->hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (rs->hInput == INVALID_HANDLE_VALUE) {
        return -1;
    }
    rs->ibufs = (INPUT_RECORD*)malloc(INPUT_BUFFER_SIZE * sizeof(INPUT_RECORD));
    if (rs->ibufs == NULL) {
        return -1;
    }
    return 0;
}

void resize(Vec* vec, int single_size, int length) {
    vec->data = malloc(single_size * length);
    vec->count = length;
}

Vec* get(ReadStruct* rs) {
    DWORD eventsRead;
    Vec* ret = (Vec*)malloc(sizeof(Vec));
    if (ret == NULL) {
        return NULL;
    }
    resize(ret, sizeof(char), INPUT_BUFFER_SIZE);
    char* write = (char*)ret->data;
    if (PeekConsoleInput(rs->hInput, rs->ibufs, INPUT_BUFFER_SIZE, &eventsRead) && eventsRead > 0) {
        DWORD count;
        ReadConsoleInput(rs->hInput, rs->ibufs, eventsRead, &count);
        for (DWORD j = 0; j < count; j++) {
            if (rs->ibufs[j].EventType == KEY_EVENT &&
                rs->ibufs[j].Event.KeyEvent.bKeyDown) {
                *(write++) = rs->ibufs[j].Event.KeyEvent.uChar.AsciiChar;
            }
        }
    }
    ret->count = write - (char*)ret->data;
    return ret;
}

void cleanup(ReadStruct* rs) {
    if (rs->ibufs != NULL) {
        free(rs->ibufs);
        rs->ibufs = NULL;
    }
}

void cleanup_vec(Vec* vec) {
    if (vec != NULL) {
        if (vec->data != NULL) {
            free(vec->data);
            vec->data = NULL;
        }
        free(vec);
    }
}

int main() {
    EditorState state;
    state.pos = 0;
    state.x = 0;
    state.y = 0;
    for (int i = 0; i < ARR_SIZE; i++) {
        state.write[i] = state.arr[i];
        state.arr[i][0] = '\0';
    }

    int c;
    while (c != 'q') {
        reload(&state);
        switch ((c = fcase())) {
        case LEFT:
            if (state.pos > 0) {
                state.pos--;
            }
            break;
        case RIGHT:
            if (state.pos < (ARR_SIZE - 1)) {
                state.pos++;
            }
            break;
        case BACKSPACE:
            sub(&state);
            break;
        default:
            if (is_char(c)) {
                add(&state, c);
            }
            break;
        }
    }

    return 0;
}
