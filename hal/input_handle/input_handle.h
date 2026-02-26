#ifndef _INPUT_HANDLE_H__
#define _INPUT_HANDLE_H__


typedef struct 
{
    void (*KeyBoardInit)(void);
    char (*KeyBoardLoop)(void);
    void (*KeyBoardDeInit)(void);
    
}BG_Input_Handle;

extern BG_Input_Handle BG_input_handle;

#endif