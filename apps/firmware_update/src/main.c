/**
********************************************************************************
\file   main.c

\brief  Main file of firmware update tool for APC/PPC2100

This file contains the main file of the openPOWERLINK firmware update tool.

\ingroup module_fwupdate_tool
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2015, Kalycito Infotech Private Ltd.
Copyright (c) 2015, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holders nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------*/

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <oplk/oplk.h>
#include <oplk/debugstr.h>

#include <system/system.h>
#include <getopt/getopt.h>
#include <console/console.h>

//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// module global vars
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// global function prototypes
//------------------------------------------------------------------------------

//============================================================================//
//            P R I V A T E   D E F I N I T I O N S                           //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define FIRMWARE_HEADER_SIZE        32

//------------------------------------------------------------------------------
// local types
//------------------------------------------------------------------------------
typedef struct
{
    char    firmwareFile[256];
    BOOL    fUpdateImage;
    BOOL    fInvalidateUpdateImage;
    BOOL    fFactoryReset;
    BOOL    fUpdateReset;
} tOptions;

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------
static int          getOptions(int argc_p, char** argv_p, tOptions* pOpts_p);
static tOplkError   invalidateImage(void);
static tOplkError   updateImage(char* pszFirmwareFile_p);
static tOplkError   writeImageToKernel(UINT8* pImage_p, UINT length_p);

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief  main function

This is the main function of the openPOWERLINK console MN demo application.

\param  argc                    Number of arguments
\param  argv                    Pointer to argument strings

\return Returns an exit code

\ingroup module_demo_mn_console
*/
//------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    tOplkError          ret;
    tOptions            opts;
    tOplkApiStackInfo   stackInfo;

    memset(&opts, 0, sizeof(tOptions));

    if (getOptions(argc, argv, &opts) == -1)
    {
        return -1;
    }

    if (system_init() != 0)
    {
        fprintf(stderr, "Error initializing system!");
        return 0;
    }

    printf("----------------------------------------------------\n");
    printf("Firmware Update application for B&R APC/PPC2100\n");
    printf("for openPOWERLINK Stack: %s\n", oplk_getVersionString());
    printf("----------------------------------------------------\n");

    ret = oplk_initialize();
    if (ret != kErrorOk)
    {
        printf("Failed to initialize openPOWERLINK (ret = 0x%X)!\n", ret);
        goto Exit;
    }

    ret = oplk_getStackInfo(&stackInfo);
    if (ret != kErrorOk)
    {
        printf("Failed to get stack information (ret = 0x%X)!\n", ret);
        goto Exit;
    }

    printf("User stack version:     0x%08X\n", stackInfo.userVersion);
    printf("User stack feature:     0x%08X\n", stackInfo.userFeature);
    printf("Kernel stack version:   0x%08X\n", stackInfo.kernelVersion);
    printf("Kernel stack feature:   0x%08X\n", stackInfo.kernelFeature);

    if (opts.fInvalidateUpdateImage)
    {
        ret = invalidateImage();
        if (ret != kErrorOk)
        {
            printf("Failed to invalidate image (ret = 0x%X)!\n", ret);
            oplk_exit();
            goto Exit;
        }

        printf("\nFirmware invalidated successfully\n");
    }

    if (opts.fUpdateImage)
    {
        ret = updateImage(opts.firmwareFile);
        if (ret != kErrorOk)
        {
            printf("Failed to update image (ret = 0x%X)!\n", ret);
            oplk_exit();
            goto Exit;
        }
    }

    if (opts.fFactoryReset || opts.fUpdateReset)
    {
        printf("\nIssue firmware reconfiguration to %s image...\n",
                opts.fFactoryReset ? "FACTORY" : "UPDATE");

        ret = oplk_serviceExecFirmwareReconfig(opts.fFactoryReset);
        if (ret != kErrorOk)
        {
            printf("Failed to execute firmware reconfiguration (ret = 0x%X)!\n", ret);
            oplk_exit();
            goto Exit;
        }

        printf("Done\n");
    }

    oplk_exit();

Exit:
    system_exit();

    return 0;
}

//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{

//------------------------------------------------------------------------------
/**
\brief  Get command line parameters

The function parses the supplied command line parameters and stores the
options at pOpts_p.

\param  argc_p                  Argument count.
\param  argc_p                  Pointer to arguments.
\param  pOpts_p                 Pointer to store options

\return The function returns the parsing status.
\retval 0           Successfully parsed
\retval -1          Parsing error
*/
//------------------------------------------------------------------------------
static int getOptions(int argc_p, char** argv_p, tOptions* pOpts_p)
{
    int opt;

    /* setup default parameters only if no parameters specified*/
    if (argc_p == 1)
    {
        strncpy(pOpts_p->firmwareFile, "image.bin", 256);
        pOpts_p->fUpdateImage = TRUE;
        pOpts_p->fUpdateReset = TRUE;
    }

    /* get command line parameters */
    while ((opt = getopt(argc_p, argv_p, "d:efu")) != -1)
    {
        switch (opt)
        {
            case 'd':
                strncpy(pOpts_p->firmwareFile, optarg, 256);
                pOpts_p->fUpdateImage = TRUE;
                break;

            case 'e':
                pOpts_p->fInvalidateUpdateImage = TRUE;
                break;

            case 'f':
                pOpts_p->fFactoryReset = TRUE;
                pOpts_p->fUpdateReset = FALSE; // falsify if also -u is given
                break;

            case 'u':
                pOpts_p->fFactoryReset = FALSE; // falsify if also -f is given
                pOpts_p->fUpdateReset = TRUE;
                break;

            default: /* '?' */
                printf("Usage: %s [COMMAND] \n"
                       "-d <UPDATE_IMAGE>: Download update image to IF card\n"
                       "-e : Invalidate the existing update image\n"
                       "-f : Reset to factory image\n"
                       "-u : Reset to update image\n",
                       argv_p[0]);
                return -1;
        }
    }

    return 0;
}

//------------------------------------------------------------------------------
/**
\brief  Invalidate the update image

The function invalidates the update image header.

\return The function returns a tOplkError code.
*/
//------------------------------------------------------------------------------
static tOplkError invalidateImage(void)
{
    tOplkError  ret = kErrorOk;
    UINT8       aInvalidHeader[FIRMWARE_HEADER_SIZE];

    memset(aInvalidHeader, 0xFF, sizeof(aInvalidHeader));

    ret = writeImageToKernel(aInvalidHeader, sizeof(aInvalidHeader));

    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Update the firmware image

The function updates the firmware image.

\param  pszFirmwareFile_p       Firmware update image file

\return The function returns a tOplkError code.
*/
//------------------------------------------------------------------------------
static tOplkError updateImage(char* pszFirmwareFile_p)
{
    tOplkError  ret = kErrorOk;
    FILE*       pFile;
    int         fileSize;
    size_t      readSize;
    UINT8*      pImage;

    pFile = fopen(pszFirmwareFile_p, "rb");
    if (pFile == NULL)
    {
        printf("Unable to open file %s\n", pszFirmwareFile_p);
        return kErrorNoResource;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);
    rewind(pFile);

    if (fileSize == 0)
    {
        printf("File is empty!\n");
        ret = kErrorNoResource;
        goto Exit;
    }

    pImage = (UINT8*)malloc(fileSize);
    if (pImage == NULL)
    {
        ret = kErrorNoResource;
        goto Exit;
    }

    readSize = fread(pImage, 1, fileSize, pFile);
    if (readSize != fileSize)
    {
        printf("Unable to read file %s\n", pszFirmwareFile_p);
        ret = kErrorNoResource;
        goto Exit;
    }

    ret = writeImageToKernel(pImage, fileSize);

Exit:
    if (pImage != NULL)
        free(pImage);

    if (pFile != NULL)
        fclose(pFile);

    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Write image to kernel stack

The function writes the given image to the kernel stack by creating chunks.

\param  pImage_p    Pointer to image to be written to kernel stack
\param  length_p    Length of the image in bytes

\return The function returns a tOplkError code.
*/
//------------------------------------------------------------------------------
static tOplkError writeImageToKernel(UINT8* pImage_p, UINT length_p)
{
    tOplkError              ret = kErrorOk;
    tOplkApiFileChunkDesc   desc;
    UINT8*                  pChunk;
    size_t                  chunkSize = oplk_serviceGetFileChunkSize();
    float                   completePercentage = 0;
    float                   completedLength;
    float                   totalLength = (float)(length_p);

    if (chunkSize == 0)
    {
        printf("No file chunk transfer support available!\n");
        return kErrorNoResource;
    }

    pChunk = (UINT8*)malloc(chunkSize);
    if (pChunk == NULL)
        return kErrorNoResource;

    memset(&desc, 0, sizeof(desc));
    desc.fFirst = TRUE;
    desc.offset = 0;

    while (length_p)
    {
        if (length_p < chunkSize)
        {
            desc.length = length_p;
            desc.fLast = TRUE;
        }
        else
            desc.length = (UINT32)chunkSize;

        memcpy(pChunk, pImage_p, desc.length);

        ret = oplk_serviceWriteFileChunk(&desc, pChunk);
        if (ret != kErrorOk)
        {
            printf("Writing file chunk failed (0x%X)!\n", ret);
            goto Exit;
        }

        desc.offset += desc.length;
        desc.fFirst = FALSE;
        pImage_p += desc.length;
        length_p -= desc.length;

        // Display progress of download
        completedLength = (float)length_p;
        completePercentage = (((totalLength - completedLength) / totalLength) * 100.0);
        printf("\rProgress [%.0f%%]", floor(completePercentage));
        fflush(stdout);
    }

Exit:
    if (pChunk != NULL)
        free(pChunk);

    return ret;
}

/// \}
