#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <windows.h>
#include <winuser.h>
// #define memory_equal(name,val,type) (*(unsigned type*)&name == (unsigned type)val)
enum FCASE_FLAGS{
    //-1到-128为错误预留(你还是节约用吧)
    ERR_VIRTUAL_KEY_INPUT    = -1,
    //-129到-255为正常码
    LEFT         = -129,//L75
    RIGHT        = -130,//R77
    UP           = -131,//U72
    DOWN         = -132,//D80
    BACKSPACE    = -133,
    CTRL_A       = -134,
    CTRL_B       = -135,
    CTRL_C       = -136,
    CTRL_D       = -137,
    CTRL_E       = -138,
    CTRL_F       = -139,
    CTRL_G       = -140,
    CTRL_H       = -141,
    CTRL_I       = -142,
    CTRL_J       = -143,
    CTRL_K       = -144,
    CTRL_L       = -145,
    CTRL_M       = -146,
    CTRL_N       = -147,
    CTRL_O       = -148,
    CTRL_P       = -149,
    CTRL_Q       = -150,
    CTRL_R       = -151,
    CTRL_S       = -152,
    CTRL_T       = -153,
    CTRL_U       = -154,
    CTRL_V       = -155,
    CTRL_W       = -156,
    CTRL_X       = -157,
    CTRL_Y       = -158,
    CTRL_Z       = -159,
    //ASCII下虚拟码的第二位(或第一位)
    LEFT_ASCII   = 75,
    RIGHT_ASCII  = 77,
    UP_ASCII     = 72,
    DOWN_ASCII   = 80,
    CTRL_A_ASCII = 1,
    CTRL_B_ASCII = 2,
    CTRL_C_ASCII = 3,
    CTRL_D_ASCII = 4,
    CTRL_E_ASCII = 5,
    CTRL_F_ASCII = 6,
    CTRL_G_ASCII = 7,
    CTRL_H_ASCII = 127,//重要!
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
    //特殊标记
    VIRTUAL_KEY = 224
};
int fcase(){
    int c1,c2;
    switch (c1=getch()){
    case VIRTUAL_KEY:
        //警告:224在windows下才正常工作，是虚拟按键码的第一个部分
        switch (getch()){
        case LEFT_ASCII:    return LEFT;
        case RIGHT_ASCII:   return RIGHT;
        case UP_ASCII:      return UP;
        case DOWN_ASCII:    return DOWN;
        default:            return ERR_VIRTUAL_KEY_INPUT;
        }
        break;
    case BACKSPACE_ASCII:   return BACKSPACE;
    case CTRL_A_ASCII:      return CTRL_A;
    case CTRL_B_ASCII:      return CTRL_B;
    case CTRL_C_ASCII:      return CTRL_C;
    case CTRL_D_ASCII:      return CTRL_D;
    case CTRL_E_ASCII:      return CTRL_E;
    case CTRL_F_ASCII:      return CTRL_F;
    case CTRL_G_ASCII:      return CTRL_G;
    case CTRL_H_ASCII:      return CTRL_H;
    case CTRL_I_ASCII:      return CTRL_I;
    case CTRL_J_ASCII:      return CTRL_J;
    case CTRL_K_ASCII:      return CTRL_K;
    case CTRL_L_ASCII:      return CTRL_L;
    case CTRL_M_ASCII:      return CTRL_M;
    case CTRL_N_ASCII:      return CTRL_N;
    case CTRL_O_ASCII:      return CTRL_O;
    case CTRL_P_ASCII:      return CTRL_P;
    case CTRL_Q_ASCII:      return CTRL_Q;
    case CTRL_R_ASCII:      return CTRL_R;
    case CTRL_S_ASCII:      return CTRL_S;
    case CTRL_T_ASCII:      return CTRL_T;
    case CTRL_U_ASCII:      return CTRL_U;
    case CTRL_V_ASCII:      return CTRL_V;
    case CTRL_W_ASCII:      return CTRL_W;
    case CTRL_X_ASCII:      return CTRL_X;
    case CTRL_Y_ASCII:      return CTRL_Y;
    case CTRL_Z_ASCII:      return CTRL_Z;
    
    default:
        return c1;
    }
}
// int main(){
//     int c = 'A';
//     for(int i = 0; i <26; i++){
//         // printf("CTRL_%c_ASCII = %d,\n",(char)(c+i),i+1);
//         // printf("case CTRL_%c_ASCII:      return CTRL_%c;\n",(char)(c+i),(char)(c+i));
//     }
//     // return 0;
//     while(c!='q'){
//         // #define cast_T
//         #ifdef cast_T
//         switch ((c = fcase()))
//         {
//         case LEFT:puts("You press key LEFT");break;
//         case RIGHT:puts("You press key RIGHT");break;
//         case UP:puts("You press key UP");break;
//         case DOWN:puts("You press key DOWN");break;
//         case CTRL_A:puts("You press key CTRL_A");break;
        
//         default:
//             printf("You press key %c\n",(char)c);
//         }
//         #else
//         printf("%d ",(int)(c=getch()));
//         #endif
//     }
// }
int arr[10]={};
bool zf[10];
int pos = 0;
COORD cosolesize(){
    CONSOLE_SCREEN_BUFFER_INFO _t;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&_t);
    return _t.dwSize;
}
void line_clear(){
    int x = cosolesize().X;
    putchar('\r');
    for(int i = 1; i <= x; i++){
        putchar(' ');
    }
    putchar('\r');
}
void reload(){
    line_clear();
    if(pos>0){
        putchar('<');
    }
    printf("%d",pos);
    if(pos<9){
        putchar('>');
    }
    putchar('|');
    if(!zf[pos]){
        putchar('-');
    }
    printf("%d",arr[pos]);
}
void add(char c){
    arr[pos] = arr[pos]*10+((int)c-'0');
}
void sub(){
    arr[pos] /= 10;
}
void flip(){
    zf[pos]=!zf[pos];
}
#define is_digit(x) ((x>='0')&&(x<='9'))
int main(){
    int c;
    memset(zf,0x1,sizeof(zf));
    while (c!='q'){
        reload();
        switch ((c = fcase())){
            case LEFT:
                if(pos>0){
                    pos--;
                }
                break;
            case RIGHT:
                if(pos < 9){
                    pos++;
                }
                break;
            case BACKSPACE:
                sub();
                break;
            case '-':
                flip();
                break;
            default:
                if(is_digit(c)){
                    add(c);
                    break;
                }
                break;
        }

    }
    
}