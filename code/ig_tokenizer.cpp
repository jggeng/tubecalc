#include "ig_base.h"

enum token_type
{
    Token_Unknown,
    
    Token_OpenParen,    
    Token_CloseParen,    
    Token_Colon,    
    Token_Semicolon,    
    Token_Asterisk,    
    Token_OpenBracket,    
    Token_CloseBracket,    
    Token_OpenBrace,    
    Token_CloseBrace,
    Token_Minus,
    Token_Comma,

    Token_String,
    Token_Number,    
    Token_Identifier,
    Token_HexOrIdentifier,
    Token_PCANDump,
    Token_AEADump,
    Token_EndOfLine,    

    Token_EndOfStream,
};

struct token
{
    token_type Type;
    
    size_t TextLength;
    char *Text;
};


struct tokenizer
{
    char *At;
};

inline bool
IsEndOfLine(char C)
{
    bool Result = ((C == '\n') ||
                   (C == '\r'));

    return(Result);
}

inline bool
IsWhitespace(char C)
{
    bool Result = ((C == ' ') ||
                   (C == '\t') ||
                   (C == '\v') ||
                   (C == '\f'));

                   // || //Do not want to eat end of line characters in this data format.
                   //IsEndOfLine(C));

    return(Result);
}

static void
EatAllWhitespace(tokenizer *Tokenizer)
{
    for(;;)
    {
        if(IsWhitespace(Tokenizer->At[0]))
        {
            ++Tokenizer->At;
        }
        // cpp comment
        else if((Tokenizer->At[0] == '/') &&
                (Tokenizer->At[1] == '/'))
        {
            Tokenizer->At += 2;
            while(Tokenizer->At[0] && !IsEndOfLine(Tokenizer->At[0]))
            {
                ++Tokenizer->At;
            }
        }

        //c comment
        else if((Tokenizer->At[0] == '/') &&
                (Tokenizer->At[1] == '*'))
        {
            Tokenizer->At += 2;
            while(Tokenizer->At[0] &&
                  !((Tokenizer->At[0] == '*') &&
                    (Tokenizer->At[1] == '/')))
            {
                ++Tokenizer->At;
            }
            
            if(Tokenizer->At[0] == '*')
            {
                Tokenizer->At += 2;
            }
        }
        else
        {
            break;
        }
    }
}

inline bool
IsAlpha(char C)
{
    bool Result = (((C >= 'a') && (C <= 'z')) ||
                   ((C >= 'A') && (C <= 'Z')));

    return(Result);
}

inline bool
IsHex(char C)
{
    bool Result = (((C >= '0') && (C <= '9')) ||
                    ((C >='a') && (C <='f')) ||
                    ((C >='A') && (C <='F')));
    return(Result);
}

inline bool
IsNumber(char C)
{
    bool Result = ((C >= '0') && (C <= '9'));
    
    return(Result);
}

inline bool
TokenEquals(token Token, char *Match)
{
    char *At = Match;
    for(int Index = 0;
        Index < Token.TextLength;
        ++Index, ++At)
    {
        if((*At == 0) ||
           (Token.Text[Index] != *At))
        {
            return(false);
        }
    }
    bool Result = (*At == 0);
    return(Result);
}

static token
GetToken(tokenizer *Tokenizer)
{
    EatAllWhitespace(Tokenizer);

    token Token = {};
    Token.TextLength = 1;
    Token.Text = Tokenizer->At;
    char C = Tokenizer->At[0];
    ++Tokenizer->At;
    switch(C)
    {
        case '\0': {Token.Type = Token_EndOfStream;} break;
           
        case '(': {Token.Type = Token_OpenParen;} break;
        case ')': {Token.Type = Token_CloseParen;} break;
        case ':': {Token.Type = Token_Colon;} break;
        case ';': {Token.Type = Token_Semicolon;} break;
        case '*': {Token.Type = Token_Asterisk;} break;
        case '[': {Token.Type = Token_OpenBracket;} break;
        case ']': {Token.Type = Token_CloseBracket;} break;
        case '{': {Token.Type = Token_OpenBrace;} break;
        case '}': {Token.Type = Token_CloseBrace;} break;
        case ',': {Token.Type = Token_Comma;} break;
                        
        case '"':
        {
            Token.Type = Token_String;

            Token.Text = Tokenizer->At;
            
            while(Tokenizer->At[0] &&
                  Tokenizer->At[0] != '"')
            {
                if((Tokenizer->At[0] == '\\') &&
                   Tokenizer->At[1])
                {
                    ++Tokenizer->At;
                }                
                ++Tokenizer->At;
            }

            Token.TextLength = Tokenizer->At - Token.Text;
            if(Tokenizer->At[0] == '"')
            {
                ++Tokenizer->At;
            }
        } break;

        default:
        {
            if (IsEndOfLine(C))
            {
                Token.Type = Token_EndOfLine;
                break;
            }
            // Start by assuming we have a number, not an identifier.

            if (IsAlpha(C))
            {
                Token.Type = Token_Identifier;
                while((Tokenizer->At[0] && !IsEndOfLine(Tokenizer->At[0])) && 
                        (IsAlpha(Tokenizer->At[0]) || IsNumber(Tokenizer->At[0])))
                {
                    ++Tokenizer->At;
                }
                Token.TextLength = Tokenizer->At - Token.Text;
            }
            else if ((C == '-') && !IsNumber(Tokenizer->At[0]))
            {
                Token.Type = Token_Minus;
                break;
            }
            else if ((C == '-') || (IsNumber(C)))
            {
                Token.Type = Token_Number;
				while(Tokenizer->At[0] && !IsEndOfLine(Tokenizer->At[0]))
                {
                    if ((Tokenizer->At[0] == '.') || IsNumber(Tokenizer->At[0]))
                    {
                        ++Tokenizer->At;
                    }
                    else
                    {
                        break;
                    }
                }
                Token.TextLength = Tokenizer->At - Token.Text;
            }
        } break;
    }
    return(Token);
}


static bool
RequireToken(tokenizer *Tokenizer, token_type DesiredType)
{
    token Token = GetToken(Tokenizer);
    bool Result = (Token.Type == DesiredType);
    return(Result);
}

static float
NumberTokenToFloat(token number)
{
    //assert(number.Type == Token_Number);
    float Result = 0;

    char *temp = (char*)malloc(number.TextLength + 1);
    for (int i =0; i<number.TextLength; i++)
    {
        temp[i] = number.Text[i];
    }
    temp[number.TextLength] = 0;
    Result = atof(temp);
    free(temp);

    return(Result);
}

static char
ByteTokenToChar(token number)
{
   // Assert(number.TextLength < 2);
    unsigned char fullResult = 0;
    for (int i = 0; 
        i < number.TextLength;
        ++i)
    {
        fullResult = fullResult * 16;
		if ((number.Text[i] >= '0') && (number.Text[i] <= '9'))
		{
			fullResult = fullResult + (number.Text[i] - '0');
		}
		
        else if ((number.Text[i] >= 'a') && (number.Text[i] <= 'f'))
		{
			fullResult = fullResult + (number.Text[i] - 'a' + 10);
		}
		
        else if ((number.Text[i] >= 'A') && (number.Text[i] <= 'F'))
		{
			fullResult = fullResult + (number.Text[i] - 'A' + 10);
		}
		
        else
		{
			printf("CRASH AND BURN\n");
			fullResult = 255;
		}
    }
    //truncate!
    return((char)fullResult);
}

static int
NumberTokenToInt(token number)
{
    int Result = 0;
    Result = (int)strtol(number.Text, &number.Text + number.TextLength, 0);
    return(Result);   
}

static int
NumberTokenHexToInt(token number)
{
    int Result = 0;
    Result = (int)strtol(number.Text, &number.Text + number.TextLength, 16);
    return(Result);   
}