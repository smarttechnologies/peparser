#pragma once

#include <windows.h>

// CodeView header structures from "Undocumented Windows 2000 Secrets" by Sven B. Schreiber
// http://undocumented.rawol.com/win_pdbx.zip (last access 2016-04-04)

#pragma pack (1)

// -----------------------------------------------------------------

typedef struct _NB10I
{
	/*000*/ DWORD dwSig;
	/*004*/ DWORD dwOffset;
	/*008*/ DWORD sig;
	/*00C*/ DWORD age;
	/*010*/ BYTE  szPdb[MAX_PATH]; // PDB file name
								   /*114*/
}
NB10I, *PNB10I, **PPNB10I;

#define NB10I_ sizeof (NB10I)

// -----------------------------------------------------------------

typedef struct _RSDSI
{
	/*000*/ DWORD dwSig;
	/*004*/ GUID  guidSig;
	/*014*/ DWORD age;
	/*018*/ BYTE  szPdb[3 * MAX_PATH];
	/*324*/
}
RSDSI, *PRSDSI, **PPRSDSI;

#define RSDSI_ sizeof (RSDSI)

// -----------------------------------------------------------------

typedef union _CV
{
	/*000*/ DWORD dwSig;
	/*000*/ NB10I nb10i;
	/*000*/ RSDSI rsdsi;
	/*324*/
}
CV, *PCV, **PPCV;

#define CV_ sizeof (CV)

// -----------------------------------------------------------------

#pragma pack ()
