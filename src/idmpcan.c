/* 

 idmpcan - A utility for dumping IRIG 106 Ch 10 CAN data

 Copyright (c) 2008 Irig106.org

 All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are 
 met:

   * Redistributions of source code must retain the above copyright 
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright 
     notice, this list of conditions and the following disclaimer in the 
     documentation and/or other materials provided with the distribution.

   * Neither the name Irig106.org nor the names of its contributors may 
     be used to endorse or promote products derived from this software 
     without specific prior written permission.

 This software is provided by the copyright holders and contributors 
 "as is" and any express or implied warranties, including, but not 
 limited to, the implied warranties of merchantability and fitness for 
 a particular purpose are disclaimed. In no event shall the copyright 
 owner or contributors be liable for any direct, indirect, incidental, 
 special, exemplary, or consequential damages (including, but not 
 limited to, procurement of substitute goods or services; loss of use, 
 data, or profits; or business interruption) however caused and on any 
 theory of liability, whether in contract, strict liability, or tort 
 (including negligence or otherwise) arising in any way out of the use 
 of this software, even if advised of the possibility of such damage.

  Created by: Tommaso Falchi Delitala <volalto86@gmail.com>
  (based on idmpuart utility)
  
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "config.h"
#include "i106_stdint.h"
#include "irig106ch10.h"

#include "i106_time.h"
#include "i106_decode_time.h"
#include "i106_decode_can.h"
#include "i106_decode_tmats.h"


/*
 * Macros and definitions
 * ----------------------
 */

#define MAJOR_VERSION  "01"
#define MINOR_VERSION  "00"

#if !defined(bTRUE)
#define bTRUE   (1==1)
#define bFALSE  (1==0)
#endif


/*
 * Data structures
 * ---------------
 */


/*
 * Module data
 * -----------
 */

int           m_iVersion;    // Data file version
int           m_usMaxBuffSize;

int           m_iI106Handle;


/*
 * Function prototypes
 * -------------------
 */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile);
void vUsage(void);


/* ------------------------------------------------------------------------ */

int main(int argc, char ** argv)
  {

    char                    szInFile[256];      // Input file name
    char                    szOutFile[256];     // Output file name
    int                     iArgIdx;
    FILE                  * psuOutFile;         // Output file handle

    int                     iChannel;           // Channel number
    unsigned long           lMsgs = 0;          // Total message
    long                    lCanMsgs = 0;
    int                     bVerbose;
    unsigned                uWordIdx;

    int                     bPrintTMATS;
    int                     bPrintRTC;
    unsigned long           ulBuffSize = 0L;

    EnI106Status            enStatus;
    SuI106Ch10Header        suI106Hdr;

    unsigned char         * pvBuff  = NULL;
    SuCan_CurrMsg           suCanMsg;
    SuTmatsInfo             suTmatsInfo;


/*
 * Process the command line arguements
 */

    if (argc < 2) 
        {
        vUsage();
        return 1;
        }

    iChannel        = -1;

    bVerbose        = bFALSE;            // No verbosity
    bPrintTMATS     = bFALSE;
    bPrintRTC       = bFALSE;

    szInFile[0]  = '\0';
    strcpy(szOutFile,"");                     // Default is stdout

    for (iArgIdx=1; iArgIdx<argc; iArgIdx++) 
        {

        switch (argv[iArgIdx][0]) 
            {
            case '-' :
                switch (argv[iArgIdx][1]) 
                    {
                    case 'v' :                   // Verbose switch
                        bVerbose = bTRUE;
                        break;

                    case 'c' :                   // Channel number
                        iArgIdx++;
                        sscanf(argv[iArgIdx],"%d",&iChannel);
                        break;

                    case 'T' :                   // Print TMATS flag
                        bPrintTMATS = bTRUE;
                        break;

                    case 'R' :                   // Print relative time counter
                        bPrintRTC = bTRUE;
                        break;

                    default :
                        break;
                    } // end flag switch
                break;

            default :
                if (szInFile[0] == '\0') strcpy(szInFile, argv[iArgIdx]);
                else                     strcpy(szOutFile,argv[iArgIdx]);
                break;

            } // end command line arg switch
        } // end for all arguments

    if (strlen(szInFile)==0) 
        {
        vUsage();
        return 1;
        }

//  uDecCnt = uDecimation;

/*
 * Opening banner
 * --------------
 */

    fprintf(stderr, "\nIDMPCAN "MAJOR_VERSION"."MINOR_VERSION"\n");
    fprintf(stderr, "Freeware Copyright (C) 2014 Irig106.org\n\n");

/*
 *  Open file and allocate a buffer for reading data.
 */

    enStatus = enI106Ch10Open(&m_iI106Handle, szInFile, I106_READ);

    switch (enStatus)
        {
        case I106_OPEN_WARNING :
            fprintf(stderr, "Warning opening data file : Status = %d\n", enStatus);
            break;
        case I106_OK :
            break;
        default :
            fprintf(stderr, "Error opening data file : Status = %d\n", enStatus);
            return 1;
            break;
        }

    enStatus = enI106_SyncTime(m_iI106Handle, bFALSE, 0);
    if (enStatus != I106_OK)
        {
        fprintf(stderr, "Error establishing time sync : Status = %d\n", enStatus);
        return 1;
        }


/*
 * Open the output file
 */

    // If output file specified then open it    
    if (strlen(szOutFile) != 0)
        {
        psuOutFile = fopen(szOutFile,"w");
        if (psuOutFile == NULL) 
            {
            fprintf(stderr, "Error opening output file\n");
            return 1;
            }
        }

    // No output file name so use stdout
    else
        {
        psuOutFile = stdout;
        }


/*
 * Read the first header. If TMATS flag set, print TMATS and exit
 */

    // Read first header and check for data read errors
    enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);
    if (enStatus != I106_OK)
        return 1;

    // If TMATS flag set, just print TMATS and exit
    if (bPrintTMATS == bTRUE)
        {
        if (suI106Hdr.ubyDataType == I106CH10_DTYPE_TMATS)
            {
            // Make a data buffer for TMATS
            pvBuff = malloc(suI106Hdr.ulPacketLen);

            // Read the data buffer and check for read errors
            enStatus = enI106Ch10ReadData(m_iI106Handle, suI106Hdr.ulPacketLen, pvBuff);
            if (enStatus != I106_OK)
                return 1;

            // Process the TMATS info
            memset( &suTmatsInfo, 0, sizeof(suTmatsInfo) );
            enStatus = enI106_Decode_Tmats(&suI106Hdr, pvBuff, &suTmatsInfo);
            if (enStatus != I106_OK) 
                {
                fprintf(stderr, " Error processing TMATS record : Status = %d\n", enStatus);
                return 1;
                }

            vPrintTmats(&suTmatsInfo, psuOutFile);
            } // end if TMATS

        // TMATS not first message
        else
            {
            printf("Error - TMATS message not found\n");
            return 1;
            }

        return 0;
        } // end if print TMATS

/*
 * Read messages until error or EOF
 */

    lMsgs = 1;

    while (1==1) 
        {

        // Read the next header
        enStatus = enI106Ch10ReadNextHeader(m_iI106Handle, &suI106Hdr);

        // Setup a one time loop to make it easy to break out on error
        do
            {
            if (enStatus == I106_EOF)
                break;

            // Check for header read errors
            if (enStatus != I106_OK)
                break;

            // If CAN message then process it
            // Note: Some vendors may use "message" type for CAN (but still use CAN data format)
            // if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_MESSAGE) && 
            if ((suI106Hdr.ubyDataType == I106CH10_DTYPE_CAN) &&
                ((iChannel == -1) || (iChannel == (int)suI106Hdr.uChID)))
                {

                // Make sure our buffer is big enough, size *does* matter
                if (ulBuffSize < suI106Hdr.ulPacketLen)
                    {
                    pvBuff = realloc(pvBuff, suI106Hdr.ulPacketLen);
                    ulBuffSize = suI106Hdr.ulPacketLen;
                    }

                // Read the data buffer
                enStatus = enI106Ch10ReadData(m_iI106Handle, ulBuffSize, pvBuff);

                // Check for data read errors
                if (enStatus != I106_OK)
                    break;

                lMsgs++;
                if (bVerbose) 
                    fprintf(stderr, "%8.8ld Messages \r",lMsgs);

                // Step through all CAN messages
                enStatus = enI106_Decode_FirstCan(&suI106Hdr, pvBuff, &suCanMsg);
                while (enStatus == I106_OK)
                    {

                    if (bPrintRTC == bFALSE)
                        {
                        enI106_RelInt2IrigTime(m_iI106Handle, suCanMsg.suTimeRef.uRelTime, &suCanMsg.suTimeRef.suIrigTime);
                        fprintf(psuOutFile,"%s ", IrigTime2String(&suCanMsg.suTimeRef.suIrigTime));
                        }
                    else
                        {
                        fprintf(psuOutFile,"%14lld ", suCanMsg.suTimeRef.uRelTime);
                        }

                    // Print out the data
                    fprintf(psuOutFile," Chan%d-%d ",suI106Hdr.uChID, suCanMsg.psuCanHdr->uSubChannel);

                    for (uWordIdx=0; uWordIdx < suCanMsg.psuCanHdr->uMsgLength; ++uWordIdx) 
                        // Print out as hex characters
                        fprintf(psuOutFile,"%2.2x ",suCanMsg.pauData[uWordIdx]);


                    fprintf(psuOutFile,"\n");
                    fflush(psuOutFile);

                    lCanMsgs++;
                    if (bVerbose) printf("%8.8ld CAN Messages \r",lCanMsgs);

                    // Get the next CAN message
                    enStatus = enI106_Decode_NextCan(&suCanMsg);
                    } // end while processing CAN messages from an IRIG packet

                } // end if CAN type and channel of interest

            } while (bFALSE); // end one time loop

        // If EOF break out of main read loop
        if (enStatus == I106_EOF)
            {
            fprintf(stderr, "End of file\n");
            break;
            }

        }   /* End while */

/*
 * Print out some summaries
 */
    printf("\nTotal Message %lu\n", lMsgs);

/*
 *  Close files
 */

    enI106Ch10Close(m_iI106Handle);
    fclose(psuOutFile);

    return 0;
    }



/* ------------------------------------------------------------------------ */

void vPrintTmats(SuTmatsInfo * psuTmatsInfo, FILE * psuOutFile)
    {
    int                     iGIndex;
    int                     iRIndex;
    SuGDataSource         * psuGDataSource;
    SuRRecord             * psuRRecord;
    SuRDataSource         * psuRDataSource;

    // Print out the TMATS info
    // ------------------------

    fprintf(psuOutFile,"\n=-=-= CAN Channel Summary =-=-=\n\n");

    // G record
    fprintf(psuOutFile,"Program Name - %s\n",psuTmatsInfo->psuFirstGRecord->szProgramName);
    fprintf(psuOutFile,"\n");
    fprintf(psuOutFile,"Channel  Data Source         \n");
    fprintf(psuOutFile,"-------  --------------------\n");

    // Data sources
    psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource;
    do  {
        if (psuGDataSource == NULL) break;

        // G record data source info
        iGIndex = psuGDataSource->iIndex;

        // R record info
        psuRRecord = psuGDataSource->psuRRecord;
        do  {
            if (psuRRecord == NULL) break;
            iRIndex = psuRRecord->iIndex;

            // R record data sources
            psuRDataSource = psuRRecord->psuFirstDataSource;
            do  {
                if (psuRDataSource == NULL) 
                    break;
                if (strcasecmp(psuRDataSource->szChannelDataType,"CANIN") == 0)
                    {
                    fprintf(psuOutFile," %5s ",   psuRDataSource->szTrackNumber);
                    fprintf(psuOutFile,"  %-20s", psuRDataSource->szDataSourceID);
                    fprintf(psuOutFile,"\n");
                    }
                psuRDataSource = psuRDataSource->psuNext;
                } while (bTRUE);

            psuRRecord = psuRRecord->psuNext;
            } while (bTRUE);


        psuGDataSource = psuTmatsInfo->psuFirstGRecord->psuFirstGDataSource->psuNext;
        } while (bTRUE);

    return;
    }


/* ------------------------------------------------------------------------ */

void vUsage(void)
    {
    printf("\nIDMPCAN "MAJOR_VERSION"."MINOR_VERSION" "__DATE__" "__TIME__"\n");
    printf("Dump CAN records from a Ch 10 data file\n");
    printf("Freeware Copyright (C) 2014 Irig106.org\n\n");
    printf("Usage: idmpcan <input file> <output file> [flags]\n");
    printf("   <filename> Input/output file names        \n");
    printf("   -v         Verbose                        \n");
    printf("   -c ChNum   Channel Number (default all)   \n");
    printf("   -R         Print Relative Time Counter    \n");
    printf("   -T         Print TMATS summary and exit   \n");
    printf("                                             \n");
    printf("The output data fields are:                  \n");
    printf("  Time Chan-Subchan Data...                  \n");
    printf("                                             \n");
    }




