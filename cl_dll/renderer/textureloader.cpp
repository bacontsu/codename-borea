/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Texture loader interface
Written by Andrew Lucas
*/

#include "hud.h"
#include "textureloader.h"
#include "bsprenderer.h"
#include "propmanager.h"
#include "PlatformHeaders.h"

#include "goldsrc_spriterenderer.h"

CTextureLoader gTextureLoader;

// taken from quake
static const struct
{
	const char* name;
	GLenum minimize, maximize;
} texModes[] = {
	//?? remove this later:
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},								  // box filter, no mipmaps
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},								  // linear filter, no mipmaps
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST}, // no (box) filter
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},	  // bilinear filter
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}, // trilinear filter
};

/*
====================
Init

====================
*/
void CTextureLoader::Init(void)
{
}

/*
====================
VidInit

====================
*/
void CTextureLoader::VidInit(void)
{
	if (m_pTextures.empty())
		return;

	for (int i = 0; i < m_pTextures.size(); i++)
		glDeleteTextures(1, &m_pTextures[i]->iIndex);

	m_pTextures.clear();

	m_pEngineTextures.clear();
}

/*
====================
Shutdown

====================
*/
void CTextureLoader::Shutdown(void)
{
	VidInit();
}

/*
====================
IsPowerOfTwo

====================
*/
bool CTextureLoader::IsPowerOfTwo(int iWidth, int iHeight)
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

cl_texture_t* CTextureLoader::LoadSprite(const char* szFile, int iFrame)
{
	for (auto texture : m_pEngineTextures)
	{
		if (!strcmp(szFile, texture->szName))
		{
			mspriteframe_t* frame = g_LegacySpriteRenderer.GetSpriteFrame(IEngineStudio.Mod_ForName(szFile, true), iFrame, 0);
			if (texture->iIndex == frame->gl_texturenum)
				return texture;
		}
	}
	model_t* sprite = IEngineStudio.Mod_ForName(szFile, true);
	mspriteframe_t* frame = g_LegacySpriteRenderer.GetSpriteFrame(sprite, iFrame, 0);

	assert(frame);

	cl_texture_t* pTexture = new cl_texture_t{};

	pTexture->iWidth = frame->width;
	pTexture->iHeight = frame->height;
	strcpy(pTexture->szName, szFile);
	pTexture->iIndex = frame->gl_texturenum;

	m_pEngineTextures.push_back(pTexture);

	return pTexture;

}

cl_texture_t* CTextureLoader::GenDummyTexture(int texIndex)
{
	cl_texture_t* pTexture = new cl_texture_t{};

	pTexture->iWidth = 8;
	pTexture->iHeight = 8;
	strcpy(pTexture->szName, "dummy");
	pTexture->iIndex = texIndex;

	m_pEngineTextures.push_back(pTexture);

	return pTexture;
}

/*
====================
LoadTexture

====================
*/
cl_texture_t* CTextureLoader::LoadTexture(const char* szFile, bool bPrompt, bool bNoMip, bool bBorder)
{
	int iType = 0;
	char szAlt[128];
	std::vector<std::byte> pFile;

	if (strlen(szFile) >= 128)
	{
		gEngfuncs.Con_Printf("Token too large on %s.\n", szFile);
		return NULL;
	}

	// Try and find a match
	cl_texture_t* pTexture = NULL;

	pTexture = HasTexture(szFile);

	if (pTexture)
	{
		// Just return regular ones if already loaded
		return pTexture;
	}

	// Some files need to be .tga
	if (!strcmp(&szFile[strlen(szFile) - 3], "dds"))
		pFile = FileSystem_LoadFileIntoBuffer(szFile, FileContentFormat::Binary);

	if (pFile.empty())
	{
		// Check for .tga then
		strcpy(szAlt, szFile);
		strcpy(&szAlt[strlen(szAlt) - 3], "tga");

		pFile = FileSystem_LoadFileIntoBuffer(szAlt, FileContentFormat::Binary);
		iType = 1;
	}

	if (pFile.empty())
	{
		if (bPrompt)
			gEngfuncs.Con_Printf("Failed to load image: %s\n", szFile);
		else
			gEngfuncs.Con_DPrintf("Failed to load image: %s\n", szFile);
		return NULL;
	}

	//
	// Allocate cache
	//
	pTexture = new cl_texture_t{};

	glGenTextures(1, &pTexture->iIndex);

	// Copy the name over
	strcpy(pTexture->szName, szFile);

	// Load DDS file
	if (iType == 0)
	{
		if (!LoadDDSFile((byte*)pFile.data(), pTexture, bNoMip))
		{
			gEngfuncs.Con_Printf("Error! Failed to load: %s.\n", szFile);

			glDeleteTextures(1, &pTexture->iIndex);
			delete pTexture;
			return NULL;
		}
	}
	else if (iType == 1)
	{
		if (!LoadTGAFile((byte*)pFile.data(), pTexture, bNoMip, bBorder))
		{
			gEngfuncs.Con_Printf("Error! Failed to load: %s.\n", szFile);

			glDeleteTextures(1, &pTexture->iIndex);
			delete pTexture;
			return NULL;
		}
	}
	m_pTextures.push_back(pTexture);

	return pTexture;
}

/*
====================
LoadTexture

====================
*/
cl_texture_t* CTextureLoader::LoadCubemapTexture(std::vector<std::string> &szFile, bool bPrompt, bool bNoMip, bool bBorder)
{
	//int iType = 0;
	//char szAlt[128];
	//byte* pFile = NULL;
	//
	//if (strlen(szFile) >= 128)
	//{
	//	gEngfuncs.Con_Printf("Token too large on %s.\n", szFile);
	//	return NULL;
	//}
	//
	//// Try and find a match
	//cl_texture_t* pTexture = NULL;
	//
	//pTexture = HasTexture(szFile);
	//
	//if (pTexture)
	//{
	//	// Just return regular ones if already loaded
	//	return pTexture;
	//}
	//
	//// Some files need to be .tga
	//if (!strcmp(&szFile[strlen(szFile) - 3], "dds"))
	//	pFile = (byte*)gEngfuncs.COM_LoadFile(szFile, 5, NULL);
	//
	//if (!pFile)
	//{
	//	// Check for .tga then
	//	strcpy(szAlt, szFile);
	//	strcpy(&szAlt[strlen(szAlt) - 3], "tga");
	//
	//	pFile = (byte*)gEngfuncs.COM_LoadFile(szAlt, 5, NULL);
	//	iType = 1;
	//}
	//
	//if (!pFile)
	//{
	//	if (bPrompt)
	//		gEngfuncs.Con_Printf("Failed to load image: %s\n", szFile);
	//	else
	//		gEngfuncs.Con_DPrintf("Failed to load image: %s\n", szFile);
	//	return NULL;
	//}
	//
	////
	//// Allocate cache
	////
	//pTexture = new cl_texture_t{};
	//
	//glGenTextures(1, &pTexture->iIndex);
	//
	//// Copy the name over
	//strcpy(pTexture->szName, szFile);
	//
	//// Load DDS file
	//if (iType == 0)
	//{
	//	if (!LoadDDSFile(pFile, pTexture, bNoMip))
	//	{
	//		gEngfuncs.Con_Printf("Error! Failed to load: %s.\n", szFile);
	//		gEngfuncs.COM_FreeFile(pFile);
	//
	//		glDeleteTextures(1, &pTexture->iIndex);
	//		delete pTexture;
	//		return NULL;
	//	}
	//}
	//else if (iType == 1)
	//{
	//	if (!LoadTGAFile(pFile, pTexture, bNoMip, bBorder))
	//	{
	//		gEngfuncs.Con_Printf("Error! Failed to load: %s.\n", szFile);
	//		gEngfuncs.COM_FreeFile(pFile);
	//
	//		glDeleteTextures(1, &pTexture->iIndex);
	//		delete pTexture;
	//		return NULL;
	//	}
	//}
	//m_pTextures.push_back(pTexture);
	//
	//gEngfuncs.COM_FreeFile(pFile);
	//return pTexture;
	return nullptr;
}

/*
====================
LoadTGAFile

====================
*/
bool CTextureLoader::LoadTGAFile(byte* pFile, cl_texture_t* pTexture, bool bNoMip, bool bBorder)
{
	// Set basic information
	tga_header_t* pHeader = (tga_header_t*)pFile;
	if (pHeader->datatypecode != 2 && pHeader->datatypecode != 10 || pHeader->bitsperpixel != 24 && pHeader->bitsperpixel != 32)
	{
		gEngfuncs.Con_Printf("Error! %s is using a non-supported format. Only 24 bit and 32 bit true color formats are supported.\n", pTexture->szName);
		return false;
	}

	pTexture->iWidth = ByteToUShort(pHeader->width);
	pTexture->iHeight = ByteToUShort(pHeader->height);

	if (!IsPowerOfTwo(pTexture->iWidth, pTexture->iHeight))
	{
		gEngfuncs.Con_Printf("Error! %s is not a power of two texture!\n", pTexture->szName);
		return false;
	}

	// Allocate data
	pTexture->iBpp = pHeader->bitsperpixel / 8;
	int iSize = pTexture->iWidth * pTexture->iHeight;
	int iImageSize = iSize * pTexture->iBpp;

	byte* pOriginal = new byte[iImageSize];
	memset(pOriginal, 0, sizeof(byte) * iImageSize);

	// Load based on type
	byte* pCurrent = pFile + 18;
	if (pHeader->datatypecode == 2)
	{
		// Uncompressed TGA
		if (pTexture->iBpp == 3)
		{
			for (int i = 0; i < iImageSize; i += 3)
			{
				pOriginal[i] = pCurrent[i + 2];
				pOriginal[i + 1] = pCurrent[i + 1];
				pOriginal[i + 2] = pCurrent[i];
			}
		}
		else if (pTexture->iBpp == 4)
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
		if (pTexture->iBpp == 3)
		{
			while (i < iImageSize)
			{
				if (*pCurrent & 0x80)
				{
					byte bLength = *pCurrent - 127;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += pTexture->iBpp)
					{
						pOriginal[i] = pCurrent[2];
						pOriginal[i + 1] = pCurrent[1];
						pOriginal[i + 2] = pCurrent[0];
					}

					pCurrent += pTexture->iBpp;
				}
				else
				{
					byte bLength = *pCurrent + 1;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += pTexture->iBpp, pCurrent += pTexture->iBpp)
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

					for (int j = 0; j < bLength; j++, i += pTexture->iBpp)
					{
						pOriginal[i] = pCurrent[2];
						pOriginal[i + 1] = pCurrent[1];
						pOriginal[i + 2] = pCurrent[0];
						pOriginal[i + 3] = pCurrent[3];
					}

					pCurrent += pTexture->iBpp;
				}
				else
				{
					byte bLength = *pCurrent + 1;
					pCurrent++;

					for (int j = 0; j < bLength; j++, i += pTexture->iBpp, pCurrent += pTexture->iBpp)
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
	for (int i = 0; i < pTexture->iHeight; i++)
	{
		GLubyte* dst = pFlipped + i * pTexture->iWidth * pTexture->iBpp;
		GLubyte* src = pOriginal + (pTexture->iHeight - i - 1) * pTexture->iWidth * pTexture->iBpp;
		memcpy(dst, src, sizeof(GLubyte) * pTexture->iWidth * pTexture->iBpp);
	}

	// Add border if asked to
	if (bBorder)
	{
		byte* pCurrent = pFlipped;
		for (int i = 0; i < pTexture->iHeight; i++)
		{
			for (int j = 0; j < pTexture->iWidth; j++)
			{
				if (i == 0 || i == (pTexture->iHeight - 1) || j == 0 || j == (pTexture->iWidth - 1))
				{
					pCurrent[0] = 0;
					pCurrent[1] = 0;
					pCurrent[2] = 0;
				}

				if (pTexture->iBpp == 3)
					pCurrent += 3;
				else
					pCurrent += 4;
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, pTexture->iIndex);

	if (bNoMip)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (pTexture->iBpp == 3)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, pTexture->iWidth, pTexture->iHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pFlipped);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pTexture->iWidth, pTexture->iHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pFlipped);

	delete[] pOriginal;
	delete[] pFlipped;
	return true;
}

/*
====================
LoadDDSFile

====================
*/
bool CTextureLoader::LoadDDSFile(byte* pFile, cl_texture_t* pTexture, bool bNoMip)
{
	dds_header_t* pHeader = (dds_header_t*)pFile;
	unsigned int iFlags = ByteToUInt(pHeader->bFlags);
	unsigned int iMagic = ByteToUInt(pHeader->bMagic);
	unsigned int iFourCC = ByteToUInt(pHeader->bPFFourCC);
	unsigned int iPFFlags = ByteToUInt(pHeader->bPFFlags);
	unsigned int iLinSize = ByteToUInt(pHeader->bPitchOrLinearSize);
	unsigned int iSize = ByteToUInt(pHeader->bSize);

	pTexture->iWidth = ByteToUInt(pHeader->bWidth);
	pTexture->iHeight = ByteToUInt(pHeader->bHeight);

	if (!IsPowerOfTwo(pTexture->iWidth, pTexture->iHeight))
	{
		gEngfuncs.Con_Printf("Error! %s is not a power of two texture!\n", pTexture->szName);
		return false;
	}

	if (iMagic != DDS_MAGIC)
		return false; // Not DDS file

	if (iSize != 124)
		return false; // Not correct size

	if (!(iFlags & DDSD_PIXELFORMAT))
		return false; // Not correct format

	if (!(iFlags & DDSD_CAPS))
		return false; // Not correct format

	if (!(iPFFlags & DDPF_FOURCC))
		return false; // Not correct type

	if (iFourCC != D3DFMT_DXT1 && iFourCC != D3DFMT_DXT5)
	{
		gEngfuncs.Con_Printf("Error! Incorrect compression on: %s! Only DXT1 and DXT5 are supported!\n", pTexture->szName);
		return false; // Not correct compression
	}

	// Copy data over
	byte* pData = new byte[iLinSize];
	memcpy(pData, (pFile + 128), iLinSize);

	glBindTexture(GL_TEXTURE_2D, pTexture->iIndex);
	if (bNoMip)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Upload to OpenGL
	glCompressedTexImage2D(GL_TEXTURE_2D, 0, (iFourCC == D3DFMT_DXT1) ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, pTexture->iWidth, pTexture->iHeight, 0, iLinSize, pData);

	delete[] pData;
	return true;
}

/*
====================
HasTexture

====================
*/
cl_texture_t* CTextureLoader::HasTexture(const char* szFile)
{
	for (int i = 0; i < m_pTextures.size(); i++)
	{
		if (!strcmp(m_pTextures[i]->szName, szFile))
			return m_pTextures[i];
	}
	return NULL;
}

/*
====================
LoadWADFiles

====================
*/
void CTextureLoader::LoadWADFiles(void)
{
	char szWAD[64];
	char szFile[32];

	char* pWAD = gPropManager.ValueForKey(&gPropManager.m_pBSPEntities[0], "wad");
	if (!pWAD)
		return;

	int iLength = strlen(pWAD);
	int iCur = 0;
	;
	while (1)
	{
		if (iCur >= iLength)
			return;

		int iLen = 0;
		while (1)
		{
			if (pWAD[iCur] == ';' || iCur >= iLength)
			{
				szWAD[iLen] = 0;
				iLen++;
				iCur++;
				break;
			}

			szWAD[iLen] = pWAD[iCur];
			iCur++;
			iLen++;
		}

		gEngfuncs.Con_Printf(szWAD);

		FilenameFromPath(szWAD, szFile);
		if (!stricmp(szFile, "decals"))
			continue; // we load decals separately

		if (!strstr(szFile, ".wad"))
			strcat(szFile, ".wad");

		int iSize = 0;
		std::vector<std::byte> pFile = FileSystem_LoadFileIntoBuffer(szFile, FileContentFormat::Binary);
		if (pFile.empty())
			continue;

		wadinfo_t* pInfo = (wadinfo_t*)pFile.data();
		if (strncmp("WAD3", pInfo->identification, 4))
		{
			continue;
		}

		wadfile_t* pWADFile = &m_pWADFiles[m_iNumWADFiles];
		m_iNumWADFiles++;

		strcpy(pWADFile->wadname, szFile);
		pWADFile->wadfile = pFile;
		pWADFile->info = (wadinfo_t*)pWADFile->wadfile.data();

		pWADFile->lumps = new lumpinfo_t[pWADFile->info->numlumps];
		memcpy(pWADFile->lumps, (pWADFile->wadfile.data() + pWADFile->info->infotableofs), sizeof(lumpinfo_t) * pWADFile->info->numlumps);
		pWADFile->numlumps = pWADFile->info->numlumps;
	}
}

/*
====================
FreeWADFiles

====================
*/
void CTextureLoader::FreeWADFiles(void)
{
	if (!m_iNumWADFiles)
		return;

	for (int i = 0; i < m_iNumWADFiles; i++)
	{
		delete[] m_pWADFiles[i].lumps;
		m_pWADFiles[i].info = nullptr;
		m_pWADFiles[i].lumps = nullptr;
		m_pWADFiles[i].numlumps = 0;
		memset(m_pWADFiles[i].wadname, 0, 64);

		m_pWADFiles[i].wadfile.clear();
	}

	m_iNumWADFiles = 0;
}

/*
====================
LoadWADTexture

====================
*/
cl_texture_t* CTextureLoader::LoadWADTexture(char* szTexture)
{
	char szName[32];
	cl_texture_t* pTexture = NULL;

	for (int i = 0; i < m_iNumWADFiles; i++)
	{
		byte* pFile = (byte*)m_pWADFiles[i].wadfile.data();
		wadinfo_t* pInfo = m_pWADFiles[i].info;
		for (int j = 0; j < pInfo->numlumps; j++)
		{
			lumpinfo_t* pLump = &m_pWADFiles[i].lumps[j];
			if (pLump->type != 0 && !(pLump->type & 0x43))
				continue;

			strcpy(szName, pLump->name);
			strLower(szName);

			if (!strcmp(szName, szTexture))
			{
				pTexture = new cl_texture_t{};

				// Fill in data
				strcpy(pTexture->szName, szTexture);
				pTexture->iWidth = ByteToUInt(pFile + pLump->filepos + 16);
				pTexture->iHeight = ByteToUInt(pFile + pLump->filepos + 20);
				pTexture->iBpp = 4;

				// Get offsets
				int iIndexOffset = ByteToUInt(pFile + pLump->filepos + 24);
				int iMip3Offset = ByteToUInt(pFile + pLump->filepos + 36);

				byte* pPalette;
				if (pLump->type & 0x43)
					pPalette = pFile + pLump->filepos + iMip3Offset + ((pTexture->iWidth / 8) * (pTexture->iHeight / 8)) + 2;
				else
					pPalette = pFile + pLump->filepos + iIndexOffset + (pTexture->iWidth * pTexture->iHeight) + 2;

				// if (iAltIndex && gBSPRenderer.m_pCvarFixTextCorruption->value == 0)
				//	pTexture->iIndex = iAltIndex;

				byte* pPixels = pFile + pLump->filepos + iIndexOffset;
				LoadPallettedTexture(pPixels, pPalette, pTexture);
				m_pTextures.push_back(pTexture);
				return pTexture;
			}
		}
	}

	return NULL;
}

/*
====================
LoadPallettedTexture

====================
*/
void CTextureLoader::LoadPallettedTexture(byte* data, byte* pal, cl_texture_t* pTexture, bool isdecal)
{
	int row1[1024], row2[1024], col1[1024], col2[1024];
	byte *pix1, *pix2, *pix3, *pix4;
	byte alpha1, alpha2, alpha3, alpha4;

	// convert texture to power of 2
	int outwidth;
	for (outwidth = 1; outwidth < pTexture->iWidth; outwidth <<= 1)
		;
	if (outwidth > 1024)
		outwidth = 1024;

	int outheight;
	for (outheight = 1; outheight < pTexture->iHeight; outheight <<= 1)
		;
	if (outheight > 1024)
		outheight = 1024;

	byte *out, *tex;
	tex = out = new byte[outwidth * outheight * 4];

	for (int i = 0; i < outwidth; i++)
	{
		col1[i] = (int)((i + 0.25) * (pTexture->iWidth / (float)outwidth));
		col2[i] = (int)((i + 0.75) * (pTexture->iWidth / (float)outwidth));
	}

	for (int i = 0; i < outheight; i++)
	{
		row1[i] = (int)((i + 0.25) * (pTexture->iHeight / (float)outheight)) * pTexture->iWidth;
		row2[i] = (int)((i + 0.75) * (pTexture->iHeight / (float)outheight)) * pTexture->iWidth;
	}

	if (isdecal)
	{
		for (int i = 0; i < outheight; i++)
		{
			for (int j = 0; j < outwidth; j++, out += 4)
			{
				byte index = data[row1[i] + col1[j]];

				byte* pix = &pal[index * 3];
				byte r = pix[0], g = pix[1], b = pix[2];

				float luminance = (0.30f * r + 0.59f * g + 0.11f * b);

				byte alpha = 255 - static_cast<byte>(luminance);
				pix = &pal[255 * 3];

				out[0] = pix[0];
				out[1] = pix[1];
				out[2] = pix[2];
				out[3] = alpha;
			}
		}
	}
	else
	{
		for (int i = 0; i < outheight; i++)
		{
			for (int j = 0; j < outwidth; j++, out += 4)
			{
				pix1 = &pal[data[row1[i] + col1[j]] * 3];
				pix2 = &pal[data[row1[i] + col2[j]] * 3];
				pix3 = &pal[data[row2[i] + col1[j]] * 3];
				pix4 = &pal[data[row2[i] + col2[j]] * 3];
				alpha1 = 0xFF;
				alpha2 = 0xFF;
				alpha3 = 0xFF;
				alpha4 = 0xFF;

				if (pTexture->szName[0] == '{')
				{
					if (data[row1[i] + col1[j]] == 0xFF)
					{
						pix1[0] = 0;
						pix1[1] = 0;
						pix1[2] = 0;
						alpha1 = 0;
					}

					if (data[row1[i] + col2[j]] == 0xFF)
					{
						pix2[0] = 0;
						pix2[1] = 0;
						pix2[2] = 0;
						alpha2 = 0;
					}

					if (data[row2[i] + col1[j]] == 0xFF)
					{
						pix3[0] = 0;
						pix3[1] = 0;
						pix3[2] = 0;
						alpha3 = 0;
					}

					if (data[row2[i] + col2[j]] == 0xFF)
					{
						pix4[0] = 0;
						pix4[1] = 0;
						pix4[2] = 0;
						alpha4 = 0;
					}
				}

				out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
				out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
				out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;

				out[3] = (alpha1 + alpha2 + alpha3 + alpha4) >> 2;
			}
		}
	}

	glGenTextures(1, &pTexture->iIndex);

	glBindTexture(GL_TEXTURE_2D, pTexture->iIndex);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	const char* texturemode = gEngfuncs.pfnGetCvarString("gl_texturemode");
	for (int i = 0; i < sizeof(texModes) / sizeof(texModes[0]); i++)
	{
		if (stricmp(texModes[i].name, texturemode))
			continue;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texModes[i].minimize);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texModes[i].maximize);
		break;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, outwidth, outheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);

	delete[] tex;
}

/*
====================
LoadTextureScript

====================
*/
void CTextureLoader::LoadTextureScript(void)
{
	int iFlags = 0;
	char szFlag[32];
	char szModel[32];
	char szTexture[32];

	// Clear previous list
	if (!m_pTextureEntries.empty())
	{
		m_pTextureEntries.clear();
	}

	std::vector<std::byte> bufferdata = FileSystem_LoadFileIntoBuffer("gfx/textures/texture_flags.txt", FileContentFormat::Text);
	int iSize = bufferdata.size();

	if (bufferdata.empty())
	{
		gEngfuncs.Con_Printf("Could not load gfx/textures/texture_flags.txt!\n");
		return;
	}

	char* pFile = (char*)bufferdata.data();

	if (!pFile)
	{
		gEngfuncs.Con_Printf("Could not load gfx/textures/texture_flags.txt!\n");
		return;
	}

	int i = NULL;
	while (1)
	{
		// Reset
		iFlags = 0;

		// Reached EOF
		if (i >= iSize)
			break;

		// Skip to next token
		while (1)
		{
			if (i >= iSize)
				break;

			if (pFile[i] != ' ' && pFile[i] != '\n' && pFile[i] != '\r')
				break;

			i++;
		}

		if (i >= iSize)
			break;

		// Read token in
		int j = NULL;
		while (1)
		{
			if (i >= iSize)
				break;

			if (pFile[i] == ' ' || pFile[i] == '\n' || pFile[i] == '\r')
				break;

			szModel[j] = pFile[i];
			j++;
			i++;
		}

		// Terminator
		szModel[j] = 0;

		if (i >= iSize)
			break;

		// Skip to next token
		while (1)
		{
			if (i >= iSize)
				break;

			if (pFile[i] != ' ' && pFile[i] != '\n' && pFile[i] != '\r')
				break;

			i++;
		}

		if (i >= iSize)
			break;

		// Read token in
		j = NULL;
		while (1)
		{
			if (i >= iSize)
				break;

			if (pFile[i] == ' ' || pFile[i] == '\n' || pFile[i] == '\r')
				break;

			szTexture[j] = pFile[i];
			j++;
			i++;
		}

		// Terminator
		szTexture[j] = 0;

		if (i >= iSize)
			break;

		while (1)
		{
			// Skip to next token
			while (1)
			{
				if (i >= iSize)
					break;

				if (pFile[i] != ' ' && pFile[i] != '\n' && pFile[i] != '\r')
					break;

				i++;
			}

			if (i >= iSize)
				break;

			// Read token in
			j = NULL;
			while (1)
			{
				if (i >= iSize)
					break;

				if (pFile[i] == ' ' || pFile[i] == '\n' || pFile[i] == '\r')
					break;

				szFlag[j] = pFile[i];
				j++;
				i++;
			}

			// Terminator
			szFlag[j] = 0;
			strLower(szFlag);

			// Only this flag for now
			if (!strcmp(szFlag, "alternate"))
				iFlags |= TEXFLAG_ALTERNATE;
			else if (!strcmp(szFlag, "fullbright"))
				iFlags |= TEXFLAG_FULLBRIGHT;
			else if (!strcmp(szFlag, "none"))
				iFlags |= TEXFLAG_NONE;
			else if (!strcmp(szFlag, "nomipmap"))
				iFlags |= TEXFLAG_NOMIPMAP;
			else if (!strcmp(szFlag, "eraseflags"))
				iFlags |= TEXFLAG_ERASE;

			// See if there's anything else ahead
			while (1)
			{
				if (i >= iSize)
					break;

				if (pFile[i] == ' ' || pFile[i] == '\n' || pFile[i] == '\r')
					break;
			}

			if (pFile[i] == '\n' || pFile[i] == '\r' || i >= iSize)
				break; // End of entry
		}

		if (iFlags)
		{
			texentry_t* pEntry = new texentry_t{};

			strcpy(pEntry->szModel, szModel);
			strLower(pEntry->szModel);
			strcpy(pEntry->szTexture, szTexture);
			strLower(pEntry->szTexture);
			pEntry->iFlags = iFlags;
			m_pTextureEntries.push_back(pEntry);
		}
	}
}

/*
====================
TextureHasFlag

====================
*/
bool CTextureLoader::TextureHasFlag(const char* szModel, char* szTexture, int iFlag)
{
	if (m_pTextureEntries.empty())
		return false;

	for (int i = 0; i < m_pTextureEntries.size(); i++)
	{
		if (!strcmp(m_pTextureEntries[i]->szModel, szModel) && !strcmp(m_pTextureEntries[i]->szTexture, szTexture) && m_pTextureEntries[i]->iFlags & iFlag)
			return true;
	}

	return false;
}

/*
====================
WriteTGA

====================
*/
void CTextureLoader::WriteTGA(byte* pixels, int bpp, int width, int height, char* szpath)
{
	int iSize = width * height * bpp;
	byte* pBuf = new byte[(iSize + 18)];
	memset(pBuf, 0, sizeof(byte) * (iSize + 18));

	tga_header_t* pHeader = (tga_header_t*)pBuf;
	pHeader->datatypecode = 2;
	pHeader->bitsperpixel = bpp * 8;
	pHeader->width[0] = (width & 0xFF);
	pHeader->width[1] = ((width >> 8) & 0xFF);
	pHeader->height[0] = (height & 0xFF);
	pHeader->height[1] = ((height >> 8) & 0xFF);

	for (int i = 0; i < height; i++)
	{
		GLubyte* dst = pBuf + 18 + i * width * bpp;
		GLubyte* src = pixels + (height - i - 1) * width * bpp;
		memcpy(dst, src, sizeof(byte) * width * bpp);

		if (bpp == 4)
		{
			for (int j = 0; j < width * bpp; j += bpp)
			{
				dst[j] = src[j + 2];
				dst[j + 1] = src[j + 1];
				dst[j + 2] = src[j];
				dst[j + 3] = src[j + 3];
			}
		}
		else
		{
			for (int j = 0; j < width * bpp; j += bpp)
			{
				dst[j] = src[j + 2];
				dst[j + 1] = src[j + 1];
				dst[j + 2] = src[j];
			}
		}
	}

	char szPath[64];
	sprintf(szPath, "%s/imagedump/%s.tga", gEngfuncs.pfnGetGameDirectory(), szpath);

	FILE* pFile = fopen(szPath, "wb");
	if (!pFile)
	{
		delete[] pBuf;
		return;
	}

	fwrite(pBuf, sizeof(byte), (iSize + 18), pFile);
	fclose(pFile);

	// Free memory
	delete[] pBuf;
}

/*
====================
LoadTGAFile

====================
*/
byte* CTextureLoader::LoadTGAFileRaw(const char* filename, int &width, int &height, int& bitsperpixel, bool bBorder)
{
	std::vector<std::byte> pFile = FileSystem_LoadFileIntoBuffer(filename, FileContentFormat::Binary);

	if (pFile.empty())
		return nullptr;

	// Set basic information
	tga_header_t* pHeader = (tga_header_t*)pFile.data();
	if (pHeader->datatypecode != 2 && pHeader->datatypecode != 10 || pHeader->bitsperpixel != 24 && pHeader->bitsperpixel != 32)
	{
		gEngfuncs.Con_Printf("Error! %s is using a non-supported format. Only 24 bit and 32 bit true color formats are supported.\n", filename);
		return false;
	}

	auto iWidth = width = ByteToUShort(pHeader->width);
	auto iHeight = height  = ByteToUShort(pHeader->height);

	if (!IsPowerOfTwo(width, height))
	{
		gEngfuncs.Con_Printf("Error! %s is not a power of two texture!\n", filename);
		return false;
	}

	// Allocate data
	auto iBpp = bitsperpixel = pHeader->bitsperpixel / 8;
	int iSize = iWidth * iHeight;
	int iImageSize = iSize * iBpp;

	byte* pOriginal = new byte[iImageSize];
	memset(pOriginal, 0, sizeof(byte) * iImageSize);

	// Load based on type
	byte* pCurrent = (byte*)(pFile.data() + sizeof(tga_header_t));
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
	for (int i = 0; i < iHeight; i++)
	{
		GLubyte* dst = pFlipped + i * iWidth * iBpp;
		GLubyte* src = pOriginal + (iHeight - i - 1) * iWidth * iBpp;
		memcpy(dst, src, sizeof(GLubyte) * iWidth * iBpp);
	}

	// Add border if asked to
	if (bBorder)
	{
		byte* pCurrent = pFlipped;
		for (int i = 0; i < iHeight; i++)
		{
			for (int j = 0; j < iWidth; j++)
			{
				if (i == 0 || i == (iHeight - 1) || j == 0 || j == (iWidth - 1))
				{
					pCurrent[0] = 0;
					pCurrent[1] = 0;
					pCurrent[2] = 0;
				}

				if (iBpp == 3)
					pCurrent += 3;
				else
					pCurrent += 4;
			}
		}
	}


	delete[] pFlipped;

	return pOriginal;
}