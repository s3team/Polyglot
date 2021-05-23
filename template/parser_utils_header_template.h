#ifndef __PARSER_UTILS__
#define __PARSER_UTILS__
#include<string>
#include<cassert>

enum TOKENSTATE{
TOKEN_NONE,
__TOKEN_STATE__
};

static TOKENSTATE token_state;

inline void set_token_state(TOKENSTATE a){
    token_state = a;
}

inline TOKENSTATE get_token_state(){
    return token_state;
};

#define look_forward_character(size) ({ \
    std::string result(size, 0); \
    char savebuf[0x100] = {0}; \
    unsigned index = 0; \
    FILE* saved_yyin = yyin; \
    int i = 0; \
    char c = 0; \
    for(i = 0; i < 0x100; i++){ \
        c = yyinput(yyscanner); \
        savebuf[i] = c; \
        if(c > 0x20){ \
            result[index++ ] = c; \
        }else if(c <= 0){ \
            savebuf[i-1] = '\0'; \
            break; \
        } \
        if(index == size) break; \
    } \
    while(i >= 0){ \
        unput(savebuf[i]); \
        i --; \
    } \
    result; \
})

#define peak(str)  ({\
    unsigned len = strlen(str); \
    std::string tmp_str = look_forward_character(len); \
    !strcmp(tmp_str.c_str(), str); \
})
        //std::cout << "unputing" << std::endl;
#endif