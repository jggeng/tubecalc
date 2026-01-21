#include <stdlib.h>
#include <stdio.h>
#include "ig_base.h"
#include "BendTable.h"

#define HANDMADE_MATH_IMPLEMENTATION
#define HANDMADE_MATH_CPP_MODE
#include "HandmadeMath.h"

#include "ig_util.cpp"
#include "ig_tokenizer.cpp"

internal void
printVec3(hmm_vec3 v3, b32 newline = TRUE)
{
    printf("X:%.3f, Y:%.3f, Z:%.3f", v3.X, v3.Y, v3.Z);
    if (newline)
        printf("\n");
}


inline r32 
calcBendAngle(hmm_vec3 a, hmm_vec3 b)
{
    r32 Result = HMM_ACosF(HMM_Dot(a, b) / (HMM_Length(a) * HMM_Length(b)));

    return(Result);
}

inline r32
ToDegrees(float Radians)
{
    float Result = 0.0f;

    Result = Radians * (180.0f / HMM_PI32);
    return (Result);
}

internal void
fillBendTable(bendTable *BendTable, hmm_vec4 *Rows, u32 Count)
{
    //printf("Count is %d\n", Count);
    Assert(BendTable);
    Assert(!BendTable->RowCount);

    //TODO(ig): allocate this with an arena
    BendTable->Row = (bendTableRow*)malloc(sizeof(bendTableRow) * (size_t)Count);
    BendTable->Vec = (bendTableRowVector*)malloc(sizeof(bendTableRowVector) * (size_t)(Count - 1));
    BendTable->Plane = (bendTablePlane*)malloc(sizeof(bendTablePlane) * (size_t)(Count - 2));
    if (Count > 3)
    {
        BendTable->TubeRotation = (bendTableTubeRotation*)malloc(sizeof(bendTableTubeRotation) * (size_t)(Count - 3));
    }

    //Copy Rows
    for (u32 iter = 0; iter < Count; iter++)
    {
        BendTable->Row[iter].Point = Rows[iter].XYZ;
        BendTable->Row[iter].Radius = Rows[iter].W;
        BendTable->Row[iter].Angle = 0.0f;
        BendTable->RowCount++;
    }
    Assert(Count == BendTable->RowCount);

    //Calculate Vectors
    for (u32 iter = 0; iter < (Count - 1); iter++)
    {
        BendTable->Vec[iter].Vector = BendTable->Row[iter+1].Point - BendTable->Row[iter].Point;
        BendTable->Vec[iter].Magnitude = HMM_Length(BendTable->Vec[iter].Vector);
        BendTable->Vec[iter].Length = 0.0f;
    }

    //Calculate Planes, Angles
    for (u32 iter = 0; iter < (Count - 2); iter++)
    {
        BendTable->Plane[iter].PlaneNormal = HMM_Normalize(HMM_Cross(BendTable->Vec[iter].Vector, BendTable->Vec[iter+1].Vector));
        BendTable->Plane[iter].BendAngle = calcBendAngle(BendTable->Vec[iter].Vector, BendTable->Vec[iter+1].Vector);
        BendTable->Plane[iter].ArcLength = BendTable->Plane[iter].BendAngle * BendTable->Row[iter+1].Radius;
        BendTable->Plane[iter].PathAdjustment = BendTable->Row[iter+1].Radius * HMM_TanF(BendTable->Plane[iter].BendAngle / 2.0f);
        //printf("PathAdjustment: %f %d \n", BendTable->Plane[iter].PathAdjustment, iter);
    }


    //Calculate Tube Roation Angles
    for (u32 iter = 0; iter < (Count - 3); iter++)
    {
        BendTable->TubeRotation[iter].RotationAngle = 
            HMM_ACosF(
                HMM_Dot(BendTable->Plane[iter].PlaneNormal, 
                        BendTable->Plane[iter+1].PlaneNormal));
    }

    //Calculate Side Lengths
    BendTable->CenterlineLength = 0.0f;

    r32 prevAdj = 0.0f;
    for (u32 iter = 0; iter < (Count - 1); iter++)
    {
        r32 postAdj = 0.0f;
        if (iter < (Count - 2))
        {
            postAdj = BendTable->Plane[iter].PathAdjustment;
        }
        
        r32 Len =         
                BendTable->Vec[iter].Magnitude - prevAdj - postAdj;

        BendTable->Vec[iter].Length = Len;

        //printf("__prev %f, post %f, len %f\n", prevAdj, postAdj, Len);
        prevAdj = postAdj;

        BendTable->CenterlineLength += Len;
        if (iter < (Count - 2))
            BendTable->CenterlineLength += BendTable->Plane[iter].ArcLength;
    }
}

internal void
debugPrintBendTable(bendTable *BendTable)
{
    printf("%d Rows, %f Total Length\n", BendTable->RowCount, BendTable->CenterlineLength);

    for (u32 r=0; r<BendTable->RowCount; r++)
    {
        printVec3(BendTable->Row[r].Point, FALSE);
        printf(" R: %.1f A: %.2f ", BendTable->Row[r].Radius, ToDegrees(BendTable->Row[r].Angle));
        if (r > 0)
        {
            printVec3(BendTable->Vec[r-1].Vector, FALSE);
            printf(" LEN: %.2f ", BendTable->Vec[r-1].Length);
            if (r > 1)
            {
                printVec3(BendTable->Plane[r-2].PlaneNormal, FALSE);
                printf(" Ang: %.2f Arc: %.2f Adj: %.2f", ToDegrees(BendTable->Plane[r-2].BendAngle), BendTable->Plane[r-2].ArcLength, BendTable->Plane[r-2].PathAdjustment);
                
                if (r > 2)
                {
                    printf(" TubeRot: %f ", ToDegrees(BendTable->TubeRotation[r-3].RotationAngle));
                }            
            }
        }
        printf("\n");
    }
}

internal void
printBendTable(bendTable *BendTable)
{
    printf("Bend Table Calculator\n");
    printf("POINT\tX\tY\tZ\tRADIUS\n");
    for (u32 r=0; r<BendTable->RowCount; r++)
    {
        hmm_vec3 *p  = &BendTable->Row[r].Point;
        printf("%d\t%0.3f\t%0.3f\t%0.3f\t%0.1f\n",
            r+1, p->X, p->Y, p->Z, BendTable->Row[r].Radius);
    }
    printf("-----------------------------------------------------\n");
    printf("%d Rows \t\t\t\t%0.3f TOTAL LENGTH\n", BendTable->RowCount, BendTable->CenterlineLength);

    printf("\nVECTOR\tX\tY\tZ\tLENGTH\n");
    for (u32 r=0; r<BendTable->RowCount - 1; r++)
    {
        bendTableRowVector *v = &BendTable->Vec[r];
        printf("%d\t%0.3f\t%0.3f\t%0.3f\t%0.3f\n",
            r+1, v->Vector.X, v->Vector.Y, v->Vector.Z, v->Length);
    }
    printf("\n");


    printf("\nPLANES\tBEND_ANGLE\tI\tJ\tK\tARC_LEN\t\tPATH_ADJ\n");
    for (u32 r=0; r<BendTable->RowCount - 2; r++)
    {
        bendTablePlane *p = &BendTable->Plane[r];
        printf("%d\t%0.1f\t\t%0.3f\t%0.3f\t%0.3f\t%0.3f\t\t%0.3f\n",
            r+1, 
            ToDegrees(p->BendAngle),
            p->PlaneNormal.X,
            p->PlaneNormal.Y,
            p->PlaneNormal.Z,
            p->ArcLength,
            p->PathAdjustment);
    }

    printf("\nTUBE_ROTATIONS\tROTATION_ANGLE\n");
    for (u32 r=0; r<BendTable->RowCount - 3; r++)
    {
        printf("%d\t\t%0.1f\n", r+1, ToDegrees(BendTable->TubeRotation[r].RotationAngle));
    }
}


internal hmm_vec4*
getBendTableFromCSV(char *csv, int *rowcount)
{
    hmm_vec4 *Result = 0;
    Assert(csv);
    
    //Setup Tokenizer
    tokenizer Tokenizer;
    Tokenizer.At = csv;
    
    //Check for how many lines of input
    token tok = GetToken(&Tokenizer);
    u32 newlinesFollowedByNumber = 0;
    b32 checkNext = TRUE;
    while (tok.Type != Token_EndOfStream)
    {
        if (tok.Type == Token_EndOfLine)
        {
            checkNext = TRUE;
        }
        else if (checkNext) //don't check next on the end of line token.
        {
            if ((tok.Type == Token_Number) || (tok.Type == Token_Minus))
            {
                ++newlinesFollowedByNumber;
            }
            checkNext = FALSE;
        }
        tok = GetToken(&Tokenizer);
    }

    //printf("Found %d lines\n", newlinesFollowedByNumber);
    
    if (newlinesFollowedByNumber >= 3 && newlinesFollowedByNumber < 25)
    {
        int valid_rows = 0;
        Tokenizer.At = csv;
        hmm_vec4 *table = (hmm_vec4*)malloc(newlinesFollowedByNumber * sizeof(hmm_vec4));
        
        for (int r=0; r<newlinesFollowedByNumber; r++)
        {
            int rc = 0;
            tok = GetToken(&Tokenizer);
            while (tok.Type != Token_EndOfStream && tok.Type != Token_EndOfLine)
            {
                if (tok.Type == Token_Number && rc < 4)
                {
                    table[r].Elements[rc++] = NumberTokenToFloat(tok); 
                }
                else if (rc >= 4)
                {
                    printf("too many numbers withthout a break\\n");
                }
                tok = GetToken(&Tokenizer);
            }
            if (rc == 3 || rc == 4)
            {
                //printf("Valid Row!\n");
                ++valid_rows;
            }
            //printVec3(table[r].XYZ, TRUE);

        }
        if (valid_rows > 2)
        {
            //printf("valid table!\n");
            Result = table;
            *rowcount = valid_rows;
        }

    }

    return (Result);
}

int
main(int argc, char **args)
{
    char *bendCSV = 0;
    bendTable BendTable = {};

    if(argc == 1)
    {
            hmm_vec4 table_rows[] =
            {   {0.0f,  0.0f,   0.0f,   0.0f},
                {0.0f,  0.0f,   2.93f,  4.00f},
                {8.02f, 4.33f,  10.70f, 4.00f},
                {8.02f, 6.89f,  12.18f, 0.0f}
            };
        
        fillBendTable(&BendTable, table_rows, ArrayCount(table_rows));
        printBendTable(&BendTable);
    }
    else if(argc == 2)
    {
        bendCSV = ReadEntireFileIntoMemoryAndNullTerminate(args[1]);
        if (bendCSV)
        {
            //printf("Read file .\\%s\n", args[1]);
            hmm_vec4 *table = 0;
            int rowcount = 0;
            table = getBendTableFromCSV(bendCSV, &rowcount);
            if (table && rowcount)
            {
                fillBendTable(&BendTable, table, rowcount);
                printBendTable(&BendTable);
                //debugPrintBendTable(&BendTable);
            }
            else
            {
                printf("Could not parse CSV\n");
            }
        }
        else
        {
            printf("Invalid filename %s\n", args[1]);
        }
    }

    else
    {
        printf("Useage: tubecalc.exe || tubecalc.exe bendtable.csv\n");
    }
    
    return 0;
}