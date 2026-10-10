
#include <tier0/platform.h>

#include "EngineInterface.h"

#include <windows.h>

#include "gl/GL.h"
#include "gl/glext.h"

struct tga_header_t
{
	byte idlength;
	byte colourmaptype;
	byte datatypecode;
	byte colourmaporigin[2]; // how come you have short ints there?
	byte colourmaplength[2];
	byte colourmapdepth;
	byte x_origin[2];
	byte y_origin[2];
	byte width[2];
	byte height[2];
	byte bitsperpixel;
	byte imagedescriptor;
};

byte bLittleEndian[2] = {1, 0};
unsigned short ByteToUShort(byte* byte)
{
	if (*(short*)bLittleEndian == 1)
		return (byte[0] + (byte[1] << 8));
	else
		return (byte[1] + (byte[0] << 8));
}

bool IsPowerOfTwo(int iWidth, int iHeight)
{
	int iWidthT = iWidth;
	while (iWidthT != 1)
	{
		if ((iWidthT % 2) != 0)
			return false;
		iWidthT /= 2;
	}

	int iHeightT = iHeight;
	while (iHeightT != 1)
	{
		if ((iHeightT % 2) != 0)
			return false;
		iHeightT /= 2;
	}
	return true;
}

unsigned char* LoadTGA_Manually_because_goldsrc_is_a_fucking_cunt(char* imagepath, int& width, int& height, const int texindex)
{
	byte* pFile = (byte*)gEngfuncs.COM_LoadFile(imagepath, 5, NULL);
	// Set basic information
	tga_header_t* pHeader = (tga_header_t*)pFile;
	if (pHeader->datatypecode != 2 && pHeader->datatypecode != 10 || pHeader->bitsperpixel != 24 && pHeader->bitsperpixel != 32)
	{
		gEngfuncs.Con_Printf("Error! %s is using a non-supported format. Only 24 bit and 32 bit true color formats are supported.\n", imagepath);
		return false;
	}

	width = ByteToUShort(pHeader->width);
	height = ByteToUShort(pHeader->height);

	if (!IsPowerOfTwo(width, height))
	{
		gEngfuncs.Con_Printf("Warning! %s is not a power of two texture!\n", imagepath);
		// return false;
	}

	// Allocate data
	int iBpp = pHeader->bitsperpixel / 8;
	int iSize = width * height;
	int iImageSize = iSize * iBpp;

	byte* pOriginal = new byte[iImageSize];
	memset(pOriginal, 0, sizeof(byte) * iImageSize);

	// Load based on type
	byte* pCurrent = pFile + 18;
	if (pHeader->datatypecode == 2)
	{
		// Uncompressed TGA
		if (iBpp == 3)
		{
			for (int i = 0; i < iImageSize; i += 3)
			{
				pOriginal[i] = pCurrent[i + 2];
				pOriginal[i + 1] = pCurrent[i + 1];
				pOriginal[i + 2] = pCurrent[i];
			}
		}
		else if (iBpp == 4)
		{
			for (int i = 0; i < iImageSize; i += 4)
			{
				pOriginal[i] = pCurrent[i + 2];
				pOriginal[i + 1] = pCurrent[i + 1];
				pOriginal[i + 2] = pCurrent[i];
				pOriginal[i + 3] = pCurrent[i + 3];
			}
		}
	}
	else
	{
		// RLE Compression
		int i = 0;
		if (iBpp == 3)
		{
			while (i < iImageSize)
			{
				if (*pCurrent & 0x80)
				{
					byte bLength = *pCurrent - 127;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += iBpp)
					{
						pOriginal[i] = pCurrent[2];
						pOriginal[i + 1] = pCurrent[1];
						pOriginal[i + 2] = pCurrent[0];
					}

					pCurrent += iBpp;
				}
				else
				{
					byte bLength = *pCurrent + 1;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += iBpp, pCurrent += iBpp)
					{
						pOriginal[i] = pCurrent[2];
						pOriginal[i + 1] = pCurrent[1];
						pOriginal[i + 2] = pCurrent[0];
					}
				}
			}
		}
		else
		{
			while (i < iImageSize)
			{
				if (*pCurrent & 0x80)
				{
					byte bLength = *pCurrent - 127;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += iBpp)
					{
						pOriginal[i] = pCurrent[2];
						pOriginal[i + 1] = pCurrent[1];
						pOriginal[i + 2] = pCurrent[0];
						pOriginal[i + 3] = pCurrent[3];
					}

					pCurrent += iBpp;
				}
				else
				{
					byte bLength = *pCurrent + 1;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += iBpp, pCurrent += iBpp)
					{
						pOriginal[i] = pCurrent[2];
						pOriginal[i + 1] = pCurrent[1];
						pOriginal[i + 2] = pCurrent[0];
						pOriginal[i + 3] = pCurrent[3];
					}
				}
			}
		}
	}

	// Flip vertically
	byte* pFlipped = new byte[iImageSize];
	for (int i = 0; i < height; i++)
	{
		byte* dst = pFlipped + i * width * iBpp;
		byte* src = pOriginal + (height - i - 1) * width * iBpp;
		memcpy(dst, src, sizeof(byte) * width * iBpp);
	}

	glBindTexture(GL_TEXTURE_2D, texindex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (iBpp == 3)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pFlipped);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pFlipped);

	gEngfuncs.COM_FreeFile(pFile);
	delete[] pOriginal;
	return pFlipped;
}