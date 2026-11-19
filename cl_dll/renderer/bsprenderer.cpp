/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

BSP Renderer
Original code by Buzer and Id Software
Extended and/or recoded by Andrew Lucas
*/

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "studio.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pm_movevars.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "propmanager.h"
#include "bsprenderer.h"
#include "particle_engine.h"
#include "watershader.h"
#include "mirrormanager.h"
#include "goldsrc_spriterenderer.h"

#include "opengl_utils/GL_FBO.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_ShadowMap.h"
#include "opengl_utils/GL_VertexArrayObject.h"
#include "opengl_utils/GL_Mesh.h"

#include "r_efx.h"

#include "StudioModelRenderer.h"
#include "BSPModel_Gen.h"

// using namespace std;

CBSPRenderer gBSPRenderer;

cl_texture_t* scopetexture = nullptr;

extern "C" {
#include "pm_shared.h"
}

extern int r_visframecount;
extern clientmleaf_t* r_oldviewleaf;


//===========================================
// OPENGL START
//
//===========================================

static GLint multidraw_startverts[65536];
static GLint multidraw_numverts[65536];
static GLint num_multidraws;

enum worldshader_uniforms
{
	world_projectionmatrix = 0,
	world_viewmatrix,
	world_modelmatrix,

	world_spotlight_texturematrix,

	world_spotlight,
	world_pointlight,
	world_shadow,
	world_onlyshadow,

	world_sundir,

	world_waterpolys,
	world_scrollingpolys,
	world_specular,
	world_fltime,

	world_alphatest,

	world_renderamt,
	world_rendercolor,

	world_light_pos,
	world_light_color,//red green blue
	world_light_radius,
	world_sunshadow_fadedist,
	world_sunshadow_strength,
	world_renderorigin,
	world_renderforward,
	world_renderright,

	world_lightmap_pass,
	world_texture_pass,

	world_fog_active,
	world_fogcolor,
	world_fogstart,
	world_fogend,

	world_lightgamma,
	world_texgamma,

	world_wireframe,

	world_clipplane,

	world_shaderlocs_size, //must be last
}; static GLuint eWorldShader_locs[world_shaderlocs_size];

enum worldshadersolid_uniforms
{
	worldsolid_projviewmatrix = 0,
	worldsolid_modelmatrix,

	worldsolid_alphatest,

	worldsolid_light_pos,

	worldsolid_shaderlocs_size, //must be last
}; static GLuint eWorldSolidShader_locs[worldsolid_shaderlocs_size];

enum decalshader_uniforms
{
	decal_projviewmatrix = 0,

	decal_wireframe,

	decal_shaderlocs_size,
}; static GLuint eDecalShader_locs[decal_shaderlocs_size];

enum skyboxshader_uniforms
{
	skybox_projviewmatrix = 0,

	skybox_skyfog,
	skybox_fogcolor,

	skybox_shaderlocs_size,
}; static GLuint eSimpleSkyboxShader_locs[skybox_shaderlocs_size];

#include "glshaders/bsp_glsl.h"
#include "glshaders/shadow/bsp_solid_glsl.h"

#include "glshaders/decal_glsl.h"

#include "glshaders/skybox_glsl.h"

#include "glshaders/gaussianblur_glsl.h"
#include "glshaders/blacknwhite_glsl.h"

static GL_ShaderProgram s_BSPShader(glsl330_world_vp, glsl330_world_fp);
static GL_ShaderProgram s_BSPSolidShader(glsl330_worldsolid_vp, glsl330_worldsolid_fp);
static GL_ShaderProgram s_DecalShader(glsl_decal_vp, glsl_decal_fp);
static GL_ShaderProgram s_SkyboxShader(glsl_skybox_vp, glsl_skybox_fp);

GL_ShaderProgram CBSPRenderer::m_FilterShader(glsl_gaussianblur_vp, glsl_gaussianblur_fp);
GL_ShaderProgram CBSPRenderer::m_BlacknwhiteShader(glsl_blacknwhite_vp, glsl_blacknwhite_fp);

static GL_Mesh* s_pSkyboxMesh;
static GL_Mesh* s_pDecalMeshBuffer;


GL_BEGIN_ATTRIBLIST(brushvertex_t)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::VertexPos, 3, GL_FLOAT, pos)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::Normal, 3, GL_FLOAT, normal)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::Specular_TexCoord, 2, GL_FLOAT, speculartexcoord)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::LightMap_TexCoord, 2, GL_FLOAT, lightmaptexcoord)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::TexCoord, 2, GL_FLOAT, texcoord)
GL_END_ATTRIBLIST()

GL_BEGIN_ATTRIBLIST(Default3DTextureVert_t)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::VertexPos, 3, GL_FLOAT, pos)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::TexCoord, 2, GL_FLOAT, texcoord)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::LightMap_TexCoord, 2, GL_FLOAT, lmcoord)
GL_END_ATTRIBLIST()



//===========================================
// OPENGL END
//
//===========================================



/*
====================
Shutdown

====================
*/
void CBSPRenderer::Shutdown(void)
{
	FreeBuffer();

	// Clear previous
	if (m_iNumSurfaces)
	{
		delete[] m_pSurfaces;
		m_pSurfaces = nullptr;
		m_iNumSurfaces = NULL;
	}

	DeleteDecals();
}

/*
====================
Init

====================
*/
void CBSPRenderer::Init(void)
{
	//
	// Check extensions
	//

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &m_iTUSupport);
	if (m_iTUSupport < 4)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: Your hardware does not support enough multitexture units!\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		gEngfuncs.pfnClientCmd("quit\n");
	}

	const char* glVersion = (const char*)glGetString(GL_VERSION);
	const char* glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

	int glMajor = 0, glMinor = 0;
	int glslMajor = 0, glslMinor = 0;
	if (glVersion)
	{
		sscanf(glVersion, "%d.%d", &glMajor, &glMinor);
	}
	if (glslVersion)
	{
		sscanf(glslVersion, "%d.%d", &glslMajor, &glslMinor);
	}

	if (glslMajor < 1 || glslMinor < 2)
	{
		MessageBox(NULL, "FATAL ERROR: Your graphics driver or this version of goldsrc does not support glsl shader version 1.20.\n"
						 "Please get in contact with the developer of the renderer to assist with the issue."
						 "\nPress Ok to quit the game.\n",
			"ERROR", MB_OK);
	}
	else if (glMajor < 4 || (glMajor >= 4 && glMinor < 4))
	{
		// because of some functions that only exist after opengl 4.4 (mainly glBufferStorage.)
		MessageBox(NULL, "ERROR: this version of goldsrc uses a older version of opengl (minimum supported : 4.4)\n"
						 "Please get in contact with the developer of the renderer to assist with the issue."
						 "\nPress Ok to quit the game.\n",
			"ERROR", MB_OK);
	}

	memset(m_pRenderEntities, 0, sizeof(m_pRenderEntities));

	glGenTextures(1, &m_iEngineLightmapIndex);


	AddLightStyle(0, "m");	// 0 normal
	AddLightStyle(1, "mmnmmommommnonmmonqnmmo");	// 1 FLICKER (first variety)
	AddLightStyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");	// 2 SLOW STRONG PULSE
	AddLightStyle(3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");	// 3 CANDLE (first variety)
	AddLightStyle(4, "mamamamamama");	// 4 FAST STROBE
	AddLightStyle(5, "jklmnopqrstuvwxyzyxwvutsrqponmlkj");	// 5 GENTLE PULSE 1
	AddLightStyle(6, "nmonqnmomnmomomno");	// 6 FLICKER (second variety)
	AddLightStyle(7, "mmmaaaabcdefgmmmmaaaammmaamm");	// 7 CANDLE (second variety)
	AddLightStyle(8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");	// 8 CANDLE (third variety)
	AddLightStyle(9, "aaaaaaaazzzzzzzz");	// 9 SLOW STROBE (fourth variety)
	AddLightStyle(10, "mmamammmmammamamaaamammma"); // 10 FLUORESCENT FLICKER
	AddLightStyle(11, "abcdefghijklmnopqrrqponmlkjihgfedcba");	// 11 SLOW PULSE NOT FADE TO BLACK
	AddLightStyle(12, "mmnnmmnnnmmnn"); 	// 12 UNDERWATER LIGHT MUTATION

	m_iFrameCount = 0;


	gEngfuncs.pfnAddCommand("te_dump", RenderersDumpInfo);

	m_pCvarDrawWorld = CVAR_CREATE("r_drawbsp", "1", FCVAR_ARCHIVE);
	m_pCvarLightStyles = CVAR_CREATE("r_lightstyles", "1", FCVAR_ARCHIVE);
	m_pCvarSpeeds = CVAR_CREATE("r_trinity_speeds", "0", 0);
	m_pCvarWireFrame = CVAR_CREATE("r_wireframe", "0", 0);
	m_pCvarDynamic = CVAR_CREATE("r_dynlights", "1", FCVAR_ARCHIVE);
	// if off, then use blobby shadows. (for now theres no blobby shadows)
	m_pCvarShadows = CVAR_CREATE("r_shadowrendertotexture", "1", FCVAR_ARCHIVE);
	m_pCvarFlashLightDepthRes = CVAR_CREATE("r_flashlightdepthres", "512", FCVAR_ARCHIVE);
	m_pCvarLightmapDebug = CVAR_CREATE("r_lightmap_debug", "0", 0);
	m_pCvar3DSkybox = CVAR_CREATE("r_3dsky", "1", 0);
	m_pCvarSunShadowsQuality = CVAR_CREATE("r_sunshadows_quality", "1", FCVAR_ARCHIVE); //brushes have some problems with sun shadows currently so make them disableable (yes it seems like thats a real word)
	m_pCvarBlurShadows = CVAR_CREATE("r_blur_shadows", "1", FCVAR_ARCHIVE);

	m_pCvarBlacknwhite = CVAR_CREATE("r_filter_blacknwhite", "0", FCVAR_ARCHIVE);
	m_pCvarFilmGrainStrength = CVAR_CREATE("r_filter_filmgrain_strength", "15", FCVAR_ARCHIVE);


	lightgamma = gEngfuncs.pfnGetCvarPointer("lightgamma");
	texgamma = gEngfuncs.pfnGetCvarPointer("texgamma");
	r_fullbright = gEngfuncs.pfnGetCvarPointer("r_fullbright");
	gl_fog = gEngfuncs.pfnGetCvarPointer("gl_fog");
#ifndef HL25_UPDATE
	gl_widescreen_yfov = CVAR_CREATE("gl_widescreen_yfov", "1", FCVAR_ARCHIVE);
#else
	gl_widescreen_yfov = gEngfuncs.pfnGetCvarPointer("gl_widescreen_yfov");
#endif

	//
	// Load shaders
	//
	eWorldShader_locs[world_projectionmatrix] = s_BSPShader.GetUniformLoc("projectionmatrix");
	eWorldShader_locs[world_viewmatrix] = s_BSPShader.GetUniformLoc("viewmatrix");
	eWorldShader_locs[world_modelmatrix] = s_BSPShader.GetUniformLoc("modelmatrix");

	eWorldShader_locs[world_spotlight_texturematrix] = s_BSPShader.GetUniformLoc("spotlight_texturematrix");

	eWorldShader_locs[world_pointlight] = s_BSPShader.GetUniformLoc("pointlight");
	eWorldShader_locs[world_spotlight] = s_BSPShader.GetUniformLoc("spotlight");
	eWorldShader_locs[world_shadow] = s_BSPShader.GetUniformLoc("shadow");
	eWorldShader_locs[world_onlyshadow] = s_BSPShader.GetUniformLoc("onlyshadow");

	eWorldShader_locs[world_sundir] = s_BSPShader.GetUniformLoc("sunDir");

	eWorldShader_locs[world_renderamt] = s_BSPShader.GetUniformLoc("renderamt");
	eWorldShader_locs[world_rendercolor] = s_BSPShader.GetUniformLoc("rendercolor");

	eWorldShader_locs[world_light_pos] = s_BSPShader.GetUniformLoc("light_pos");
	eWorldShader_locs[world_light_radius] = s_BSPShader.GetUniformLoc("light_radius");
	eWorldShader_locs[world_light_color] = s_BSPShader.GetUniformLoc("light_color");

	eWorldShader_locs[world_sunshadow_fadedist] = s_BSPShader.GetUniformLoc("sunshadow_fadedist");
	eWorldShader_locs[world_sunshadow_strength] = s_BSPShader.GetUniformLoc("sunshadow_strength");

	eWorldShader_locs[world_lightmap_pass] = s_BSPShader.GetUniformLoc("lightmap_pass");
	eWorldShader_locs[world_texture_pass] = s_BSPShader.GetUniformLoc("texture_pass");

	eWorldShader_locs[world_renderorigin] = s_BSPShader.GetUniformLoc("renderorigin");
	//eWorldShader_locs[world_renderforward] = s_BSPShader.GetUniformLoc("renderforward");
	eWorldShader_locs[world_renderright] = s_BSPShader.GetUniformLoc("renderright");

	eWorldShader_locs[world_fog_active] = s_BSPShader.GetUniformLoc("fog_active");
	eWorldShader_locs[world_fogcolor] = s_BSPShader.GetUniformLoc("fogcolor");
	eWorldShader_locs[world_fogstart] = s_BSPShader.GetUniformLoc("fogstart");
	eWorldShader_locs[world_fogend] = s_BSPShader.GetUniformLoc("fogend");

	eWorldShader_locs[world_lightgamma] = s_BSPShader.GetUniformLoc("lightgamma");
	eWorldShader_locs[world_texgamma] = s_BSPShader.GetUniformLoc("texgamma");

	eWorldShader_locs[world_wireframe] = s_BSPShader.GetUniformLoc("wireframe");

	eWorldShader_locs[world_waterpolys] = s_BSPShader.GetUniformLoc("waterpolys");
	eWorldShader_locs[world_scrollingpolys] = s_BSPShader.GetUniformLoc("scrollingpolys");
	eWorldShader_locs[world_specular] = s_BSPShader.GetUniformLoc("specular");
	eWorldShader_locs[world_fltime] = s_BSPShader.GetUniformLoc("fltime");
	eWorldShader_locs[world_alphatest] = s_BSPShader.GetUniformLoc("alphatest");

	eWorldShader_locs[world_clipplane] = s_BSPShader.GetUniformLoc("clipplane");


	eWorldSolidShader_locs[worldsolid_projviewmatrix] = s_BSPSolidShader.GetUniformLoc("projviewmatrix");
	eWorldSolidShader_locs[worldsolid_modelmatrix] = s_BSPSolidShader.GetUniformLoc("modelmatrix");
	eWorldSolidShader_locs[worldsolid_alphatest] = s_BSPSolidShader.GetUniformLoc("alphatest");
	eWorldSolidShader_locs[worldsolid_light_pos] = s_BSPSolidShader.GetUniformLoc("light_pos");

	eSimpleSkyboxShader_locs[skybox_projviewmatrix] = s_SkyboxShader.GetUniformLoc("projviewmatrix");
	eSimpleSkyboxShader_locs[skybox_skyfog] = s_SkyboxShader.GetUniformLoc("skyfog");
	eSimpleSkyboxShader_locs[skybox_fogcolor] = s_SkyboxShader.GetUniformLoc("fogcolor");

	eDecalShader_locs[decal_projviewmatrix] = s_DecalShader.GetUniformLoc("projviewmatrix");
	eDecalShader_locs[decal_wireframe] = s_DecalShader.GetUniformLoc("wireframe");


	s_BSPShader.Bind();
	s_BSPShader.Uniform1i(s_BSPShader.GetUniformLoc("lightmap_texture"), LIGHTMAP_TEXUNIT - GL_TEXTURE0);
	s_BSPShader.Uniform1i(s_BSPShader.GetUniformLoc("base_texture"), SURFTEXTURE_TEXUNIT - GL_TEXTURE0);
	s_BSPShader.Uniform1i(s_BSPShader.GetUniformLoc("spotlight_texture"), SPOTLIGHT_TEXUNIT - GL_TEXTURE0);
	s_BSPShader.Uniform1i(s_BSPShader.GetUniformLoc("shadow_texture"), SHADOWMAP_TEXUNIT - GL_TEXTURE0);
	s_BSPShader.Uniform1i(s_BSPShader.GetUniformLoc("cubemap_texture"), CUBEMAPSHADOW_TEXUNIT - GL_TEXTURE0);

	s_BSPSolidShader.Bind();
	s_BSPSolidShader.Uniform1i(s_BSPSolidShader.GetUniformLoc("base_texture"), 1);

	s_SkyboxShader.Bind();
	s_SkyboxShader.Uniform1i(s_SkyboxShader.GetUniformLoc("texture0"), 0);

	s_DecalShader.Bind();
	s_DecalShader.Uniform1i(s_DecalShader.GetUniformLoc("lmtexture"), 0);
	s_DecalShader.Uniform1i(s_DecalShader.GetUniformLoc("texture0"), 1);

	m_FilterShader.Bind();
	m_FilterShader.Uniform1i(m_FilterShader.GetUniformLoc("texture_"), 0);
	m_FilterShader.Uniform1i(m_FilterShader.GetUniformLoc("cube_texture_"), 1);

	m_FilterShader.Uniform1i(m_FilterShader.GetUniformLoc("flipped"), 0);

	m_BlacknwhiteShader.Bind();
	m_BlacknwhiteShader.Uniform1i(m_BlacknwhiteShader.GetUniformLoc("texture0"), 0);
	m_BlacknwhiteShader.Uniform1i(m_BlacknwhiteShader.GetUniformLoc("flipped"), 0);

	Vector verts[] =
		{
			//covers entire screen
			Vector(1.0, 1.0, 0.0),		// Top-right
			Vector(-1.0, 1.0, 0.0),		// Top-left
			Vector(-1.0, -1.0, 0.0),		// Bottom-left

			Vector(-1.0, -1.0, 0.0),		// Bottom-left
			Vector(1.0, -1.0, 0.0),		// Bottom-right
			Vector(1.0, 1.0, 0.0),		// Top-right

			//covers only the top right quarter of screen
			Vector(1.0, 1.0, 0.0),		// Top-right
			Vector(0, 1.0, 0.0),			// Top-left
			Vector(0.0, 0.0, 0.0),				// Bottom-left

			Vector(0.0, 0.0, 0.0),				// Bottom-left
			Vector(1.0, 0.0, 0.0),			// Bottom-right
			Vector(1.0, 1.0, 0.0),		// Top-right

			// covers only the bottom right quarter of screen
			Vector(1.0, 0.0, 0.0),		// Top-right
			Vector(-1.0, 1.0, 0.0),			// Top-left
			Vector(0.0, -1.0, 0.0),				// Bottom-left

			Vector(0.0, -1.0, 0.0),				// Bottom-left
			Vector(1.0f, -1.0, 0.0),			// Bottom-right
			Vector(1.0f, 0.0, 0.0),			// Top-right

			// covers only the bottom left quarter of screen
			Vector(0.0, 0.0, 0.0),	 // Top-right
			Vector(-1.0, 0, 0.0), // Top-left
			Vector(-1.0, -1.0, 0.0),	 // Bottom-left

			Vector(-1.0, -1.0, 0.0),	 // Bottom-left
			Vector(0, -1.0, 0.0), // Bottom-right
			Vector(0, 0.0, 0.0),	 // Top-right

			// covers only the top left quarter of screen
			Vector(0.0, 1.0, 0.0),	 // Top-right
			Vector(-1.0, 1.0, 0.0),	 // Top-left
			Vector(-1.0, 0.0, 0.0), // Bottom-left

			Vector(-1.0, 0.0, 0.0), // Bottom-left
			Vector(0, 0, 0.0),	 // Bottom-right
			Vector(0.0, 1.0, 0.0)	// Top-right
		};

	m_p2DScreenMesh = new GL_Mesh(std::size(verts), nullptr, true, verts);
	m_p2DScreenMesh->SetDrawMode(GL_TRIANGLES);

	s_pDecalMeshBuffer = new GL_Mesh(524288, Default3DTextureVert_t::GetAttribLayout(), false);
	s_pDecalMeshBuffer->SetDrawMode(GL_TRIANGLES);

	constexpr Vector m_vPoints[8] =
		{
		 Vector(-150, -150, -150),
		 Vector(150, -150, -150),
		 Vector(150, 150, -150),
		 Vector(-150, 150, -150),
		 Vector(-150, -150, 150),
		 Vector(150, -150, 150),
		 Vector(150, 150, 150),
		 Vector(-150, 150, 150),
		};

	constexpr int m_iIDs[6][4] = {
		{1, 2, 6, 5}, 
		{2, 3, 7, 6}, 
		{3, 0, 4, 7},
		{0, 1, 5, 4}, 
		{2, 1, 0, 3}, 
		{7, 4, 5, 6}
	};

	std::vector<Default3DTextureVert_t> skyVerts;
	for (int i = 0; i < 6; i++)
	{
		int a = m_iIDs[i][0];
		int b = m_iIDs[i][1];
		int c = m_iIDs[i][2];
		int d = m_iIDs[i][3];

		// First triangle (a, b, c)
		skyVerts.push_back(	{m_vPoints[a], {0.0f, 1.0f}}	);
		skyVerts.push_back(	{m_vPoints[b], {1.0f, 1.0f}}	);
		skyVerts.push_back(	{m_vPoints[c], {1.0f, 0.0f}}	);

		// Second triangle (a, c, d)
		skyVerts.push_back(	{m_vPoints[a], {0.0f, 1.0f} }	);
		skyVerts.push_back(	{m_vPoints[c], {1.0f, 0.0f} }	);
		skyVerts.push_back(	{m_vPoints[d], {0.0f, 0.0f} }	);
	}

	s_pSkyboxMesh = new GL_Mesh(skyVerts.size(), Default3DTextureVert_t::GetAttribLayout(), true, skyVerts.data());
	s_pSkyboxMesh->SetDrawMode(GL_TRIANGLES);

	GL_ShaderProgram::ResetShaderBind();

	glLineWidth(0.7f);//for wireframes. this function is never called ever again, not in the goldsrc engine and not anywhere else.

};

/*
====================
VidInit

====================
*/
void CBSPRenderer::VidInit(void)
{

	// Clear this
	VectorClear(m_vSkyOrigin);
	VectorClear(m_vSkyWorldOrigin);

	m_pDynLights.clear();

	m_pSunShadowMap = nullptr;

	GL_ShadowMap::ClearAllShadowMaps();

	m_pSunShadowMap = GL_ShadowMap::AllocateShadowMap(GL_TextureHandler::_2DTexture_Storage, GL_R16F, 3184, 3184, 0, GL_RED, GL_FLOAT, false);

	if (m_pMainFBOTexture)
		delete m_pMainFBOTexture;

	GL_TextureHandler::gl_texturecreationinfo_t mainFBOTextureInfo =
	{
			std::string(), GL_TextureHandler::_2DTexture, GL_RGB8, ScreenWidth, ScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE
	};

	m_pMainFBOTexture = new GL_TextureHandler(&mainFBOTextureInfo);

	if (m_pMainFBO)
		delete m_pMainFBO;

	m_pMainFBO = new GL_FBOHandler();

	if (m_pMainRBO)
		delete m_pMainRBO;

	m_pMainRBO = new GL_RBOHandler();
	

	m_pMainFBO->Bind(GL_FBOHandler::Framebuffer);
	m_pMainRBO->Bind();
	m_pMainRBO->RenderBufferStorage(GL_DEPTH_COMPONENT16, ScreenWidth, ScreenHeight);
	m_pMainFBO->FramebufferRenderbuffer(GL_FBOHandler::Framebuffer, GL_FBOHandler::DepthAttachment, m_pMainRBO);

	GL_FBOHandler::ResetToMainFBO();

	// Clear all lightstyles.
	memset(m_iLightStyleValue, 0, sizeof(m_iLightStyleValue));
	m_pDecalGroups.clear();
	memset(m_pNormalTextureList, 0, sizeof(m_pNormalTextureList));
	memset(&m_pFlashlightTextures, 0, sizeof(m_pFlashlightTextures));
	memset(&gHUD.m_pSkyFogSettings, 0, sizeof(fog_settings_t));
	memset(&gHUD.m_pFogSettings, 0, sizeof(fog_settings_t));

	// Clear previous
	if (m_iNumSurfaces)
	{
		delete[] m_pSurfaces;
		m_pSurfaces = nullptr;
		m_iNumSurfaces = NULL;
	}

	if (m_pSurfacePointersArray)
	{
		delete[] m_pSurfacePointersArray;
		m_pSurfacePointersArray = nullptr;
	}

	// Get pointer to very first dynamic and entity light, key doesn't matter.
	m_pFirstELight = gEngfuncs.pEfxAPI->CL_AllocElight(0);
	m_pFirstDLight = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
	m_fSkySpeed = NULL;
	m_iFrameCount = NULL;
	m_iNumTextures = NULL;
	m_bMirroring = false;
	m_iNumFlashlightTextures = NULL;
	m_bGotStaticLights = false;

	r_oldviewleaf = nullptr;

	DeleteDecals();

	// A call to VidInit means a reload
	m_bReloaded = true;

	BSPWorld_Model::VidInit();
}

/*
====================
ExtensionSupported

====================
*/
bool CBSPRenderer::ExtensionSupported(const char* ext)
{
	const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
	const char* start = extensions;
	const char* ptr;

	while ((ptr = strstr(start, ext)) != NULL)
	{
		// we've found, ensure name is exactly ext
		const char* end = ptr + strlen(ext);
		if (isspace(*end) || *end == '\0')
			return true;

		start = end;
	}
	return false;
}

extern int cl_numvisedicts;
extern cl_entity_s* cl_visedicts[512];

/*
====================
GetRenderEnts

====================
*/
void CBSPRenderer::GetRenderEnts(void)
{
	// Clear counters
	m_iNumModelLights = 0;
	m_fShadowGenerationTime = 0;

	g_GlobalGLState.ResetStates();

	g_StudioRenderer.StudioClearDrawList();
	g_LegacySpriteRenderer.ClearDrawList();

	m_iNumRenderEntities = 0;

	cl_entity_t* pPlayer = gEngfuncs.GetLocalPlayer();
	cl_entity_t* pView = &engine_cl->viewent;

	int iMsg = pPlayer->curstate.messagenum;

	// Clear water shader class
	for (int i = 0; i < gWaterShader.m_iNumWaterEntities; i++)
		gWaterShader.m_pWaterEntities[i].draw = false;

	// Clear mirror class
	for (int i = 0; i < gMirrorManager.m_iNumMirrors; i++)
		gMirrorManager.m_pMirrors[i].draw = false;

	for (int i = 1; i < engine_cl->max_edicts; i++)
	{
		cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(i);

		if (!pEntity)
			break;

		if (!pEntity->model)
			continue;

		if (pEntity->curstate.messagenum != iMsg)
			continue;

		if ((pEntity->curstate.effects & FL_MIRROR) && gMirrorManager.m_pCvarDrawMirrors->value > 0)
		{
			if (!pEntity->efrag)
				gMirrorManager.AllocNewMirror(pEntity);

			if (pEntity->efrag)
				((cl_mirror_t*)pEntity->efrag)->draw = true;

			continue;
		}

		if ((pEntity->curstate.effects & FL_WATERSHADER) && gWaterShader.m_pCvarWaterShader->value >= 1)
		{
			if (!pEntity->efrag)
				gWaterShader.AddEntity(pEntity);

			if (pEntity->efrag)
				((cl_water_t*)pEntity->efrag)->draw = true;

			continue;
		}

		if (pEntity->curstate.effects & FL_ELIGHT)
		{
			// mlights are static
			mlight_t* mlight = &m_pModelLights[m_iNumModelLights];
			
			mlight->color.x = (float)pEntity->curstate.rendercolor.r / 255;
			mlight->color.y = (float)pEntity->curstate.rendercolor.g / 255;
			mlight->color.z = (float)pEntity->curstate.rendercolor.b / 255;
			mlight->radius = pEntity->curstate.renderamt * 9.5;
			
			mlight->origin = pEntity->origin;
			mlight->mins.x = mlight->origin.x - mlight->radius;
			mlight->mins.y = mlight->origin.y - mlight->radius;
			mlight->mins.z = mlight->origin.z - mlight->radius;
			mlight->maxs.x = mlight->origin.x + mlight->radius;
			mlight->maxs.y = mlight->origin.y + mlight->radius;
			mlight->maxs.z = mlight->origin.z + mlight->radius;
			
			mlight->spotcos = 0;
			mlight->flashlight = false;

			if (pEntity->curstate.renderfx >= 1)
			{
				cl_dlight_t* dlight = CL_AllocDLight(pEntity->index);
			
				dlight->color.x = mlight->color.x;
				dlight->color.y = mlight->color.y;
				dlight->color.z = mlight->color.z;
			
				dlight->radius = pEntity->curstate.renderamt * 9.5;
				dlight->origin = pEntity->curstate.origin;
				dlight->die = engine_cl->time + 1;
				dlight->flags |= LIGHT_STUDIOMDL_SHADOW | LIGHT_ONLYSHADOWS;
				if (pEntity->curstate.renderfx == 2)
					dlight->flags |= LIGHT_BRUSH_SHADOW;
				if (pEntity->curstate.renderfx == 3)
					dlight->flags |= LIGHT_WORLD_SHADOW;
			
			}




			m_iNumModelLights++;
			continue;
		}

		if (pEntity->curstate.effects & FL_DLIGHT)
		{
			cl_dlight_t* dlight = CL_AllocDLight(pEntity->index);

			dlight->color.x = (float)pEntity->curstate.rendercolor.r / 255;
			dlight->color.y = (float)pEntity->curstate.rendercolor.g / 255;
			dlight->color.z = (float)pEntity->curstate.rendercolor.b / 255;
			dlight->radius = pEntity->curstate.renderamt * 16;
			dlight->origin = pEntity->curstate.origin;
			dlight->die = engine_cl->time + 1;

			dlight->flags = pEntity->curstate.sequence ? 0 : LIGHT_CASTSHADOWS;
			continue;
		}
			//debug pointlight shadowmaps
		//if (pEntity->player)
		//{
		//	cl_dlight_t* dlight = CL_AllocDLight(pEntity->index);
		//
		//	dlight->color.x = 1;
		//	dlight->color.y = 1;
		//	dlight->color.z = 1;
		//	dlight->radius = 2048;
		//	dlight->origin = m_vRenderOrigin;
		//	dlight->die = engine_cl->time + 1;
		//	continue;
		//}

		if (pEntity->curstate.effects & FL_SPOTLIGHT)
		{

			if (pEntity->curstate.body >= m_iNumFlashlightTextures)
			{
				gEngfuncs.Con_Printf("Texture index greater than the amount of flashlight textures loaded!\n");
				continue;
			}

			cl_dlight_t* dlight = CL_AllocDLight(pEntity->index);

			dlight->color.x = (float)pEntity->curstate.rendercolor.r / 255;
			dlight->color.y = (float)pEntity->curstate.rendercolor.g / 255;
			dlight->color.z = (float)pEntity->curstate.rendercolor.b / 255;
			dlight->radius = pEntity->curstate.renderamt * 16;
			dlight->origin = pEntity->curstate.origin;
			dlight->cone_size = pEntity->curstate.scale;
			dlight->angles = pEntity->angles;
			dlight->die = engine_cl->time + 1;
			dlight->textureindex = m_pFlashlightTextures[pEntity->curstate.body]->iIndex;
			dlight->flags = pEntity->curstate.sequence ? 0 : LIGHT_CASTSHADOWS;

			dlight->frustum.SetFrustum(dlight->angles, dlight->origin, dlight->cone_size * 1.2, dlight->radius);
			continue;
		}

		if (pEntity->curstate.effects & EF_LIGHT)
		{
			cl_dlight_t* dlight = CL_AllocDLight(pEntity->index);

			dlight->color.x = 1.0;
			dlight->color.y = 1.0;
			dlight->color.z = 1.0;
			dlight->radius = 500;
			dlight->origin = pEntity->curstate.origin;
			dlight->die = engine_cl->time + 1;
			dlight->decay = 500;
			dlight->flags = pEntity->curstate.sequence ? 0 : LIGHT_CASTSHADOWS;
			continue;
		}

		if (!g_StudioRenderer.m_pCvarDrawEntities->value)
			continue;

		if (pEntity->model->type == mod_brush)
		{
			m_pRenderEntities[m_iNumRenderEntities] = pEntity;
			m_iNumRenderEntities++;
		}
		else if (pEntity->model->type == mod_studio)
		{
			g_StudioRenderer.StudioPushEntityToDraw(pEntity);
		}
		else if (pEntity->model->type == mod_sprite)
		{
			g_LegacySpriteRenderer.PushEntityToDraw(pEntity);
		}
	}

	for (int i = 0; i < cl_numvisedicts; i++)
	{
		auto pEntity = cl_visedicts[i];
		if (!pEntity->model)
			continue;

		m_pRenderEntities[m_iNumRenderEntities] = pEntity;
		m_iNumRenderEntities++;

		if (pEntity->model->type == mod_studio)
			g_StudioRenderer.StudioPushEntityToDraw(pEntity);
		else if (pEntity->model->type == mod_sprite)
			g_LegacySpriteRenderer.PushEntityToDraw(pEntity);
	}

	g_StudioRenderer.StudioUploadRenderData();

	m_bGotAdditional = false;

	dlight_t* el = m_pFirstELight;
	for (int i = 0; i < MAX_GOLDSRC_ELIGHTS; i++, el++)
	{
		if (el->die < engine_cl->time || !el->radius)
			continue;

		mlight_t* mlight = &m_pModelLights[m_iNumModelLights];
		m_iNumModelLights++;

		mlight->color.x = (float)el->color.r / 255;
		mlight->color.y = (float)el->color.g / 255;
		mlight->color.z = (float)el->color.b / 255;
		mlight->radius = el->radius * 2;
		mlight->origin = el->origin;
		mlight->mins.x = mlight->origin.x - mlight->radius;
		mlight->mins.y = mlight->origin.y - mlight->radius;
		mlight->mins.z = mlight->origin.z - mlight->radius;
		mlight->maxs.x = mlight->origin.x + mlight->radius;
		mlight->maxs.y = mlight->origin.y + mlight->radius;
		mlight->maxs.z = mlight->origin.z + mlight->radius;
		mlight->flashlight = false;
		mlight->spotcos = 0;
	}

	for (auto &dynlight : m_pDynLights)
	{
		if (dynlight->visframe)
			continue;

		mlight_t* mlight = &m_pModelLights[m_iNumModelLights];

		mlight->color.x = dynlight->color.x;
		mlight->color.y = dynlight->color.y;
		mlight->color.z = dynlight->color.z;
		mlight->radius = dynlight->radius;
		mlight->origin = dynlight->origin;
		mlight->mins.x = dynlight->origin.x - mlight->radius;
		mlight->mins.y = dynlight->origin.y - mlight->radius;
		mlight->mins.z = dynlight->origin.z - mlight->radius;
		mlight->maxs.x = dynlight->origin.x + mlight->radius;
		mlight->maxs.y = dynlight->origin.y + mlight->radius;
		mlight->maxs.z = dynlight->origin.z + mlight->radius;
		mlight->spotcos = 0;

		if (dynlight->key == -666)
			mlight->flashlight = true;
		else
			mlight->flashlight = false;

		if (dynlight->cone_size)
		{
			Vector vAngles = dynlight->angles;
			AngleVectors(vAngles, &mlight->forward, NULL, NULL);
			mlight->spotcos = dynlight->cone_size;
			mlight->frustum = &dynlight->frustum;
		}

		m_iNumModelLights++;
	}
}

/*
====================
AddEntity

====================
*/
void CBSPRenderer::AddEntity(cl_entity_t* pEntity)
{
	if (!pEntity->model)
		return;

	m_pRenderEntities[m_iNumRenderEntities] = pEntity;
	m_iNumRenderEntities++;
}

/*
====================
GetAdditionalLights

====================
*/
void CBSPRenderer::GetAdditionalLights(void)
{
	if (m_bGotAdditional)
		return;

	for (auto mdl_light : gPropManager.m_pModelLights)
	{
		if (IsInPotentiallyVisibleSet(mdl_light.visframe))
		{
			mlight_t* mlight = &m_pModelLights[m_iNumModelLights];
			m_iNumModelLights++;

			mlight->color.x = (float)mdl_light.curstate.rendercolor.r / 255;
			mlight->color.y = (float)mdl_light.curstate.rendercolor.g / 255;
			mlight->color.z = (float)mdl_light.curstate.rendercolor.b / 255;
			mlight->radius = mdl_light.curstate.renderamt * 9.5;
			mlight->origin = mdl_light.curstate.origin;
			mlight->mins.x = mlight->origin.x - mlight->radius;
			mlight->mins.y = mlight->origin.y - mlight->radius;
			mlight->mins.z = mlight->origin.z - mlight->radius;
			mlight->maxs.x = mlight->origin.x + mlight->radius;
			mlight->maxs.y = mlight->origin.y + mlight->radius;
			mlight->maxs.z = mlight->origin.z + mlight->radius;

			mlight->flashlight = false;
			mlight->spotcos = 0;
		}

		if (!m_bGotStaticLights && mdl_light.curstate.eflags) //.eflags = studioshadow keyvalue in map editor
		{
			cl_dlight_t* dlight = CL_AllocDLight(0);

			dlight->color.x = (float)mdl_light.curstate.rendercolor.r / 255;
			dlight->color.y = (float)mdl_light.curstate.rendercolor.g / 255;
			dlight->color.z = (float)mdl_light.curstate.rendercolor.b / 255;
			dlight->radius = mdl_light.curstate.renderamt * 9.5;
			dlight->origin = mdl_light.curstate.origin;
			dlight->die = engine_cl->time + std::numeric_limits<double>::max();

			clientmleaf_t* pLeaf = Mod_PointInLeaf(dlight->origin);

			if (pLeaf)
			{
				dlight->visframe = pLeaf - BSPWorld_Model::m_pWorldLeafs - 1;
			}

			dlight->flags |= LIGHT_STUDIOMDL_SHADOW | LIGHT_ONLYSHADOWS;
		}
	}

	// Got them all
	m_bGotAdditional = true;
	m_bGotStaticLights = true;
}

/*
====================
AddLightStyle

====================
*/
void CBSPRenderer::AddLightStyle(int iNum, const char* szStyle)
{
	memset(&m_pLightStyles[iNum], 0, sizeof(lightstyle_t));

	m_pLightStyles[iNum].length = strlen(szStyle);
	strcpy(m_pLightStyles[iNum].map, szStyle);
};

/*
====================
FilterEntities

====================
*/
int CBSPRenderer::FilterEntities(int type, struct cl_entity_s* pEntity, const char* modelname)
{
	if (pEntity->model->type == mod_brush)
		return FALSE;

	if (pEntity->curstate.effects & FL_DLIGHT)
		return FALSE;

	if (pEntity->curstate.effects & FL_ELIGHT)
		return FALSE;

	if (pEntity->curstate.effects & FL_SPOTLIGHT)
		return FALSE;

	return TRUE;
}


extern void AdjustFOV(float* fov_x, float* fov_y, float width, float height, qboolean lock_x);

/*
====================
SetupPreFrame

====================
*/
void CBSPRenderer::SetupPreFrame(ref_params_t* pparams)
{
	m_RefParams = *pparams;

	VectorCopy(pparams->viewangles, m_vViewAngles);
	VectorCopy(pparams->vieworg, m_vRenderOrigin);

	m_pViewLeaf = Mod_PointInLeaf(pparams->vieworg);
	R_MarkLeaves(m_pViewLeaf);

	// Get additional lights here
	GetAdditionalLights();

	float fov = gHUD.m_iFOV;
	if (!fov)
		return;
	float fovy = fov;

	AdjustFOV(&fov, &fovy, ScreenWidth, ScreenHeight, 0);

	float aspect = (float)ScreenWidth / (float)ScreenHeight;

	m_ProjectionMatrix = glm::perspective(fov, aspect, 8.f, 32768.f);
	
	Vector forward, up;
	AngleVectors(m_vViewAngles, &forward, nullptr, &up);

	glm::vec3 cameraForward = glm::vec3(forward.x, forward.y, forward.z);

	glm::vec3 cameraPos = glm::vec3(m_vRenderOrigin.x, m_vRenderOrigin.y, m_vRenderOrigin.z);
	glm::vec3 cameraTarget = cameraPos + cameraForward;
	glm::vec3 cameraUp = glm::vec3(up.x, up.y, up.z);

	m_ViewMatrix = glm::lookAt(cameraPos, cameraTarget, cameraUp);

	m_ModelMatrix = glm::mat4(1.0f);

	GL_ShaderProgram::ResetShaderBind();

}

/*
====================
CheckTextures

====================
*/
void CBSPRenderer::CheckTextures(void)
{
	char szFile[64];
	char szTexture[32];

	model_t* pWorld = engine_cl->worldmodel;
	for (int i = 0; i < pWorld->numtextures; i++)
	{
		if (!pWorld->textures[i])
			continue;

		// Skip specials
		if (!strcmp(pWorld->textures[i]->name, "origin") || !strcmp(pWorld->textures[i]->name, "clip") || !strcmp(pWorld->textures[i]->name, "null") || !strcmp(pWorld->textures[i]->name, "skip") || !strcmp(pWorld->textures[i]->name, "hint"))
			continue;

		strcpy(szTexture, pWorld->textures[i]->name);
		strLower(szTexture);

		if (gTextureLoader.TextureHasFlag("world", szTexture, TEXFLAG_ALTERNATE))
		{
			gEngfuncs.Con_DPrintf("World has '%s' marked as using an alternate texture.\n", pWorld->textures[i]->name);
			sprintf(szFile, "gfx/textures/world/%s.dds", szTexture);
			cl_texture_t* pTexture = gTextureLoader.LoadTexture(szFile);
			pWorld->textures[i]->gl_texturenum = pTexture->iIndex;

			if (pTexture)
			{
				gEngfuncs.Con_DPrintf("Loaded '%s'\n", szFile);
				continue;
			}
		}
		
		//loading textures manually seems to fix weird lightmap-being-used-as-texture issue, but only until a gamma variable has been changed, the hell is happening
		//apparently goldsrc's GL_BuildLightmaps seems to replace the last texture in a map with the lightmap texture bruh
		auto texture = gTextureLoader.LoadWADTexture(szTexture);
		if(texture)
			pWorld->textures[i]->gl_texturenum = texture->iIndex;

		auto scrollingpoly = stristr(pWorld->textures[i]->name, "scroll");
		auto specular = stristr(pWorld->textures[i]->name, "spec") || stristr(pWorld->textures[i]->name, "reflect") || stristr(pWorld->textures[i]->name, "chrome");

		pWorld->textures[i]->texture_flag = 0;

		if(scrollingpoly)
			pWorld->textures[i]->texture_flag |= TEXTURE_SCROLL;
		if (specular)
			pWorld->textures[i]->texture_flag |= TEXTURE_SPECULAR;
	}

	if (scopetexture != nullptr)
	{
		scopetexture = nullptr;
	}
	scopetexture = gTextureLoader.LoadTexture("gfx/textures/smgscope.tga");
}
/*
====================
SetupRenderer

====================
*/
void CBSPRenderer::SetupRenderer(void)
{
	if (!m_bReloaded)
		return;

	glPushAttrib(GL_TEXTURE_BIT);

	m_bReloaded = false;

	if (!engine_cl->worldmodel)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "FATAL ERROR: Failed to get world!\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	BSPWorld_Model::InitWorldModel(engine_cl->worldmodel);

	gPropManager.GenerateEntityList();

	//digest studiomdls currently precached in engine
	g_StudioRenderer.StudioSwapEngineCache();

	gTextureLoader.LoadTextureScript();
	gTextureLoader.LoadWADFiles();
	LoadWADDecals();
	LoadDecals();
	CheckTextures(); // Do it for world seperately
	gWaterShader.LoadScript();

	CreateTextures();

	UploadLightmaps();
	GenerateVertexArray();

	InitSky();
	RemoveSky();

	gTextureLoader.FreeWADFiles();
	gPropManager.ClearEntityData();

	glPopAttrib();
}

/*
====================
RendererRefDef

====================
*/
void CBSPRenderer::RendererRefDef(ref_params_t* pparams)
{

	gHUD.viewFrustum.SetFrustum(pparams->viewangles, pparams->vieworg, gHUD.m_iFOV, gHUD.m_pFogSettings.end, true);

	gParticleEngine.m_iNumParticles = NULL;
	m_iBrushPolyCounter = NULL;
	m_iBSPVertsCounter = NULL;
	m_iStudioPolyCounter = NULL;

	// Advance frame count here
	m_iFrameCount++;

	// Clear HL's dlight array
	dlight_t* dl = m_pFirstDLight;
	for (int i = 0; i < MAX_GOLDSRC_DLIGHTS; i++, dl++)
		memset(dl, 0, sizeof(dlight_t));
};

/*
====================
RemoveSky

====================
*/
void CBSPRenderer::RemoveSky(void)
{
	bool foundSky = false;
	clientmsurface_t* surfaces = BSPWorld_Model::m_pWorldSurfaces;
	for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++)
	{
		if (surfaces[i].flags & SURF_DRAWSKY)
		{
			glpoly_t* p = surfaces[i].polys;
			p->numverts = -p->numverts;
			foundSky = true;
		}
	}

	if (!foundSky)
		m_bDrawSky = false;
};

/*
====================
CreateTextures

====================
*/
void CBSPRenderer::CreateTextures(void)
{
	//
	// Load flashlight texture
	//
	m_pFlashlightTextures[0] = gTextureLoader.LoadTexture("gfx/textures/flashlight.tga", true, false, true);
	m_iNumFlashlightTextures++;

	if (!m_pFlashlightTextures[0])
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "ERROR: Failed to load gfx/textures/flashlight.tga.\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	// ALWAYS Bind
	glBindTexture(GL_TEXTURE_2D, m_pFlashlightTextures[0]->iIndex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	for (int i = 0; i < gTextureLoader.m_pTextureEntries.size(); i++)
	{
		texentry_t* pEntry = gTextureLoader.m_pTextureEntries[i];
		if (!strcmp(pEntry->szModel, "flashlight"))
		{
			char szPath[64];
			sprintf(szPath, "gfx/textures/%s.tga", pEntry->szTexture);
			cl_texture_t* pTexture = gTextureLoader.LoadTexture(szPath, true, false, true);
			if (pTexture)
			{
				m_pFlashlightTextures[m_iNumFlashlightTextures] = pTexture;
				m_iNumFlashlightTextures++;

				glBindTexture(GL_TEXTURE_2D, pTexture->iIndex);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		}
	}

	//
	// Copy over all world textures
	//
	for (int i = 0; i < engine_cl->worldmodel->numtextures; i++)
	{
		if (!engine_cl->worldmodel->textures[i])
			continue;

		memcpy(&m_pNormalTextureList[m_iNumTextures], engine_cl->worldmodel->textures[i], sizeof(texture_t));
		m_iNumTextures++;
	}

	// Relink animations
	for (int i = 0; i < m_iNumTextures; i++)
	{
		if (m_pNormalTextureList[i].anim_next)
		{
			for (int j = 0; j < m_iNumTextures; j++)
			{
				if (!strcmp(m_pNormalTextureList[j].name, m_pNormalTextureList[i].anim_next->name))
				{
					m_pNormalTextureList[i].anim_next = &m_pNormalTextureList[j];
					break;
				}
			}
		}
	}
}

/*
====================
FreeBuffer

====================
*/
void CBSPRenderer::FreeBuffer(void)
{
	if (m_pBSPMesh)
	{
		delete m_pBSPMesh;
		m_pBSPMesh = nullptr;
	}

	if (m_pBufferData)
	{
		delete[] m_pBufferData;
		m_pBufferData = NULL;
	}

	if (m_pFacesExtraData)
	{
		delete[] m_pFacesExtraData;
		m_pFacesExtraData = NULL;
	}
}


/*
====================
GenerateVertexArray

====================
*/
void CBSPRenderer::GenerateVertexArray(void)
{
	int iNumFaces = 0;
	int iCurFace = 0;

	int iNumVerts = 0;
	int iCurVert = 0;

	// delete existing data
	FreeBuffer();

	clientmsurface_t* surfaces = BSPWorld_Model::m_pWorldSurfaces;
	for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++)
	{
		if (surfaces[i].flags & SURF_DRAWSKY)
			continue;

		if (!(surfaces[i].flags & SURF_DRAWTURB) && surfaces[i].polys->next)
			continue; // lets be sure

		for (glpoly_t* bp = surfaces[i].polys; bp; bp = bp->next)
			iNumVerts += 3 + (bp->numverts - 3) * 3;

		iNumFaces++;
	}

	int nbPointers = engine_cl->worldmodel->numsurfaces;

	m_pSurfacePointersArray = new brushface_t*[nbPointers];
	for (int i = 0; i < iNumFaces; i++)
		m_pSurfacePointersArray[i] = nullptr;

	// Set array up
	m_pBufferData = new brushvertex_t[iNumVerts];
	memset(m_pBufferData, 0, sizeof(brushvertex_t) * iNumVerts);
	m_iTotalVertCount = iNumVerts;
	m_iTotalTriCount = iNumVerts / 3;

	m_pFacesExtraData = new brushface_t[iNumFaces];
	memset(m_pFacesExtraData, 0, sizeof(brushface_t) * iNumFaces);
	m_iTotalFaceCount = iNumFaces;

	int pointerIndex = 0;
	for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++, pointerIndex++)
	{
		if (surfaces[i].flags & SURF_DRAWSKY)
			continue;

		glpoly_t* poly = surfaces[i].polys;

		if (poly->numverts < 3)
			continue;

		if (!(surfaces[i].flags & SURF_DRAWTURB) && poly->next)
			continue; // lets be sure

		brushface_t* ext = &m_pFacesExtraData[iCurFace];
		VectorCopy(surfaces[i].texinfo->vecs[0], ext->s_tangent);
		VectorCopy(surfaces[i].texinfo->vecs[1], ext->t_tangent);
		VectorNormalize(ext->s_tangent);
		VectorNormalize(ext->t_tangent);
		VectorCopy(surfaces[i].plane->normal, ext->normal);
		ext->index = i;

		// Set ptr
		m_pSurfacePointersArray[pointerIndex] = ext;

		if (surfaces[i].flags & SURF_PLANEBACK)
			VectorInverse(ext->normal);

		// Link up with renderlist textures
		for (int j = 0; j < m_iNumTextures; j++)
		{
			if (!strcmp(m_pNormalTextureList[j].name, surfaces[i].texinfo->texture->name))
			{
				m_pSurfaces[ext->index].regtexture = &m_pNormalTextureList[j];
				break;
			}
		}

		int column = surfaces[i].lightmaptexturenum % LIGHTMAP_NUMROWS;
		int row = (surfaces[i].lightmaptexturenum / LIGHTMAP_NUMROWS) % LIGHTMAP_NUMCOLUMNS;

		ext->start_vertex = iCurVert;
		for (glpoly_t* bp = surfaces[i].polys; bp; bp = bp->next)
		{
			brushvertex_t pVertexes[3];
			float* v = bp->verts[0];

			for (int j = 0; j < 3; j++, v += VERTEXSIZE)
			{
				pVertexes[j].pos[0] = v[0];
				pVertexes[j].pos[1] = v[1];
				pVertexes[j].pos[2] = v[2];
				pVertexes[j].texcoord[0] = v[3];
				pVertexes[j].texcoord[1] = v[4];
				pVertexes[j].lightmaptexcoord[0] = (v[5] + column) / LIGHTMAP_NUMCOLUMNS;
				pVertexes[j].lightmaptexcoord[1] = (v[6] + row) / LIGHTMAP_NUMROWS;

				pVertexes[j].normal[0] = ext->normal[0];
				pVertexes[j].normal[1] = ext->normal[1];
				pVertexes[j].normal[2] = ext->normal[2];
			}

			memcpy(&m_pBufferData[iCurVert], &pVertexes[0], sizeof(brushvertex_t));
			iCurVert++;
			memcpy(&m_pBufferData[iCurVert], &pVertexes[1], sizeof(brushvertex_t));
			iCurVert++;
			memcpy(&m_pBufferData[iCurVert], &pVertexes[2], sizeof(brushvertex_t));
			iCurVert++;

			for (int j = 0; j < (bp->numverts - 3); j++, v += VERTEXSIZE)
			{
				memcpy(&pVertexes[1], &pVertexes[2], sizeof(brushvertex_t));

				pVertexes[2].pos[0] = v[0];
				pVertexes[2].pos[1] = v[1];
				pVertexes[2].pos[2] = v[2];
				pVertexes[2].texcoord[0] = v[3];
				pVertexes[2].texcoord[1] = v[4];
				pVertexes[2].lightmaptexcoord[0] = (v[5] + column) / LIGHTMAP_NUMCOLUMNS;
				pVertexes[2].lightmaptexcoord[1] = (v[6] + row) / LIGHTMAP_NUMROWS;

				pVertexes[2].normal[0] = ext->normal[0];
				pVertexes[2].normal[1] = ext->normal[1];
				pVertexes[2].normal[2] = ext->normal[2];

				memcpy(&m_pBufferData[iCurVert], &pVertexes[0], sizeof(brushvertex_t));
				iCurVert++;
				memcpy(&m_pBufferData[iCurVert], &pVertexes[1], sizeof(brushvertex_t));
				iCurVert++;
				memcpy(&m_pBufferData[iCurVert], &pVertexes[2], sizeof(brushvertex_t));
				iCurVert++;
			}
		}
		ext->num_vertexes = iCurVert - ext->start_vertex;
		iCurFace++;
	}

	//now check for special textures

	iNumVerts = 0;
	iCurVert = 0;
	iNumFaces = 0;
	iCurFace = 0;
	pointerIndex = 0;

	for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++)
	{
		if (surfaces[i].flags & SURF_DRAWSKY)
			continue;

		if (!(surfaces[i].flags & SURF_DRAWTURB) && surfaces[i].polys->next)
			continue; // lets be sure

		for (glpoly_t* bp = surfaces[i].polys; bp; bp = bp->next)
			iNumVerts += 3 + (bp->numverts - 3) * 3;

		iNumFaces++;
	}

	for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++, pointerIndex++)
	{
		if (surfaces[i].flags & SURF_DRAWSKY)
			continue;

		glpoly_t* poly = surfaces[i].polys;

		if (poly->numverts < 3)
			continue;

		if (!(surfaces[i].flags & SURF_DRAWTURB) && poly->next)
			continue; // lets be sure

		brushface_t* ext = &m_pFacesExtraData[iCurFace];

		auto texname = surfaces[i].texinfo->texture->name;

		auto specular = stristr(texname, "spec") || stristr(texname, "reflect") || stristr(texname, "chrome");

		for (glpoly_t* bp = surfaces[i].polys; bp; bp = bp->next)
		{
			brushvertex_t pVertexes[3];
			float* v = bp->verts[0];

			float bigx = 0, bigy = 0;

			for (int j = 0; j < 3; j++, v += VERTEXSIZE)
			{
				float scale_speed[2];

				if (specular)
				{
					brushvertex_t* pEndVert = &m_pBufferData[ext->start_vertex];

					for (int verti = 1; verti < ext->num_vertexes; verti++)
					{
						// check for all vert
						if ((m_pBufferData[ext->start_vertex + verti].pos - m_pBufferData[ext->start_vertex].pos).Length() > (pEndVert->pos - m_pBufferData[ext->start_vertex].pos).Length())
						{
							pEndVert = &m_pBufferData[ext->start_vertex + verti];
						}
					}

					scale_speed[0] = fabs((pEndVert->texcoord[0]) - (m_pBufferData[ext->start_vertex].texcoord[0])) / (pEndVert->pos - m_pBufferData[ext->start_vertex].pos).Length();
					scale_speed[1] = fabs((pEndVert->texcoord[1]) - (m_pBufferData[ext->start_vertex].texcoord[1])) / (pEndVert->pos - m_pBufferData[ext->start_vertex].pos).Length();

					auto fa = &surfaces[i];

					bigx = scale_speed[0];
					bigy = scale_speed[1];

				}


				pVertexes[j].pos[0] = v[0];
				pVertexes[j].pos[1] = v[1];
				pVertexes[j].pos[2] = v[2];
				pVertexes[j].texcoord[0] = bigx;
				pVertexes[j].texcoord[1] = bigy;

				pVertexes[j].normal[0] = ext->normal[0];
				pVertexes[j].normal[1] = ext->normal[1];
				pVertexes[j].normal[2] = ext->normal[2];

				pVertexes[j].speculartexcoord[0] = v[3];
				pVertexes[j].speculartexcoord[1] = v[4];
			}

			if(specular)
			{
				memcpy(&m_pBufferData[iCurVert], &pVertexes[0], sizeof(brushvertex_t));
				iCurVert++;
				memcpy(&m_pBufferData[iCurVert], &pVertexes[1], sizeof(brushvertex_t));
				iCurVert++;
				memcpy(&m_pBufferData[iCurVert], &pVertexes[2], sizeof(brushvertex_t));
				iCurVert++;
			}
			else
			{
				iCurVert += 3;
			}

			for (int j = 0; j < (bp->numverts - 3); j++, v += VERTEXSIZE)
			{
				memcpy(&pVertexes[1], &pVertexes[2], sizeof(brushvertex_t));

				pVertexes[2].pos[0] = v[0];
				pVertexes[2].pos[1] = v[1];
				pVertexes[2].pos[2] = v[2];

				pVertexes[2].texcoord[0] = bigx;
				pVertexes[2].texcoord[1] = bigy;

				pVertexes[2].normal[0] = ext->normal[0];
				pVertexes[2].normal[1] = ext->normal[1];
				pVertexes[2].normal[2] = ext->normal[2];

				pVertexes[2].speculartexcoord[0] = v[3];
				pVertexes[2].speculartexcoord[1] = v[4];

				if (specular)
				{
					memcpy(&m_pBufferData[iCurVert], &pVertexes[0], sizeof(brushvertex_t));
					iCurVert++;
					memcpy(&m_pBufferData[iCurVert], &pVertexes[1], sizeof(brushvertex_t));
					iCurVert++;
					memcpy(&m_pBufferData[iCurVert], &pVertexes[2], sizeof(brushvertex_t));
					iCurVert++;
				}
				else
				{
					iCurVert += 3;
				}
			}
		}
		ext->num_vertexes = iCurVert - ext->start_vertex;
		iCurFace++;
	}


	if (m_pBSPMesh)
		delete m_pBSPMesh;

	m_pBSPMesh = new GL_Mesh(iNumVerts, brushvertex_t::GetAttribLayout(), true, m_pBufferData);
	m_pBSPMesh->SetDrawMode(GL_TRIANGLES);
};

/*
====================
HasDynLights

====================
*/
bool CBSPRenderer::HasDynLights(void)
{
	float time = engine_cl->time;

	if (IsEntityTransparent(m_pCurrentEntity))
		return false;

	if (m_pCvarDynamic->value < 1)
		return false;

	if (!m_pDynLights.empty())
		return true;

	return false;
};

void CBSPRenderer::LoadWADDecals()
{
	bool loaded_valve_decals = false;
	char waddir[64];
	sprintf(waddir, "%s/decals.wad", gEngfuncs.pfnGetGameDirectory());
	int length;
	std::vector<std::byte> wadfile = FileSystem_LoadFileIntoBuffer(waddir, FileContentFormat::Binary);
	if (wadfile.empty())
	{
		strcpy(waddir, "valve/decals.wad");
		wadfile = FileSystem_LoadFileIntoBuffer(waddir, FileContentFormat::Binary);
		loaded_valve_decals = true;
		if (wadfile.empty()) // impossible but whatever
		{
			gEngfuncs.Con_DPrintf("ERROR: FAILED TO GET DECALS WAD!!!");
			return;
		}
	}
	wadinfo_t* pInfo = (wadinfo_t*)wadfile.data();
	if (strncmp("WAD3", pInfo->identification, 4))
	{
		return;
	}

	wadfile_t* pWADFile = &gTextureLoader.m_pWADFiles[gTextureLoader.m_iNumWADFiles];
	gTextureLoader.m_iNumWADFiles++;

	strcpy(pWADFile->wadname, "decals.wad");
	pWADFile->wadfile.insert(pWADFile->wadfile.end(), std::begin(wadfile), std::end(wadfile));
	pWADFile->info = (wadinfo_t*)pWADFile->wadfile.data();

	pWADFile->lumps = new lumpinfo_t[pWADFile->info->numlumps];
	memcpy(pWADFile->lumps, (pWADFile->wadfile.data() + pWADFile->info->infotableofs), sizeof(lumpinfo_t) * pWADFile->info->numlumps);
	pWADFile->numlumps = pWADFile->info->numlumps;

	if (!loaded_valve_decals)
	{
		strcpy(waddir, "valve/decals.wad");
		wadfile = FileSystem_LoadFileIntoBuffer(waddir, FileContentFormat::Binary);
		loaded_valve_decals = true;

		wadfile_t* pWADFile = &gTextureLoader.m_pWADFiles[gTextureLoader.m_iNumWADFiles];
		gTextureLoader.m_iNumWADFiles++;

		strcpy(pWADFile->wadname, "decals.wad");
		pWADFile->wadfile.insert(pWADFile->wadfile.end(), std::begin(wadfile), std::end(wadfile));
		pWADFile->info = (wadinfo_t*)pWADFile->wadfile.data();

		pWADFile->lumps = new lumpinfo_t[pWADFile->info->numlumps];
		memcpy(pWADFile->lumps, (pWADFile->wadfile.data() + pWADFile->info->infotableofs), sizeof(lumpinfo_t) * pWADFile->info->numlumps);
		pWADFile->numlumps = pWADFile->info->numlumps;
	}
}

/*
====================
ClearSurfaceDrawChain

====================
*/
void CBSPRenderer::ClearSurfaceDrawChain(void)
{
	for (int i = 0; i < m_iNumTextures; i++)
	{
		m_pNormalTextureList[i].texturechain = NULL;
	}
};

/*
====================
DrawNormalTriangles

====================
*/
void CBSPRenderer::DrawNormalTriangles(bool draw_world)
{
	DrawSky();
	
	if (draw_world)
		DrawWorld();
	
	DrawDecals();
};

/*
====================
DrawNormalTriangles

====================
*/
void CBSPRenderer::DrawNormalTriangles_Cheap(bool drawworld, bool draw_ents)
{
	if (m_bDrawSky)
	{
		s_SkyboxShader.Bind();
		s_pSkyboxMesh->BindMesh();

		glm::mat4 viewrotation = m_ViewMatrix;
		viewrotation[3][0] = viewrotation[3][1] = viewrotation[3][2] = 0;

		s_SkyboxShader.UniformMatrix4fv(eSimpleSkyboxShader_locs[skybox_projviewmatrix], 1, GL_FALSE, glm::value_ptr(m_ProjectionMatrix * viewrotation));
		s_SkyboxShader.Uniform1i(eSimpleSkyboxShader_locs[skybox_skyfog], gHUD.m_pFogSettings.affectsky);
		s_SkyboxShader.Uniform3fv(eSimpleSkyboxShader_locs[skybox_fogcolor], 1, gHUD.m_pFogSettings.color);

		for (int i = 0; i < 6; i++)
		{
			BindGLTexture(GL_TEXTURE0, m_iSkyTextures[i]);
			s_pSkyboxMesh->DrawArrays(i * 6, 6);
		}

		glClear(GL_DEPTH_BUFFER_BIT);
	}

	if (m_pCvarDrawWorld->value && drawworld)
	{
		VectorCopy(m_vRenderOrigin, m_vVecToEyes);

		m_pBSPMesh->BindMesh();

		s_BSPShader.Bind();

		ClearSurfaceDrawChain();

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_2D, m_iEngineLightmapIndex);

		RecursiveWorldNode(BSPWorld_Model::m_pWorldNodes);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		s_BSPShader.Uniform1i(eWorldShader_locs[world_fog_active], gl_fog->value ? gHUD.m_pFogSettings.active : false);

		s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_projectionmatrix], 1, GL_FALSE, glm::value_ptr(m_ProjectionMatrix));
		s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_viewmatrix], 1, GL_FALSE, glm::value_ptr(m_ViewMatrix));
		s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));

		s_BSPShader.Uniform3fv(eWorldShader_locs[world_renderorigin], 1, m_vRenderOrigin);
		s_BSPShader.Uniform3fv(eWorldShader_locs[world_fogcolor], 1, gHUD.m_pFogSettings.color);
		s_BSPShader.Uniform3i(eWorldShader_locs[world_rendercolor], 255, 255, 255);

		s_BSPShader.Uniform1f(eWorldShader_locs[world_fogstart], gHUD.m_pFogSettings.start);
		s_BSPShader.Uniform1f(eWorldShader_locs[world_fogend], gHUD.m_pFogSettings.end);

		float texgamma_val = 1.2 - (texgamma->value - 1.8); //cause goldsrc limits it to 1.8
		float lightgamma_val = 1.2 - (lightgamma->value - 1.8);

		s_BSPShader.Uniform1f(eWorldShader_locs[world_texgamma], texgamma_val);
		s_BSPShader.Uniform1f(eWorldShader_locs[world_lightgamma], lightgamma_val);

		s_BSPShader.Uniform1i(eWorldShader_locs[world_renderamt], 255);

		s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 1);
		s_BSPShader.Uniform1i(eWorldShader_locs[world_texture_pass], 0);

		BindGLTexture(LIGHTMAP_TEXUNIT, m_iEngineLightmapIndex);

		// Render normal ones first
		for (int i = 0; i < m_iNumTextures; i++)
		{
			texture_t* pTexture = &m_pNormalTextureList[i];

			clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

			// Nothing to draw
			if (!psurface)
				continue;

			while (psurface)
			{
				int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
				brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

				if (psurface->flags & SURF_DRAWTURB)
				{
					psurface = psurface->texturechain;
					continue;
				}

				multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
				multidraw_numverts[num_multidraws] = pbrushface->num_vertexes;
				num_multidraws++;

				psurface = psurface->texturechain;
			}

			if (!num_multidraws)
				continue;
			
			m_pBSPMesh->MultiDrawArrays(multidraw_startverts, multidraw_numverts, num_multidraws);

			num_multidraws = 0;

		}
		DrawDynamicLightsForWorld();
		s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 0);
		s_BSPShader.Uniform1i(eWorldShader_locs[world_texture_pass], 1);
		// Render normal ones first
		for (int i = 0; i < m_iNumTextures; i++)
		{
			texture_t* pTexture = &m_pNormalTextureList[i];
			clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

			// Nothing to draw
			if (!psurface)
				continue;

			while (psurface)
			{
				int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
				brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

				if (psurface->flags & SURF_DRAWTURB)
				{
					psurface = psurface->texturechain;
					continue;
				}

				multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
				multidraw_numverts[num_multidraws] = pbrushface->num_vertexes;
				num_multidraws++;

				m_iBSPVertsCounter += pbrushface->num_vertexes;

				psurface = psurface->texturechain;
			}

			if (!num_multidraws)
				continue;

			BindGLTexture(SURFTEXTURE_TEXUNIT, pTexture->gl_texturenum);
			m_pBSPMesh->MultiDrawArrays(multidraw_startverts, multidraw_numverts, num_multidraws);

			num_multidraws = 0;

		}

		if (draw_ents)
		{
			for (int i = 0; i < m_iNumRenderEntities; i++)
			{
				cl_entity_t* ent = m_pRenderEntities[i];
				if (IsEntityMoved(ent) && !IsEntityTransparent(ent))
					DrawBrushModel(ent, false);
			}
		}
	}

	g_GlobalGLState.SetBlend(false);

	GL_ShaderProgram::ResetShaderBind();
	GL_Mesh::UnbindMesh();
};


/*
====================
DrawTransparentTriangles

====================
*/
void CBSPRenderer::DrawTransparentTriangles(void)
{
	if (!m_pCvarDrawWorld->value || !g_StudioRenderer.m_pCvarDrawEntities->value)
	{
		return;
	}

	for (int i = 0; i <= m_iNumRenderEntities; i++)
	{
		if (i == m_iNumRenderEntities)
			return; //jump away from this function

		cl_entity_t* ent = m_pRenderEntities[i];
		if (IsEntityTransparent(ent))
			break;
	}

	m_pCurrentEntity = gEngfuncs.GetEntityByIndex(0);
	
	s_BSPShader.Bind();
	
	m_pBSPMesh->BindMesh();
	
	for (int i = 0; i < m_iNumRenderEntities; i++)
	{
		cl_entity_t* ent = m_pRenderEntities[i];
		if (IsEntityTransparent(ent))
			DrawBrushModel(ent, false);
	}

	DrawDecals(true);

	GL_VertexArrayObject::ResetVAOBinding();
};

/*
====================
DrawWorld

====================
*/
void CBSPRenderer::DrawWorld(bool m_bSkyBox)
{
	if (!m_pCvarDrawWorld->value)
	{
		return;
	}

	VectorCopy(m_vRenderOrigin, m_vVecToEyes);
	
	m_pBSPMesh->BindMesh();

	s_BSPShader.Bind();

	ClearSurfaceDrawChain();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D, m_iEngineLightmapIndex);

	RecursiveWorldNode(BSPWorld_Model::m_pWorldNodes);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	
	s_BSPShader.Uniform1i(eWorldShader_locs[world_fog_active], gl_fog->value ? gHUD.m_pFogSettings.active : false);
	
	
	s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_projectionmatrix], 1, GL_FALSE, glm::value_ptr(m_ProjectionMatrix));
	s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_viewmatrix], 1, GL_FALSE, glm::value_ptr(m_ViewMatrix));
	s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));
	
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_renderorigin], 1, m_vRenderOrigin);
	//s_BSPShader.Uniform3fv(eWorldShader_locs[world_renderright], 1, m_RefParams.right);
	//s_BSPShader.Uniform3fv(eWorldShader_locs[world_renderforward], 1, m_RefParams.forward);
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_fogcolor], 1, gHUD.m_pFogSettings.color);
	s_BSPShader.Uniform3i(eWorldShader_locs[world_rendercolor], 255, 255, 255);
	
	s_BSPShader.Uniform1f(eWorldShader_locs[world_fogstart], gHUD.m_pFogSettings.start);
	s_BSPShader.Uniform1f(eWorldShader_locs[world_fogend], gHUD.m_pFogSettings.end);

	s_BSPShader.Uniform1f(eWorldShader_locs[world_fltime], engine_cl->time);
	
	float texgamma_val = 1.2 - (texgamma->value - 1.8); //cause goldsrc limits it to 1.8
	float lightgamma_val = 1.2 - (lightgamma->value - 1.8);
	
	s_BSPShader.Uniform1f(eWorldShader_locs[world_texgamma], texgamma_val);
	s_BSPShader.Uniform1f(eWorldShader_locs[world_lightgamma], lightgamma_val);

	s_BSPShader.Uniform1i(eWorldShader_locs[world_sunshadow_fadedist], m_iSunShadow_FadeDist);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_sunshadow_strength], m_iSunShadow_Strength);
	
	s_BSPShader.Uniform1i(eWorldShader_locs[world_renderamt], 255);

	//Draw all static entities
	if (!m_bSkyBox)
	{
		for (int i = 0; i < m_iNumRenderEntities; i++)
		{
			cl_entity_t* ent = m_pRenderEntities[i];
			if (!IsEntityMoved(ent) && !IsEntityTransparent(ent))
				DrawBrushModel(ent, true);
		}
	}

	m_pCurrentEntity = gEngfuncs.GetEntityByIndex(0);

	//render lightmap only
	if (!r_fullbright->value)
	{
		RenderFirstPass();
		// render spotlights and shadows
		DrawDynamicLightsForWorld();
	}
	//render textures and multiply it with the lightmap on the scene

	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	RenderFinalPasses();

	//for debugging
	RenderWireframe();

	// Now render moved entities seperately each
	if (!m_bSkyBox)
	{
		for (int i = 0; i < m_iNumRenderEntities; i++)
		{
			cl_entity_t* ent = m_pRenderEntities[i];
			if (IsEntityMoved(ent) && !IsEntityTransparent(ent))
				DrawBrushModel(ent, false);
		}
	}

	GL_Mesh::UnbindMesh();
};


void CBSPRenderer::SetClippingPlane(const mplane_t& plane)
{
	s_BSPShader.Bind();
	s_BSPShader.Uniform4fv(eWorldShader_locs[world_clipplane], 1, glm::value_ptr(glm::vec4(plane.normal.x, plane.normal.y, plane.normal.z, plane.dist)));
}



//transform a point in world space to screen space

Vector CBSPRenderer::TriWorldToScreen(Vector point)
{
	Vector screen(0, 0, 0);

	glm::vec4 worldPos(point.x, point.y, point.z, 1.0f);

	// clip space
	glm::vec4 clip = m_ProjectionMatrix * m_ViewMatrix * worldPos;

	// perspective divide -> NDC (-1..1)
	glm::vec3 ndc = glm::vec3(clip) / clip.w;

	// to screen coords (pixels)
	float screenX = (ndc.x * 0.5f + 0.5f) * ScreenWidth;
	float screenY = (ndc.y * 0.5f + 0.5f) * ScreenHeight;

	return Vector(screenX / ScreenWidth, screenY / ScreenHeight, 0);
}

/*
====================
RenderFirstPass

====================
*/
void CBSPRenderer::RenderFirstPass()
{
	if (r_fullbright->value)
		return;

	BindGLTexture(LIGHTMAP_TEXUNIT, m_iEngineLightmapIndex);

	s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 1);

	s_BSPShader.Uniform1i(eWorldShader_locs[world_texture_pass], 0);

	// Render normal ones first
	std::vector<int> alphatextures;
	for (int i = 0; i < m_iNumTextures; i++)
	{
		texture_t* pTexture = TextureAnimation(&m_pNormalTextureList[i], m_pCurrentEntity->curstate.frame);
		clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

		// bacontsu - fake specular
		bool specular = pTexture->texture_flag & TEXTURE_SPECULAR;
		bool alphatest = pTexture->name[0] == '{';

		if (specular || alphatest || !psurface)
		{
			if (alphatest)
				alphatextures.push_back(i);
			continue;
		}

		// Nothing to draw
		if (!psurface)
			continue;

		while (psurface)
		{
			int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
			brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

			if (!(psurface->flags & SURF_DRAWTURB))
			{
				multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
				multidraw_numverts[num_multidraws] = (pbrushface->num_vertexes);
				num_multidraws++;
			}

			m_iBSPVertsCounter += pbrushface->num_vertexes;

			psurface = psurface->texturechain;
		}

	}

	m_pBSPMesh->MultiDrawArrays(multidraw_startverts, multidraw_numverts, num_multidraws);
	num_multidraws = 0;

	if (alphatextures.empty())
		return;

	s_BSPShader.Uniform1i(eWorldShader_locs[world_alphatest], 1);

	// now render special textures
	for (int i = 0; i < alphatextures.size(); i++)
	{
		int index = alphatextures[i];
		texture_t* pTexture = TextureAnimation(&m_pNormalTextureList[index], m_pCurrentEntity->curstate.frame);
		clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[index].texturechain;
		BindGLTexture(SURFTEXTURE_TEXUNIT, pTexture->gl_texturenum);

		while (psurface)
		{
			int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
			brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

			multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
			multidraw_numverts[num_multidraws] = (pbrushface->num_vertexes);
			num_multidraws++;

			m_iBSPVertsCounter += pbrushface->num_vertexes;

			psurface = psurface->texturechain;
		}

		m_pBSPMesh->MultiDrawArrays(multidraw_startverts, multidraw_numverts, num_multidraws);
		num_multidraws = 0;
	}

	s_BSPShader.Uniform1i(eWorldShader_locs[world_alphatest], 0);
}

/*
====================
RenderFinalPasses

====================
*/
void CBSPRenderer::RenderFinalPasses()
{
	if (m_pCvarLightmapDebug->value)
	{
		g_GlobalGLState.SetBlend(false);
		return;
	}
	
	if(!r_fullbright->value)
		g_GlobalGLState.SetBlend(true);

	s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 0);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_texture_pass], 1);

	// Render normal ones first
	for (int i = 0; i < m_iNumTextures; i++)
	{
		texture_t* pTexture = TextureAnimation(&m_pNormalTextureList[i], m_pCurrentEntity->curstate.frame);
		clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

		// Nothing to draw
		if (!psurface)
			continue;

		BindGLTexture(SURFTEXTURE_TEXUNIT, pTexture->gl_texturenum);

		bool scrollingpoly = pTexture->texture_flag & TEXTURE_SCROLL;

		// bacontsu - fake specular
		bool specular = pTexture->texture_flag & TEXTURE_SPECULAR;

		if (scrollingpoly)
		{
			s_BSPShader.Uniform1i(eWorldShader_locs[world_scrollingpolys], 1);
		}
		else if (specular)
		{
			g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			s_BSPShader.Uniform1i(eWorldShader_locs[world_specular], 1);

		}

		while (psurface)
		{
			int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
			brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

			if (psurface->flags & SURF_DRAWTURB)
			{
				s_BSPShader.Uniform1i(eWorldShader_locs[world_waterpolys], 1);
					glDisable(GL_CULL_FACE);
					glDrawArrays(GL_TRIANGLES, pbrushface->start_vertex, pbrushface->num_vertexes);
					glEnable(GL_CULL_FACE);
				s_BSPShader.Uniform1i(eWorldShader_locs[world_waterpolys], 0);
			}
			else
			{
				multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
				multidraw_numverts[num_multidraws] = (pbrushface->num_vertexes);
				num_multidraws++;
			}

			m_iBSPVertsCounter += pbrushface->num_vertexes;

			psurface = psurface->texturechain;
		}

		m_pBSPMesh->MultiDrawArrays(multidraw_startverts, multidraw_numverts, num_multidraws);
		num_multidraws = 0;

		if (scrollingpoly)
		{
			s_BSPShader.Uniform1i(eWorldShader_locs[world_scrollingpolys], 0);
		}
		else if (specular)
		{
			g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			s_BSPShader.Uniform1i(eWorldShader_locs[world_specular], 0);
		}
	}

	if (!r_fullbright->value)
	{
		g_GlobalGLState.SetBlend(false);
	}
}

void CBSPRenderer::RenderWireframe()
{
	if(!m_pCvarWireFrame->value)
		return;

	bool nodepth = m_pCvarWireFrame->value > 1 ? true : false;

	g_GlobalGLState.SetBlend(false);

	if (nodepth)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		g_GlobalGLState.SetDepthTest(false);
	}

	s_BSPShader.Uniform1i(eWorldShader_locs[world_wireframe], 1);

	for (int i = 0; i < m_iNumTextures; i++)
	{
		clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

		// Nothing to draw
		if (!psurface)
			continue;

		while (psurface)
		{
			int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
			brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

			multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
			multidraw_numverts[num_multidraws] = pbrushface->num_vertexes;
			num_multidraws++;

			psurface = psurface->texturechain;
			m_iBSPVertsCounter += pbrushface->num_vertexes;
		}
	}

	glMultiDrawArrays(GL_LINE_LOOP, (GLint*)multidraw_startverts, (GLint*)multidraw_numverts, num_multidraws);
	num_multidraws = 0;

	s_BSPShader.Uniform1i(eWorldShader_locs[world_wireframe], 0);

	if (nodepth)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		g_GlobalGLState.SetDepthTest(true);
	}

}

/*
====================
RecursiveWorldNode

====================
*/
void CBSPRenderer::RecursiveWorldNode(clientmnode_t* node)
{
	unsigned short c, side;
	mplane_t* plane;
	clientmsurface_t *surf, **mark;
	clientmleaf_t* pleaf;
	float dot;

	while (true)
	{

		if (node->contents == CONTENTS_SOLID)
			return; // solid

		if (node->visframe != r_visframecount && r_visframecount != -2)
			return;

		if (gHUD.viewFrustum.CullBox(node->minmaxs, node->minmaxs + 3))
			return;

		// if a leaf node, draw stuff
		if (node->contents < 0)
		{
			pleaf = (clientmleaf_t*)node;
			mark = pleaf->firstmarksurface;
			c = pleaf->nummarksurfaces;

			if (c)
			{
				do
				{
					(*mark)->visframe = m_iFrameCount;
					mark++;
				} while (--c);
			}
			return;
		}

		// node is just a decision point, so go down the apropriate sides
		// find which side of the node we are on
		plane = node->plane;
		if (plane->type <= PLANE_Z)
		{
			dot = m_vRenderOrigin[plane->type] - plane->dist;
		}
		else
		{
			dot = DotProduct(m_vRenderOrigin, plane->normal) - plane->dist;
		}

		side = dot >= 0 ? 0 : 1;

		// recurse down the children, front side first
		RecursiveWorldNode(node->children[side]);

		// draw stuff
		c = node->numsurfaces;
		if (c)
		{
			surf = BSPWorld_Model::m_pWorldSurfaces + node->firstsurface;

			for (int i = 0; i < node->numsurfaces; i++, surf++)
			{
				if (surf->visframe != m_iFrameCount)
					continue;

				// don't backface underwater surfaces, because they warp
				if (!(surf->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
					continue; // wrong side

				if (surf->flags & SURF_DRAWSKY)
					continue;

				SurfaceToChain(BSPWorld_Model::m_pWorldSurfaces, surf);
			}
		}

		// recurse down the back side
		// NOTE: With this while loop, this is identical to just calling
		// RecursiveWorldNode(node->children[!side]);
		node = node->children[!side];

	}
};


/*
====================
DrawBrushModel

====================
*/
void CBSPRenderer::DrawBrushModel(cl_entity_t* pEntity, bool bStatic)
{
	Vector mins, maxs;
	int i;
	clientmsurface_t* psurf;
	float dot;
	mplane_t* pplane;
	Vector trans;
	Vector rorigin;
	bool bRotated = false;

	m_pCurrentEntity = pEntity;
	model_t* pModel = m_pCurrentEntity->model;

	if (m_pCurrentEntity->model == engine_cl->worldmodel || m_pCurrentEntity->model->type != mod_brush)
		return;

	if (m_pCurrentEntity->curstate.rendermode != NULL && m_pCurrentEntity->curstate.renderamt == NULL)
		return;

	if (m_pCurrentEntity->angles[0] || m_pCurrentEntity->angles[1] || m_pCurrentEntity->angles[2])
	{
		bRotated = true;
		for (i = 0; i < 3; i++)
		{
			mins[i] = m_pCurrentEntity->origin[i] - pModel->radius;
			maxs[i] = m_pCurrentEntity->origin[i] + pModel->radius;
		}
	}
	else
	{
		VectorAdd(m_pCurrentEntity->origin, pModel->mins, mins);
		VectorAdd(m_pCurrentEntity->origin, pModel->maxs, maxs);
	}

	if (gHUD.viewFrustum.CullBox(mins, maxs))
		return;

	VectorSubtract(m_vRenderOrigin, m_pCurrentEntity->origin, m_vVecToEyes);

	if (bRotated)
	{
		Vector temp;
		Vector forward, right, up;

		VectorCopy(m_vVecToEyes, temp);
		AngleVectors(m_pCurrentEntity->angles, &forward, &right, &up);
		m_vVecToEyes[0] = DotProduct(temp, forward);
		m_vVecToEyes[1] = DotProduct(temp, right);
		m_vVecToEyes[2] = DotProduct(temp, up);
		m_vVecToEyes[1] = -m_vVecToEyes[1];
	}

	glm::mat4 oldmodelmatrix = m_ModelMatrix;

	if (!bStatic)
	{
		glm::vec3 entityangles = glm::vec3(m_pCurrentEntity->angles.x, m_pCurrentEntity->angles.y, m_pCurrentEntity->angles.z);

		glm::mat4  modelview = glm::translate(glm::mat4(1.0f), glm::vec3(m_pCurrentEntity->origin.x, m_pCurrentEntity->origin.y, m_pCurrentEntity->origin.z));
		modelview = glm::rotate(modelview, glm::radians(entityangles.y), glm::vec3(0.0f, 0.0f, 1.0f));
		modelview = glm::rotate(modelview, glm::radians(entityangles.x), glm::vec3(0.0f, 1.0f, 0.0f));
		modelview = glm::rotate(modelview, glm::radians(entityangles.z), glm::vec3(1.0f, 0.0f, 0.0f));

		m_ModelMatrix = modelview;

		ClearSurfaceDrawChain();
	}

	int alpha = (float)m_pCurrentEntity->curstate.renderamt;
	int r = (float)m_pCurrentEntity->curstate.rendercolor.r;
	int g = (float)m_pCurrentEntity->curstate.rendercolor.g;
	int b = (float)m_pCurrentEntity->curstate.rendercolor.b;

	if(!r && !g && !b)
	{
		alpha = alpha ? alpha : 255;
		r = r ? r : 255;
		g = g ? g : 255;
		b = b ? b : 255;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D, m_iEngineLightmapIndex);

	psurf = &BSPWorld_Model::m_pWorldSurfaces[pModel->firstmodelsurface];

	bool shoulddraw = false;

	for (i = 0; i < pModel->nummodelsurfaces; i++, psurf++)
	{
		pplane = psurf->plane;
		DotProductSub(&dot, &m_vVecToEyes, &pplane->normal, &pplane->dist);

		auto iswater = (psurf->flags & SURF_DRAWTURB);

		if (iswater || ((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (iswater && (psurf->plane->normal[2] != 1 || psurf->flags & SURF_PLANEBACK))
				continue;

			psurf->visframe = m_iFrameCount;

			if (psurf->flags & SURF_DRAWSKY)
				continue;

			SurfaceToChain(BSPWorld_Model::m_pWorldSurfaces, psurf);
			shoulddraw = true;
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	if (!shoulddraw)
	{
		m_ModelMatrix = oldmodelmatrix;

		m_pCurrentEntity->visframe = m_iFrameCount;
		m_pCurrentEntity = nullptr;
		return;
	}

	//
	// Render every pass
	//
	if (!bStatic)
	{
		s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));

		s_BSPShader.Uniform1i(eWorldShader_locs[world_renderamt], alpha);
		s_BSPShader.Uniform3i(eWorldShader_locs[world_rendercolor], r, g, b);

		if (m_pCurrentEntity->curstate.rendermode == kRenderTransAdd)
		{
			g_GlobalGLState.SetDepthWrite(false);
			g_GlobalGLState.SetBlend(true);
			g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		else if (m_pCurrentEntity->curstate.rendermode == kRenderTransTexture)
		{
			g_GlobalGLState.SetDepthWrite(false);
			g_GlobalGLState.SetBlend(true);
			g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else if (m_pCurrentEntity->curstate.rendermode == kRenderTransColor)
		{
			g_GlobalGLState.SetBlend(true);
			g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
		{
			g_GlobalGLState.SetBlend(false);
		}

		if(m_pCurrentEntity->curstate.rendermode != kRenderNormal && m_pCurrentEntity->curstate.rendermode != kRenderTransAlpha)
		{
			if(m_pCurrentEntity->curstate.rendermode == kRenderTransAdd)
				s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 0);
			else
				s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 1);

			s_BSPShader.Uniform1i(eWorldShader_locs[world_texture_pass], 1);

			// Render normal ones first
			for (int i = 0; i < m_iNumTextures; i++)
			{
				texture_t* pTexture = TextureAnimation(&m_pNormalTextureList[i], m_pCurrentEntity->curstate.frame);
				clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

				// Nothing to draw
				if (!psurface)
					continue;

				BindGLTexture(SURFTEXTURE_TEXUNIT, pTexture->gl_texturenum);

				auto scrollingpoly = pTexture->texture_flag & TEXTURE_SCROLL;

				// bacontsu - fake specular
				auto specular = pTexture->texture_flag & TEXTURE_SPECULAR;

				if (scrollingpoly)
				{
					s_BSPShader.Uniform1i(eWorldShader_locs[world_scrollingpolys], 1);
				}
				else if (specular)
				{
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					s_BSPShader.Uniform1i(eWorldShader_locs[world_specular], 1);
					s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 0);

				}

				while (psurface)
				{
					int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
					brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

					if (psurface->flags & SURF_DRAWTURB)
					{
						s_BSPShader.Uniform1i(eWorldShader_locs[world_waterpolys], 1);
						glDisable(GL_CULL_FACE);
						glDrawArrays(GL_TRIANGLES, pbrushface->start_vertex, pbrushface->num_vertexes);
						glEnable(GL_CULL_FACE);
						s_BSPShader.Uniform1i(eWorldShader_locs[world_waterpolys], 0);
					}
					else
					{
						multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
						multidraw_numverts[num_multidraws] = (pbrushface->num_vertexes);
						num_multidraws++;
					}

					m_iBSPVertsCounter += pbrushface->num_vertexes;

					psurface = psurface->texturechain;
				}

				glMultiDrawArrays(GL_TRIANGLES, (GLint*)multidraw_startverts, (GLint*)multidraw_numverts, num_multidraws);
				num_multidraws = 0;

				if (scrollingpoly)
				{
					s_BSPShader.Uniform1i(eWorldShader_locs[world_scrollingpolys], 0);
				}
				else if (specular)
				{
					g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
					s_BSPShader.Uniform1i(eWorldShader_locs[world_specular], 0);
					s_BSPShader.Uniform1i(eWorldShader_locs[world_lightmap_pass], 1);
				}
			}
		}
		else
		{
			RenderFirstPass();

			DrawDynamicLightsForEntity(m_pCurrentEntity);

			RenderFinalPasses();
		}

		s_BSPShader.Uniform1i(eWorldShader_locs[world_renderamt], 255);
		s_BSPShader.Uniform3i(eWorldShader_locs[world_rendercolor], 255, 255, 255);

		m_ModelMatrix = oldmodelmatrix;

		s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));

		g_GlobalGLState.SetDepthWrite(true);
	}

	m_pCurrentEntity->visframe = m_iFrameCount;
	m_pCurrentEntity = nullptr;
}

/*
====================
DrawPolyFromArray

====================
*/
void CBSPRenderer::DrawPolyFromArray(clientmsurface_t* psurfbase, clientmsurface_t* psurf)
{
	int surfaceIndex = psurf - psurfbase;
	brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

	m_pBSPMesh->DrawArrays(pbrushface->start_vertex, pbrushface->num_vertexes);

	m_iBSPVertsCounter += pbrushface->num_vertexes;
}

/*
====================
SurfaceToChain

====================
*/
void CBSPRenderer::SurfaceToChain(clientmsurface_t* psurfbase, clientmsurface_t* s)
{
	int surfaceIndex = s - psurfbase;
	brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

	if (!m_pSurfaces[pbrushface->index].regtexture)
		return;

	//	this is horrid.
	//	the most efficient way to do this would be to make a glsl shader that does what the code below does, so there's no cpu -> gpu data transfering.
	//	not sure when i'll do that

	if (!(s->flags & SURF_DRAWTURB))
	{
		for (int i = 0; i < MAXLIGHTMAPS; i++)
		{
			if (s->styles[i] == 255)
				break;

			if (m_iLightStyleValue[s->styles[i]] != m_pSurfaces[pbrushface->index].cached_light[i])
			{
				BuildLightmap(s, surfaceIndex, m_pEngineLightmaps);

				int smax = (s->extents[0] >> 4) + 1;
				int tmax = (s->extents[1] >> 4) + 1;

				glTexSubImage2D(GL_TEXTURE_2D, 0, m_pSurfaces[pbrushface->index].light_s, m_pSurfaces[pbrushface->index].light_t, smax, tmax, GL_RGB, GL_UNSIGNED_BYTE, m_pBlockLights);
				break;
			}
		}
	}

	s->texturechain = (clientmsurface_t*)m_pSurfaces[pbrushface->index].regtexture->texturechain;
	m_pSurfaces[pbrushface->index].regtexture->texturechain = (msurface_t*)s;
};

/*
====================
AnimateLight

====================
*/
void CBSPRenderer::AnimateLight(void)
{
	int i, j, k;

	if (!m_pCvarLightStyles->value)
		return;

	i = (int)(engine_cl->time * 10);
	for (j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!m_pLightStyles[j].length)
		{
			m_iLightStyleValue[j] = 256;
			continue;
		}
		k = i % m_pLightStyles[j].length;
		k = m_pLightStyles[j].map[k] - 'a';
		k = k * 22;
		m_iLightStyleValue[j] = k;
	}
}

/*
====================
BuildLightmap

====================
*/
void CBSPRenderer::BuildLightmap(clientmsurface_t* surf, int surfindex, color24* out)
{
	const int smax = (surf->extents[0] >> 4) + 1;
	const int tmax = (surf->extents[1] >> 4) + 1;
	const int size = smax * tmax;

	memset(m_pBlockLights, 0, sizeof(color24) * size);

	if (surf->samples && size <= BLOCKLIGHTS_SIZE && surf->styles[0] != 255)
	{
		color24* lightmap = surf->samples;

		for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; ++maps)
		{
			const int style = surf->styles[maps];
			int styleValue = m_iLightStyleValue[style];

			m_pSurfaces[surfindex].cached_light[maps] = styleValue;

			const float scale = styleValue / 255.0f;

			for (int i = 0; i < size; ++i)
			{
				const color24& sample = lightmap[i];
				color24& dst = m_pBlockLights[i];

				dst.r = std::min(255, dst.r + int(sample.r * scale));
				dst.g = std::min(255, dst.g + int(sample.g * scale));
				dst.b = std::min(255, dst.b + int(sample.b * scale));
			}

			lightmap += size;
		}
	}

	for (int i = 0; i < size; ++i)
	{
		color24& c = m_pBlockLights[i];

		float intensity = (c.r + c.g + c.b);
		intensity = std::min(1.0f, intensity);

		c.r = c.r * intensity;
		c.g = c.g * intensity;
		c.b = c.b * intensity;
	}

	const int column = surf->lightmaptexturenum % LIGHTMAP_NUMROWS;
	const int row = (surf->lightmaptexturenum / LIGHTMAP_NUMROWS) % LIGHTMAP_NUMCOLUMNS;

	for (int i = 0; i < tmax; ++i)
	{
		color24* src = &m_pBlockLights[i * smax];
		color24* dest = out + BLOCK_SIZE * BLOCK_SIZE * LIGHTMAP_NUMCOLUMNS * row + BLOCK_SIZE * column;
		dest += ((BLOCK_SIZE * LIGHTMAP_NUMCOLUMNS * surf->light_t) + surf->light_s) + (BLOCK_SIZE * LIGHTMAP_NUMCOLUMNS * i);

		memcpy(dest, src, sizeof(color24) * smax);
	}
}

/*
====================
UploadLightmaps

====================
*/
void CBSPRenderer::UploadLightmaps(void)
{
	std::fill(std::begin(m_iLightStyleValue), std::end(m_iLightStyleValue), 255); //dont set lightstyles to 0, cause we'll have to make the lightmaps in a bit
	memset(m_pEngineLightmaps, 0, sizeof(m_pEngineLightmaps));
	m_iNumLightmaps = NULL;

	//
	// Allocate all surface infos
	//
	m_pSurfaces = new clientsurfdata_t[(engine_cl->worldmodel->numsurfaces)]();
	// memset(m_pSurfaces, NULL, sizeof(clientsurfdata_t) * (m_pWorld->numsurfaces + m_iNumDetailSurfaces));
	m_iNumSurfaces = engine_cl->worldmodel->numsurfaces;

	if (engine_cl->worldmodel->lightdata)
	{
		//
		// Convert and merge lightmaps into one
		//

		for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++)
		{
			clientmsurface_t* pSurf = &BSPWorld_Model::m_pWorldSurfaces[i];

			if (pSurf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
				continue;

			BuildLightmap(pSurf, i, m_pEngineLightmaps);

			const int column = pSurf->lightmaptexturenum % LIGHTMAP_NUMROWS;
			const int row = (pSurf->lightmaptexturenum / LIGHTMAP_NUMROWS) % LIGHTMAP_NUMCOLUMNS;

			m_pSurfaces[i].light_s = pSurf->light_s + BLOCK_SIZE * column;
			m_pSurfaces[i].light_t = pSurf->light_t + BLOCK_SIZE * row;

			if (pSurf->lightmaptexturenum > m_iNumLightmaps)
				m_iNumLightmaps = pSurf->lightmaptexturenum;
		}
	}
	else
	{
		//
		// Count lightmaps anyway
		//
		for (int i = 0; i < engine_cl->worldmodel->numsurfaces; i++)
		{
			clientmsurface_t* pSurf = &BSPWorld_Model::m_pWorldSurfaces[i];

			if (pSurf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
				continue;

			if (pSurf->lightmaptexturenum > m_iNumLightmaps)
				m_iNumLightmaps = pSurf->lightmaptexturenum;
		}

		//
		// Fill with half-bright
		//
		memset(m_pEngineLightmaps, 128, sizeof(m_pEngineLightmaps));
	}

	//
	// Upload the large texture
	//
	glBindTexture(GL_TEXTURE_2D, m_iEngineLightmapIndex);
	glTexStorage2D(GL_TEXTURE_2D, 4, GL_RGB8, LIGHTMAP_RESOLUTION, LIGHTMAP_RESOLUTION);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LIGHTMAP_RESOLUTION, LIGHTMAP_RESOLUTION, GL_RGB, GL_UNSIGNED_BYTE, m_pEngineLightmaps);
}

/*
====================
BindGLTexture

====================
*/
void CBSPRenderer::BindGLTexture(GLenum texture, GLuint id)
{
	glBindTextureUnit(texture - GL_TEXTURE0, id);
}
/*
====================
TextureAnimation

====================
*/
texture_t* CBSPRenderer::TextureAnimation(texture_t* base, int frame)
{
	int reletive;
	int count;

	if (frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if ((base->name[0] != '+') || (!base->anim_total))
		return base;

	reletive = (int)(engine_cl->time * 20) % base->anim_total;

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			gEngfuncs.Con_Printf("TextureAnimation: broken cycle");
		if (++count > 100)
			gEngfuncs.Con_Printf("TextureAnimation: infinite cycle");
	}

	return base;
}

/*
====================
FindIntersectionPoint

====================
*/
void CBSPRenderer::FindIntersectionPoint(const Vector& p1, const Vector& p2, const Vector& normal, const Vector& planepoint, Vector& newpoint)
{
	Vector planevec;
	Vector linevec;
	float planedist, linedist;

	VectorSubtract(planepoint, p1, planevec);
	VectorSubtract(p2, p1, linevec);

	planedist = DotProduct(normal, planevec);
	linedist = DotProduct(normal, linevec);

	if (linedist != 0)
	{
		VectorMA(p1, planedist / linedist, linevec, newpoint);
		return;
	}
	VectorClear(newpoint);
};

/*
====================
ClipPolygonByPlane

====================
*/
int CBSPRenderer::ClipPolygonByPlane(const Vector* arrIn, int numpoints, Vector normal, Vector planepoint, Vector* arrOut)
{
	int i, cur, prev;
	int first = -1;
	int outCur = 0;
	float dots[64];
	for (i = 0; i < numpoints; i++)
	{
		Vector vecDir;
		VectorSubtract(arrIn[i], planepoint, vecDir);
		dots[i] = DotProduct(vecDir, normal);

		if (dots[i] > 0)
			first = i;
	}

	if (first == -1)
		return 0;

	VectorCopy(arrIn[first], arrOut[outCur]);
	outCur++;

	cur = first + 1;
	if (cur == numpoints)
		cur = 0;

	while (cur != first)
	{
		if (dots[cur] > 0)
		{
			VectorCopy(arrIn[cur], arrOut[outCur]);
			cur++;
			outCur++;

			if (cur == numpoints)
				cur = 0;
		}
		else
			break;
	}

	if (cur == first)
		return outCur;

	if (dots[cur] < 0)
	{
		Vector newpoint;
		if (cur > 0)
			prev = cur - 1;
		else
			prev = numpoints - 1;

		FindIntersectionPoint(arrIn[prev], arrIn[cur], normal, planepoint, newpoint);
		VectorCopy(newpoint, arrOut[outCur]);
	}
	else
	{
		VectorCopy(arrIn[cur], arrOut[outCur]);
	}

	outCur++;
	cur++;

	if (cur == numpoints)
		cur = 0;

	while (dots[cur] < 0)
	{
		cur++;
		if (cur == numpoints)
			cur = 0;
	}

	if (cur > 0)
		prev = cur - 1;
	else
		prev = numpoints - 1;

	if (dots[cur] > 0 && dots[prev] < 0)
	{
		Vector newpoint;
		FindIntersectionPoint(arrIn[prev], arrIn[cur], normal, planepoint, newpoint);
		VectorCopy(newpoint, arrOut[outCur]);
		outCur++;
	}

	while (cur != first)
	{
		VectorCopy(arrIn[cur], arrOut[outCur]);
		cur++;
		outCur++;
		if (cur == numpoints)
			cur = 0;
	}
	return outCur;
}

/*
====================
GetUpRight

====================
*/
void CBSPRenderer::GetUpRight(Vector forward, Vector& up, Vector& right)
{
	VectorClear(up);

	if (forward.x || forward.y)
		up.z = 1;
	else
		up.x = 1;

	right = CrossProduct(forward, up);
	right = right.Normalize();

	up = CrossProduct(forward, right);
	up = up.Normalize();
};

/*
====================
LoadDecalTexture

====================
*/
cl_texture_t* CBSPRenderer::LoadDecalTexture(const char* texname)
{
	char path[256];
	sprintf(path, "gfx/textures/decals/%s.dds", texname);

	cl_texture_t* pTexture = gTextureLoader.LoadTexture(path);

	if (!pTexture)
	{
		gEngfuncs.Con_Printf("BSP Renderer: Missing decal texture %s!\n", path);
		return NULL;
	}

	// ALWAYS Bind
	glBindTexture(GL_TEXTURE_2D, pTexture->iIndex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return pTexture;
};

/*
====================
LoadDecals

====================
*/
void CBSPRenderer::LoadDecals(void)
{
	std::vector<std::byte> textfile = FileSystem_LoadFileIntoBuffer("gfx/textures/decals/decalinfo.txt", FileContentFormat::Text);
	char* pfile = (char*)textfile.data();
	if (textfile.empty())
	{
		gEngfuncs.Con_Printf("BSP Renderer: Cannot open file \"gfx/textures/decals/decalinfo.txt\"\n");
		return;
	}

	int counter = 0;
	char* ptext = pfile;
	while (1)
	{
		// store position where group names recorded
		char* groupnames = ptext;

		// loop until we'll find decal names
		int numgroups = 0;
		char token[256];
		while (1)
		{
			ptext = gEngfuncs.COM_ParseFile(ptext, token);
			if (!ptext)
				goto getout;

			if (token[0] == '{')
				break;

			numgroups++;
		}

		decalgroupentry_t tempentries[MAX_GROUPENTRIES];
		int numtemp = 0;
		while (1)
		{
			char sz_xsize[64];
			char sz_ysize[64];
			ptext = gEngfuncs.COM_ParseFile(ptext, token);

			if (!ptext)
				goto getout;
			if (token[0] == '}')
				break;


			ptext = gEngfuncs.COM_ParseFile(ptext, sz_xsize);

			if (!ptext)
				goto getout;

			ptext = gEngfuncs.COM_ParseFile(ptext, sz_ysize);

			if (!ptext)
				goto getout;

			cl_texture_t* pTexture = LoadDecalTexture(token);

			if (!pTexture)
				continue;

			strcpy(tempentries[numtemp].szName, token);
			tempentries[numtemp].gl_texid = pTexture->iIndex;
			tempentries[numtemp].xsize = atof(sz_xsize) / 2;
			tempentries[numtemp].ysize = atof(sz_ysize) / 2;
			tempentries[numtemp].flags = 0;
			numtemp++;
		}

		// get back to group names
		for (int i = 0; i < numgroups; i++)
		{
			groupnames = gEngfuncs.COM_ParseFile(groupnames, token);
			if (!numtemp)
			{
				gEngfuncs.Con_Printf("BSP Renderer: got empty decal group: %s\n", token);
				continue;
			}

			auto decalgroup = std::make_unique<decalgroup_t>();

			decalgroup->iSize = numtemp;
			strcpy(decalgroup->szName, token);
			memcpy(decalgroup->entries, tempentries, sizeof(tempentries));

			m_pDecalGroups.push_back(std::move(decalgroup));

			counter++;
		}
	}

getout:
	gEngfuncs.Con_Printf("BSP Renderer: %d decal groups created\n", counter);

	// load decals.wad

	m_WADDecals.clear();

	cl_texture_t* pTexture = nullptr;
	for (int i = 0; i < gTextureLoader.m_iNumWADFiles; i++)
	{
		if (!stricmp(gTextureLoader.m_pWADFiles[i].wadname, "decals.wad"))
		{
			byte* pFile = (byte*)gTextureLoader.m_pWADFiles[i].wadfile.data();
			wadinfo_t* pInfo = gTextureLoader.m_pWADFiles[i].info;
			for (int j = 0; j < pInfo->numlumps; j++)
			{
				lumpinfo_t* pLump = &gTextureLoader.m_pWADFiles[i].lumps[j];
				if (pLump->type != 0 && !(pLump->type & 0x43))
					continue;

				strLower(pLump->name);

				pTexture = new cl_texture_t{};

				// Fill in data
				strcpy(pTexture->szName, pLump->name);
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

				byte* pPixels = pFile + pLump->filepos + iIndexOffset;
				gTextureLoader.LoadPallettedTexture(pPixels, pPalette, pTexture, true);

				decalgroupentry_t waddecal;

				strcpy(waddecal.szName, pLump->name);
				waddecal.gl_texid = pTexture->iIndex;
				waddecal.xsize = pTexture->iWidth / 2;
				waddecal.ysize = pTexture->iHeight / 2;
				waddecal.group = nullptr; // unused
				waddecal.flags = DECALFLAG_TRANSDECAL;

				m_WADDecals.push_back(waddecal);
				gTextureLoader.m_pTextures.push_back(pTexture);
			}
		}
	}
};

/*
====================
AllocDecal

====================
*/
customdecal_t* CBSPRenderer::AllocDecal(void)
{
	auto ret = std::make_unique<customdecal_t>();

	if (ret->inumpolys)
	{
		for (int i = 0; i < ret->inumpolys; i++)
			delete[] ret->polys[i].pverts;

		delete[] ret->polys;
	}

	customdecal_t* ptr = ret.get();

	m_pDecals.push_back(std::move(ret));

	return ptr;
}

/*
====================
AllocStaticDecal

====================
*/
customdecal_t* CBSPRenderer::AllocStaticDecal(void)
{
	if (m_pStaticDecals.size() < MAX_STATICDECALS)
	{
		auto ret = std::make_unique<customdecal_t>();

		customdecal_t* ptr = ret.get();

		m_pStaticDecals.push_back(std::move(ret));
		return ptr;
	}
	return NULL;
}

/*
====================
FindDecalByName

====================
*/
decalgroupentry_t* CBSPRenderer::FindDecalByName(const char* szName)
{
	for (int i = 0; i < m_pDecalGroups.size(); i++)
	{
		if (m_pDecalGroups[i]->iSize == 0)
			continue;

		for (int j = 0; j < m_pDecalGroups[i]->iSize; j++)
		{
			if (!strcmp(m_pDecalGroups[i]->entries[j].szName, szName))
				return &m_pDecalGroups[i]->entries[j];
		}
	}
	return NULL;
}

/*
====================
GetRandomDecal

====================
*/
decalgroupentry_t* CBSPRenderer::GetRandomDecal(decalgroup_t* group)
{
	if (group->iSize == 0)
		return NULL;

	if (group->iSize == 1)
		return &group->entries[0];

	int idx = gEngfuncs.pfnRandomLong(0, group->iSize - 1);

	return &group->entries[idx];
}

/*
====================
FindGroup

====================
*/
decalgroup_t* CBSPRenderer::FindGroup(const char* _name)
{
	for (int i = 0; i < m_pDecalGroups.size(); i++)
	{
		if (!strcmp(m_pDecalGroups[i]->szName, _name))
			return m_pDecalGroups[i].get();
	}

	return NULL; // nothing found
}

/*
====================
CullDecalBBox

====================
*/
bool CBSPRenderer::CullDecalBBox(Vector mins, Vector maxs)
{
	if (mins[0] > m_vDecalMaxs[0])
		return true;

	if (mins[1] > m_vDecalMaxs[1])
		return true;

	if (mins[2] > m_vDecalMaxs[2])
		return true;

	if (maxs[0] < m_vDecalMins[0])
		return true;

	if (maxs[1] < m_vDecalMins[1])
		return true;

	if (maxs[2] < m_vDecalMins[2])
		return true;

	return false;
}

/*
====================
CreateDecal

====================
*/
void CBSPRenderer::CreateDecal(Vector endpos, Vector pnormal, const char* name, int persistent, bool fromwad, float angle)
{
	Vector mins, maxs;
	Vector decalpos, decalnormal;
	decalgroupentry_t* pDecalTex = nullptr;

	if (!engine_cl->worldmodel || m_pDecalGroups.empty())
	{
		if (m_pMsgCache.size() >= MAX_DECAL_MSG_CACHE)
			return;

		auto msgcache = std::make_unique<decal_msg_cache>();

		strcpy(msgcache->name, name);
		msgcache->normal = pnormal;
		msgcache->pos = endpos;
		msgcache->persistent = persistent;
		msgcache->fromwad = fromwad;
		msgcache->angle = angle;
		m_pMsgCache.push_back(std::move(msgcache));
		return;
	}

	if (fromwad)
	{
		for (int i  = 0; i < m_WADDecals.size(); i++)
		{
			if (!strcmpi(m_WADDecals[i].szName, name))
			{
				pDecalTex = &m_WADDecals[i];
				break;
			}
		}
	}
	else if (!persistent)
	{
		decalgroup_t* pGroup = FindGroup(name);

		if (!pGroup)
			return;

		pDecalTex = GetRandomDecal(pGroup);
	}
	else
	{
		pDecalTex = FindDecalByName(name);
	}

	if (!pDecalTex)
		return;

	int xsize = pDecalTex->xsize;
	int ysize = pDecalTex->ysize;

	float radius = (xsize > ysize) ? xsize : ysize;

	m_vDecalMins[0] = endpos[0] - radius;
	m_vDecalMins[1] = endpos[1] - radius;
	m_vDecalMins[2] = endpos[2] - radius;
	m_vDecalMaxs[0] = endpos[0] + radius;
	m_vDecalMaxs[1] = endpos[1] + radius;
	m_vDecalMaxs[2] = endpos[2] + radius;

	customdecal_t* pDecal = NULL;
	if (persistent)
	{
		pDecal = AllocStaticDecal();

		if (!pDecal)
			return;
	}
	else
	{
		for (int i = 0; i < m_pDecals.size(); i++)
		{
			if (m_pDecals[i]->texinfo->group != pDecalTex->group)
				continue;

			xsize = m_pDecals[i]->texinfo->xsize;
			ysize = m_pDecals[i]->texinfo->ysize;
			radius = (xsize > ysize) ? xsize : ysize;

			mins[0] = m_pDecals[i]->position[0] - radius;
			mins[1] = m_pDecals[i]->position[1] - radius;
			mins[2] = m_pDecals[i]->position[2] - radius;
			maxs[0] = m_pDecals[i]->position[0] + radius;
			maxs[1] = m_pDecals[i]->position[1] + radius;
			maxs[2] = m_pDecals[i]->position[2] + radius;

			if (!CullDecalBBox(mins, maxs))
			{
				for (int j = 0; j < m_pDecals[i]->inumpolys; j++)
					delete[] m_pDecals[i]->polys[j].pverts;

				delete[] m_pDecals[i]->polys;
				m_pDecals.erase(m_pDecals.begin() + i);
				m_pDecals.push_back(std::move(std::make_unique<customdecal_t>()));
				pDecal = m_pDecals[m_pDecals.size() - 1].get();
				break;
			}
		}

		if (!pDecal)
			pDecal = AllocDecal();
	}

	if (!pDecal)
		return;

	pDecal->texinfo = pDecalTex;
	VectorCopy(endpos, pDecal->position);
	VectorCopy(pnormal, pDecal->normal);

	RecursiveCreateDecal(engine_cl->worldmodel->nodes, pDecalTex, pDecal, endpos, pnormal);

	for (int i = 1; i < MAXRENDERENTS; i++)
	{
		cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(i);

		if (!pEntity)
			break;

		if (!pEntity->model)
			continue;

		if (pEntity->model->type != mod_brush)
			continue;

		if (pEntity->curstate.solid == SOLID_NOT)
			continue;

		if (pEntity->angles[0] || pEntity->angles[1] || pEntity->angles[2])
		{
			mins[0] = pEntity->origin[0] - pEntity->model->radius;
			mins[1] = pEntity->origin[1] - pEntity->model->radius;
			mins[2] = pEntity->origin[2] - pEntity->model->radius;
			maxs[0] = pEntity->origin[0] + pEntity->model->radius;
			maxs[1] = pEntity->origin[1] + pEntity->model->radius;
			maxs[2] = pEntity->origin[2] + pEntity->model->radius;
		}
		else
		{
			mins = pEntity->origin + pEntity->model->mins;
			maxs = pEntity->origin + pEntity->model->maxs;
		}

		if (CullDecalBBox(mins, maxs))
			continue;

		if (pEntity->origin[0] || pEntity->origin[1] || pEntity->origin[2])
		{
			VectorSubtract(endpos, pEntity->origin, decalpos);
			if (pEntity->angles[0] || pEntity->angles[1] || pEntity->angles[2])
			{
				Vector temp, forward, right, up;
				AngleVectors(pEntity->angles, &forward, &right, &up);

				VectorCopy(decalpos, temp);
				decalpos[0] = DotProduct(temp, forward);
				decalpos[1] = -DotProduct(temp, right);
				decalpos[2] = DotProduct(temp, up);

				VectorCopy(pnormal, temp);
				decalnormal[0] = DotProduct(temp, forward);
				decalnormal[1] = -DotProduct(temp, right);
				decalnormal[2] = DotProduct(temp, up);
			}
			else
			{
				VectorSubtract(endpos, pEntity->origin, decalpos);
				VectorCopy(pnormal, decalnormal);
			}
		}
		else
		{
			VectorCopy(pnormal, decalnormal);
			VectorCopy(endpos, decalpos);
		}

		clientmsurface_t* surf = &BSPWorld_Model::m_pWorldSurfaces[pEntity->model->firstmodelsurface];
		for (int k = 0; k < pEntity->model->nummodelsurfaces; k++, surf++)
		{
			float dot;
			mplane_t* pplane = surf->plane;
			DotProductSub(&dot, &decalpos, &pplane->normal, &pplane->dist);

			if (dot < 0)
				dot *= -1;

			if (dot < radius)
			{
				Vector normal = pplane->normal;

				if (surf->flags & SURF_PLANEBACK)
					VectorInverse(normal);

				if (DotProduct(normal, decalnormal) < 0.01)
					continue;

				DecalSurface(surf, pDecalTex, pEntity, pDecal, decalpos, decalnormal);
			}
		}
	}
}

/*
====================
RecursiveCreateDecal

====================
*/
void CBSPRenderer::RecursiveCreateDecal(mnode_t* node, decalgroupentry_t* texptr, customdecal_t* pDecal, Vector endpos, Vector pnormal, float angle)
{
	if (node->contents == CONTENTS_SOLID)
		return; // solid

	if (node->contents < 0)
		return;

	if (CullDecalBBox(node->minmaxs, (node->minmaxs + 3)))
		return;

	int xsize = texptr->xsize;
	int ysize = texptr->ysize;

	float radius = (xsize > ysize) ? xsize : ysize;

	int side;
	float dot;
	mplane_t* plane = node->plane;

	plane = node->plane;
	if (plane->type <= PLANE_Z)
	{
		dot = endpos[plane->type] - plane->dist;
	}
	else
	{
		dot = DotProduct(endpos, plane->normal) - plane->dist;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

	// recurse down the children, front side first
	RecursiveCreateDecal(node->children[side], texptr, pDecal, endpos, pnormal, angle);

	// draw stuff
	int c = node->numsurfaces;
	if (c)
	{
		clientmsurface_t* surf = BSPWorld_Model::m_pWorldSurfaces + node->firstsurface;

		if (dot < 0 - BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for (; c; c--, surf++)
		{
			float dot;
			mplane_t* pplane = surf->plane;
			DotProductSub(&dot, &endpos, &pplane->normal, &pplane->dist);

			if (dot < 0)
				dot *= -1;

			if (dot < radius)
			{
				Vector normal = pplane->normal;

				if (surf->flags & SURF_PLANEBACK)
					VectorInverse(normal);

				if (pnormal.x != -999)
				{
					if (DotProduct(normal, pnormal) < 0.01)
						continue;

					DecalSurface(surf, texptr, NULL, pDecal, endpos, pnormal, angle);
				}
				else
					DecalSurface(surf, texptr, NULL, pDecal, endpos, normal, angle);
			}
		}
	}

	RecursiveCreateDecal(node->children[!side], texptr, pDecal, endpos, pnormal, angle);
}

// Generate lightmap coordinates at each vertex for decal vertices v[] on surface psurf
static void DecalVertsLight(customdecalpoly_t* p, clientmsurface_t* psurf)
{
	int j;
	float s, t;
	mtexinfo_t* tex = psurf->texinfo;

	for (j = 0; j < p->numverts; j++)
	{
		customdecalvert_t* v = &p->pverts[j];
		s = DotProduct(v->position, tex->vecs[0]) + tex->vecs[0][3];
		s -= psurf->texturemins[0];

		assert(s <= psurf->extents[0]);

		s += psurf->light_s * 16;
		s += 8;
		s /= LIGHTMAP_RESOLUTION * 16;

		t = DotProduct(v->position, tex->vecs[1]) + tex->vecs[1][3];
		t -= psurf->texturemins[1];

		assert(t <= psurf->extents[1]);

		t += psurf->light_t * 16;
		t += 8;
		t /= LIGHTMAP_RESOLUTION * 16;

		v->lmcoord[0] = s;
		v->lmcoord[1] = t;
	}
}

/*
====================
DecalSurface

====================
*/
void CBSPRenderer::DecalSurface(clientmsurface_t* surf, decalgroupentry_t* texptr, cl_entity_t* pEntity, customdecal_t* pDecal, Vector endpos, Vector pnormal, float angle)
{
	Vector norm;
	Vector right, up;

	Vector dverts1[64];
	Vector dverts2[64];

	if (pEntity && surf->texinfo->texture->name[0] == '{' && pEntity->curstate.rendermode == kRenderTransAlpha)
		return;

	if (surf->texinfo->texture->texture_flag & TEXTURE_SCROLL)
		return;

	if (surf->flags & SURF_DRAWTURB || surf->flags & SURF_DRAWSKY)
		return;

	mtexinfo_t* texinfo = surf->texinfo;

	Vector sAxis = {texinfo->vecs[0][0], texinfo->vecs[0][1], texinfo->vecs[0][2]};
	Vector tAxis = {texinfo->vecs[1][0], texinfo->vecs[1][1], texinfo->vecs[1][2]};

	VectorNormalize(sAxis);
	VectorNormalize(tAxis);

	VectorCopy(sAxis, right);
	VectorCopy(tAxis, up);

	if (angle != 0)
	{
		float radians = angle * (M_PI / 180.0f);
		float cosA = cosf(radians);
		float sinA = sinf(radians);

		Vector newRight, newUp;

		for (int i = 0; i < 3; ++i)
		{
			newRight[i] = right[i] * cosA + up[i] * sinA;
			newUp[i] = up[i] * cosA - right[i] * sinA;
		}

		VectorCopy(newRight, right);
		VectorCopy(newUp, up);
	}

	int xsize = texptr->xsize;
	int ysize = texptr->ysize;

	float texc_orig_x = DotProduct(endpos, right);
	float texc_orig_y = DotProduct(endpos, up);

	glpoly_t* p = surf->polys;
	float* v = p->verts[0];

	for (int j = 0; j < p->numverts; j++, v += VERTEXSIZE)
		VectorCopy(v, dverts1[j]);

	int nv;
	Vector planepoint;
	VectorMA(endpos, -xsize, right, planepoint);
	nv = ClipPolygonByPlane(dverts1, p->numverts, right, planepoint, dverts2);

	VectorMA(endpos, xsize, right, planepoint);
	nv = ClipPolygonByPlane(dverts2, nv, right * -1, planepoint, dverts1);

	VectorMA(endpos, -ysize, up, planepoint);
	nv = ClipPolygonByPlane(dverts1, nv, up, planepoint, dverts2);

	VectorMA(endpos, ysize, up, planepoint);
	nv = ClipPolygonByPlane(dverts2, nv, up * -1, planepoint, dverts1);

	if (!nv)
		return;

	if (pDecal->polys)
	{
		customdecalpoly_t* ppolys = new customdecalpoly_t[(pDecal->inumpolys + 1)];
		memcpy(ppolys, pDecal->polys, sizeof(customdecalpoly_t) * pDecal->inumpolys);
		delete[] pDecal->polys;
		pDecal->polys = ppolys;
		pDecal->inumpolys++;
	}
	else
	{
		pDecal->polys = new customdecalpoly_t;
		pDecal->inumpolys++;
	}

	customdecalpoly_t* pPoly = &pDecal->polys[(pDecal->inumpolys - 1)];
	pPoly->pverts = new customdecalvert_t[nv];
	pPoly->numverts = nv;

	for (int j = 0; j < nv; j++)
	{
		float texc_x = (DotProduct(dverts1[j], right) - texc_orig_x) / xsize;
		float texc_y = (DotProduct(dverts1[j], up) - texc_orig_y) / ysize;

		pPoly->pverts[j].texcoord[0] = (texc_x + 1) / 2;
		pPoly->pverts[j].texcoord[1] = (texc_y + 1) / 2;
		pPoly->pverts[j].position = dverts1[j];

		DecalVertsLight(pPoly, surf);
	}

	pPoly->surface = surf;
	pPoly->entity = pEntity;
}

/*
====================
CreateCachedDecals

====================
*/
void CBSPRenderer::CreateCachedDecals(void)
{
	for (auto decal : gPropManager.m_pDecals)
	{
		pmtrace_t pTrace;
		EV_SetTraceHull(2);

		// Z Axis
		gEngfuncs.pEventAPI->EV_PlayerTrace(decal.pos + Vector(0, 0, 2), decal.pos - Vector(0, 0, 2), PM_WORLD_ONLY, -2, &pTrace);

		if (pTrace.fraction == 1 || pTrace.fraction == 0)
			gEngfuncs.pEventAPI->EV_PlayerTrace(decal.pos - Vector(0, 0, 2), decal.pos + Vector(0, 0, 2), PM_WORLD_ONLY, -2, &pTrace);

		// Y Axis
		if (pTrace.fraction == 1 || pTrace.fraction == 0)
			gEngfuncs.pEventAPI->EV_PlayerTrace(decal.pos + Vector(0, 2, 0), decal.pos - Vector(0, 2, 0), PM_WORLD_ONLY, -2, &pTrace);

		if (pTrace.fraction == 1 || pTrace.fraction == 0)
			gEngfuncs.pEventAPI->EV_PlayerTrace(decal.pos - Vector(0, 2, 0), decal.pos + Vector(0, 2, 0), PM_WORLD_ONLY, -2, &pTrace);

		// X Axis
		if (pTrace.fraction == 1 || pTrace.fraction == 0)
			gEngfuncs.pEventAPI->EV_PlayerTrace(decal.pos + Vector(2, 0, 0), decal.pos - Vector(2, 0, 0), PM_WORLD_ONLY, -2, &pTrace);

		if (pTrace.fraction == 1 || pTrace.fraction == 0)
			gEngfuncs.pEventAPI->EV_PlayerTrace(decal.pos - Vector(2, 0, 0), decal.pos + Vector(2, 0, 0), PM_WORLD_ONLY, -2, &pTrace);

		if (pTrace.fraction == 1 || pTrace.fraction == 0)
			pTrace.plane.normal = Vector(0, 0, 1);

		CreateDecal(decal.pos, pTrace.plane.normal, decal.name, decal.persistent);
	}

	for (int i = 0; i < m_pMsgCache.size(); i++)
	{
		CreateDecal(m_pMsgCache[i]->pos, m_pMsgCache[i]->normal, m_pMsgCache[i]->name, m_pMsgCache[i]->persistent, m_pMsgCache[i]->fromwad);
	}

	m_pMsgCache.clear();
	gPropManager.m_pDecals.clear();
}

/*
====================
DrawSingleDecal

====================
*/
void CBSPRenderer::DrawSingleDecal(customdecal_t* decal, std::vector<Default3DTextureVert_t> &decalvertlist, bool m_bTransPass, bool *bNeedsBufferUpdate)
{
	glm::mat4 modelmatrix = glm::mat4(1.0f);

	for (int i = 0; i < decal->inumpolys; i++)
	{
		customdecalpoly_t* ppoly = &decal->polys[i];

		if (ppoly->surface->visframe != m_iFrameCount)
			continue;

		if (ppoly->entity)
		{
			if (IsEntityTransparent(ppoly->entity) && !m_bTransPass)
				continue;
			else if (!IsEntityTransparent(ppoly->entity) && m_bTransPass && (!ppoly->entity->curstate.effects & FL_MIRROR) )
				continue;

			if (IsEntityMoved(ppoly->entity))
			{
				glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(ppoly->entity->origin[0], ppoly->entity->origin[1], ppoly->entity->origin[2]));
				glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(ppoly->entity->angles[1]), glm::vec3(0, 0, 1));
				rotation = glm::rotate(rotation, glm::radians(ppoly->entity->angles[0]), glm::vec3(0, 1, 0));
				rotation = glm::rotate(rotation, glm::radians(ppoly->entity->angles[2]), glm::vec3(1, 0, 0));

				modelmatrix = translation * rotation;
				if ( (ppoly->entity->curstate.origin != ppoly->entity->prevstate.origin) || (ppoly->entity->curstate.angles != ppoly->entity->prevstate.angles) )
					(*bNeedsBufferUpdate) = true;
			}
		}
		else if (m_bTransPass)
			continue;

		for (int k = 1; k < ppoly->numverts - 1; ++k)
		{
			Default3DTextureVert_t v0, v1, v2;

			glm::vec4 pos1(ppoly->pverts[0].position.x, ppoly->pverts[0].position.y, ppoly->pverts[0].position.z, 1.0f);
			glm::vec4 pos2(ppoly->pverts[k].position.x, ppoly->pverts[k].position.y, ppoly->pverts[k].position.z, 1.0f);
			glm::vec4 pos3(ppoly->pverts[k + 1].position.x, ppoly->pverts[k + 1].position.y, ppoly->pverts[k + 1].position.z, 1.0f);

			pos1 = modelmatrix * pos1;
			pos2 = modelmatrix * pos2;
			pos3 = modelmatrix * pos3;

			v0.pos = Vector(pos1.x, pos1.y, pos1.z);
			memcpy(v0.texcoord, ppoly->pverts[0].texcoord, sizeof(float) * 2);
			memcpy(v0.lmcoord, ppoly->pverts[0].lmcoord, sizeof(float) * 2);

			v1.pos = Vector(pos2.x, pos2.y, pos2.z);
			memcpy(v1.texcoord, ppoly->pverts[k].texcoord, sizeof(float) * 2);
			memcpy(v1.lmcoord, ppoly->pverts[k].lmcoord, sizeof(float) * 2);

			v2.pos = Vector(pos3.x, pos3.y, pos3.z);
			memcpy(v2.texcoord, ppoly->pverts[k + 1].texcoord, sizeof(float) * 2);
			memcpy(v2.lmcoord, ppoly->pverts[k + 1].lmcoord, sizeof(float) * 2);

			decalvertlist.push_back(v0);
			decalvertlist.push_back(v1);
			decalvertlist.push_back(v2);
		}
	}
}

static int decals_lastdecalvertbuffersize = 0;
static int decals_lastdecalvertbuffersize_trans = 0;
static bool decals_needsbufferupdate = false;

struct texturebatchhash_t
{
	GLuint texindex;
	bool transdecal;
	bool operator==(const texturebatchhash_t& other) const
	{
		return (texindex == other.texindex
			&& transdecal == other.transdecal);
	}
};

template <>
struct std::hash<texturebatchhash_t>
{
	std::size_t operator()(const texturebatchhash_t& k) const
	{
		// Compute individual hash values for first,
		// second and third and combine them using XOR
		// and bit shifting:

		return ((std::hash<GLuint>()(k.texindex)
			^ (std::hash<bool>()(k.transdecal) << 1)));
	}
};

static void RenderRetroDecal(const std::pair<const texturebatchhash_t, std::vector<Default3DTextureVert_t>>& info,
	int bufferoffset){

	//todo : make these decals react to dynamic lights..
	//maybe switching to deferred rendering can help with this

	gBSPRenderer.BindGLTexture(LIGHTMAP_TEXUNIT, gBSPRenderer.m_iEngineLightmapIndex);
	gBSPRenderer.BindGLTexture(SURFTEXTURE_TEXUNIT, info.first.texindex);

	s_DecalShader.Uniform1i(s_DecalShader.GetUniformLoc("retrodecal"), 1);

	g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	s_pDecalMeshBuffer->DrawArrays(bufferoffset, info.second.size());
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);


	s_DecalShader.Uniform1i(s_DecalShader.GetUniformLoc("retrodecal"), 0);
}
static void RenderNewDecal(const std::pair<const texturebatchhash_t, std::vector<Default3DTextureVert_t>>& info,
	int bufferoffset) {
	gBSPRenderer.BindGLTexture(SURFTEXTURE_TEXUNIT, info.first.texindex);

	s_pDecalMeshBuffer->DrawArrays(bufferoffset, info.second.size());
}

/*
====================
DrawDecals

====================
*/
void CBSPRenderer::DrawDecals(bool m_bTransPass)
{
	CreateCachedDecals();

	if (m_pDecals.empty() && m_pStaticDecals.empty())
	{
		return;
	}

	std::unordered_map<texturebatchhash_t, std::vector<Default3DTextureVert_t>, std::hash<texturebatchhash_t>, std::equal_to<texturebatchhash_t>> decalbatch;

	for (int i = 0; i < m_pDecals.size(); i++)
	{
		std::vector<Default3DTextureVert_t> decalvertlist;
		DrawSingleDecal(m_pDecals[i].get(), decalvertlist, m_bTransPass, &decals_needsbufferupdate);
		customdecal_t* decal = m_pDecals[i].get();
		texturebatchhash_t hash { decal->texinfo->gl_texid, decal->texinfo->flags & DECALFLAG_TRANSDECAL };
		auto& row = decalbatch[hash];
		row.insert(row.end(), std::begin(decalvertlist), std::end(decalvertlist));
	}
	for (int i = 0; i < m_pStaticDecals.size(); i++)
	{
		std::vector<Default3DTextureVert_t> decalvertlist;
		DrawSingleDecal(m_pStaticDecals[i].get(), decalvertlist, m_bTransPass, &decals_needsbufferupdate);
		customdecal_t* decal = m_pStaticDecals[i].get();
		texturebatchhash_t hash{ decal->texinfo->gl_texid, decal->texinfo->flags & DECALFLAG_TRANSDECAL };
		auto& row = decalbatch[hash];
		row.insert(row.end(), std::begin(decalvertlist), std::end(decalvertlist));
	}
	if (decalbatch.empty())
		return;

	std::vector<Default3DTextureVert_t> decalvertlist_buffer;
	for (auto texture : decalbatch)
	{
		decalvertlist_buffer.insert(decalvertlist_buffer.end(), std::begin(texture.second), std::end(texture.second));
	}
	if (decalvertlist_buffer.size() >= (2 << 19))
		gEngfuncs.Con_Printf("[TRINITY] WARNING!! Decal vertice count has reached its limit !! (maximum of 524.288 vertices space stored in gpu buffer)");

	s_pDecalMeshBuffer->BindMesh();

	static bool updated_base_buffer = false; //this is so ugly
	if(!m_bTransPass)
	{
		if (decals_lastdecalvertbuffersize != decalvertlist_buffer.size() || decals_needsbufferupdate)
		{
			updated_base_buffer = true;
			decals_lastdecalvertbuffersize = decalvertlist_buffer.size();
			s_pDecalMeshBuffer->ModifyMesh(decalvertlist_buffer.data(), decalvertlist_buffer.size(), 0);
		}
	}
	else
	{
		if (!decalvertlist_buffer.empty())
		{
			if (decals_lastdecalvertbuffersize_trans != decalvertlist_buffer.size() || decals_needsbufferupdate || updated_base_buffer)
			{
				updated_base_buffer = false;
				decals_lastdecalvertbuffersize_trans = decalvertlist_buffer.size();
				s_pDecalMeshBuffer->ModifyMesh(decalvertlist_buffer.data(), decalvertlist_buffer.size(), decals_lastdecalvertbuffersize);
			}
		}
	}


	s_DecalShader.Bind();


	glDepthFunc(GL_LEQUAL);
	g_GlobalGLState.SetDepthWrite(false);

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	glPolygonOffset(-1, -1);
	g_GlobalGLState.SetPolygonOffsetFill(true);

	s_DecalShader.UniformMatrix4fv(eDecalShader_locs[decal_projviewmatrix], 1, GL_FALSE, glm::value_ptr(m_ProjectionMatrix * m_ViewMatrix));

	int bufferoffset = 0;
	if (m_bTransPass)
		bufferoffset = decals_lastdecalvertbuffersize;
	for (auto texture : decalbatch)
	{
		if (texture.first.transdecal)
			RenderRetroDecal(texture, bufferoffset);
		else
			RenderNewDecal(texture, bufferoffset);

		bufferoffset += texture.second.size();
	}

	if(m_pCvarWireFrame->value)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		g_GlobalGLState.SetCullFace(false);

		s_DecalShader.Uniform1i(eDecalShader_locs[decal_wireframe], 1);

		s_pDecalMeshBuffer->DrawArrays(0, decalvertlist_buffer.size());

		s_DecalShader.Uniform1i(eDecalShader_locs[decal_wireframe], 0);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		g_GlobalGLState.SetCullFace(true);
	}

	g_GlobalGLState.SetDepthWrite(true);

	g_GlobalGLState.SetBlend(false);;
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	g_GlobalGLState.SetPolygonOffsetFill(false);

	GL_Mesh::UnbindMesh();

	decals_needsbufferupdate = false;

}

/*
====================
MsgCustomDecal

====================
*/
int CBSPRenderer::MsgCustomDecal(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	Vector pos, normal;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	normal.x = READ_COORD();
	normal.y = READ_COORD();
	normal.z = READ_COORD();
	const char* decalname = READ_STRING();
	int persistent = READ_BYTE();
	int fromwad = READ_BYTE();
	float angle = READ_COORD();

	CreateDecal(pos, normal, decalname, persistent, fromwad, angle);
	return 1;
}

/*
====================
DeleteDecals

====================
*/
void CBSPRenderer::DeleteDecals(void)
{
	for (int i = 0; i < m_pDecals.size(); i++)
	{
		for (int j = 0; j < m_pDecals[i]->inumpolys; j++)
			delete[] m_pDecals[i]->polys[j].pverts;

		delete[] m_pDecals[i]->polys;
	}

	// Clear array completely
	m_pDecals.clear();

	for (int i = 0; i < m_pStaticDecals.size(); i++)
	{
		for (int j = 0; j < m_pStaticDecals[i]->inumpolys; j++)
			delete[] m_pStaticDecals[i]->polys[j].pverts;

		delete[] m_pStaticDecals[i]->polys;
	}

	// Clear array completely
	m_pStaticDecals.clear();

	decals_lastdecalvertbuffersize = decals_lastdecalvertbuffersize_trans = 0;
	decals_needsbufferupdate = true;
}

/*
====================
SetDynLightBBox

====================
*/
void CBSPRenderer::SetDynLightBBox(void)
{
	m_vDLightMins[0] = m_vCurDLightOrigin[0] - m_pCurrentDynLight->radius;
	m_vDLightMins[1] = m_vCurDLightOrigin[1] - m_pCurrentDynLight->radius;
	m_vDLightMins[2] = m_vCurDLightOrigin[2] - m_pCurrentDynLight->radius;
	m_vDLightMaxs[0] = m_vCurDLightOrigin[0] + m_pCurrentDynLight->radius;
	m_vDLightMaxs[1] = m_vCurDLightOrigin[1] + m_pCurrentDynLight->radius;
	m_vDLightMaxs[2] = m_vCurDLightOrigin[2] + m_pCurrentDynLight->radius;
}

/*
====================
CullDynLightBBox

====================
*/
int CBSPRenderer::CullDynLightBBox(Vector mins, Vector maxs)
{
	if (mins[0] > m_vDLightMaxs[0])
		return TRUE;

	if (mins[1] > m_vDLightMaxs[1])
		return TRUE;

	if (mins[2] > m_vDLightMaxs[2])
		return TRUE;

	if (maxs[0] < m_vDLightMins[0])
		return TRUE;

	if (maxs[1] < m_vDLightMins[1])
		return TRUE;

	if (maxs[2] < m_vDLightMins[2])
		return TRUE;

	return FALSE;
}

/*
====================
SetupDynLight

====================
*/
void CBSPRenderer::SetupDynLight(void)
{
	auto color = m_pCurrentDynLight->color;

	s_BSPShader.Uniform3fv(eWorldShader_locs[world_light_pos], 1, m_vCurDLightOrigin);
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_light_color], 1, m_pCurrentDynLight->color);
	s_BSPShader.Uniform1f(eWorldShader_locs[world_light_radius], m_pCurrentDynLight->radius);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_pointlight], 1);

	bool onlyshadows = (m_pCurrentDynLight->flags & LIGHT_ONLYSHADOWS);

	if (m_pCvarShadows->value && m_bMainPass && m_pCurrentDynLight->cubedepth)
	{
		s_BSPShader.Uniform1i(eWorldShader_locs[world_shadow], 1);
		s_BSPShader.Uniform1i(eWorldShader_locs[world_onlyshadow], onlyshadows);
		BindGLTexture(CUBEMAPSHADOW_TEXUNIT, m_pCurrentDynLight->cubedepth->GetTextureID());
	}

	if (!onlyshadows)
		g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
	else
		g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_ZERO);
}

/*
====================
FinishDynLight

====================
*/
void CBSPRenderer::FinishDynLight(void)
{
	s_BSPShader.Uniform1i(eWorldShader_locs[world_pointlight], 0);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_shadow], 0);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_onlyshadow], 0);

	BindGLTexture(CUBEMAPSHADOW_TEXUNIT, 0);
}

/*
====================
LightCanShadow

====================
*/
bool CBSPRenderer::LightCanShadow(void)
{
	if (m_pCvarShadows->value < 1)
		return false;

	if (m_pCurrentEntity->angles[0])
		return false;

	if (m_pCurrentEntity->angles[2])
		return false;

	if (!(m_pCurrentDynLight->flags & LIGHT_CASTSHADOWS))
		return false;

	return true;
}

/*
====================
SetupSpotLight

====================
*/
void CBSPRenderer::SetupSpotLight(void)
{
	glm::vec3 viewangles = glm::vec3(m_pCurrentDynLight->angles.x, m_pCurrentDynLight->angles.y, m_pCurrentDynLight->angles.z);
	Vector forward_, up_;
	AngleVectors(Vector(viewangles.x, viewangles.y, viewangles.z), &forward_, nullptr, &up_);

	glm::vec3 forward = glm::vec3(forward_.x, forward_.y, forward_.z);
	glm::vec3 up = glm::vec3(up_.x, up_.y, up_.z);


	float fov = m_pCurrentDynLight->cone_size;
	float aspect = 1.0f;
	float nearPlane = 1.0f;
	float farPlane = m_pCurrentDynLight->radius;

	glm::mat4 lightProj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);

	// spotlight view
	glm::vec3 lightPos = glm::vec3(m_vCurDLightOrigin.x, m_vCurDLightOrigin.y, m_vCurDLightOrigin.z);
	glm::vec3 lightDir = glm::vec3(forward.x * 50, forward.y * 50, forward.z * 50);

	glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, up);

	// final texture matrix
	glm::mat4 textureMatrix = lightProj * lightView;

	s_BSPShader.Uniform1i(eWorldShader_locs[world_spotlight], 1);

	bool onlyshadows = (m_pCurrentDynLight->flags & LIGHT_ONLYSHADOWS);

	s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_spotlight_texturematrix], 1, GL_FALSE, glm::value_ptr(textureMatrix));
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_light_pos], 1, m_pCurrentDynLight->origin);
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_light_color], 1, m_pCurrentDynLight->color);
	s_BSPShader.Uniform1f(eWorldShader_locs[world_light_radius], m_pCurrentDynLight->radius);

	BindGLTexture(SPOTLIGHT_TEXUNIT, m_pCurrentDynLight->textureindex);

	if (m_pCvarShadows->value && m_bMainPass && m_pCurrentDynLight->depth)
	{
		s_BSPShader.Uniform1i(eWorldShader_locs[world_shadow], 1);
		s_BSPShader.Uniform1i(eWorldShader_locs[world_onlyshadow], onlyshadows);

		BindGLTexture(SHADOWMAP_TEXUNIT, m_pCurrentDynLight->depth->GetTextureID());
	}


	if (!onlyshadows)
		g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
	else
		g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_ZERO);
}

/*
====================
FinishSpotLight

====================
*/
void CBSPRenderer::FinishSpotLight(void)
{
	s_BSPShader.Uniform1i(eWorldShader_locs[world_spotlight], 0);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_shadow], 0);
	s_BSPShader.Uniform1i(eWorldShader_locs[world_onlyshadow], 0);
}

// move this somewhere else

struct brushface
{
	int startvert = 0, numverts = 0;

	bool operator<(const brushface& other) const
	{
		return startvert < other.startvert || (startvert == other.startvert && numverts < other.numverts);
	}
	bool operator==(const brushface& other) const
	{
		return startvert == other.startvert && numverts == other.numverts;
	}
};

struct brushfacehash
{
    std::size_t operator()(const brushface& f) const noexcept
    {
        std::size_t h1 = std::hash<int>()(f.startvert);
        std::size_t h2 = std::hash<int>()(f.numverts);
        return h1 ^ (h2 << 1); // simple combine
    }
};

void CBSPRenderer::RenderSunShadow()
{
	if (!m_pCvarShadows->value || !m_bMainPass)
		return;

	//	this is just a way to mimic source engine's sun shadows. this works well, for me its good enough, a little weird-
	//	- but certainly alot better than the previous method which involved making a 256x256 shadowmap for every single-
	//	- entity on screen and projecting it onto the ground (yuck).

	Vector sunForward, sunUp;
	static float sunRadius = 8192;

	Vector sunAngles = Vector(75, 0, 0);
	if (g_StudioRenderer.m_pCvarSkyVecX->value != 0 &&
		g_StudioRenderer.m_pCvarSkyVecY->value != 0 &&
		g_StudioRenderer.m_pCvarSkyVecZ->value != 0)
	{
		Vector skyvec = Vector(g_StudioRenderer.m_pCvarSkyVecX->value, g_StudioRenderer.m_pCvarSkyVecY->value, g_StudioRenderer.m_pCvarSkyVecZ->value);
		VectorAngles(skyvec, sunAngles);
		sunAngles.x *= -1;
	}
	AngleVectors(sunAngles, &sunForward, nullptr, &sunUp);

	glm::vec3 glmSunForward = glm::vec3(sunForward.x, sunForward.y, sunForward.z);
	glm::vec3 glmSunUp = glm::vec3(sunUp.x, sunUp.y, sunUp.z);

	Vector vSunPos = m_vRenderOrigin + -(sunForward * sunRadius / 4);

	glm::vec3 glmvSunPos = glm::vec3(vSunPos.x, vSunPos.y, vSunPos.z);
	static glm::mat4 sunProjectionMatrix = glm::ortho(-768.f, 768.f, -768.f, 768.f, 1.0f, sunRadius);
	glm::mat4 sunViewMatrix = glm::lookAt(glmvSunPos, glmvSunPos + glmSunForward, glmSunUp);

	// final texture matrix
	glm::mat4 textureMatrix = sunProjectionMatrix * sunViewMatrix;

	s_BSPShader.Uniform1i(eWorldShader_locs[world_shadow], 1);

	s_BSPShader.UniformMatrix4fv(eWorldShader_locs[world_spotlight_texturematrix], 1, GL_FALSE, glm::value_ptr(textureMatrix));
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_sundir], 1, glm::value_ptr(glmSunForward));
	s_BSPShader.Uniform3fv(eWorldShader_locs[world_light_pos], 1, vSunPos);
	s_BSPShader.Uniform1f(eWorldShader_locs[world_light_radius], sunRadius);

	BindGLTexture(SHADOWMAP_TEXUNIT, m_pSunShadowMap->GetTextureID());
	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_ZERO);

	for (int i = 0; i < m_iNumTextures; i++)
	{
		clientmsurface_t* psurface = (clientmsurface_t*)m_pNormalTextureList[i].texturechain;

		while (psurface)
		{
			if (psurface->flags & SURF_DRAWTURB)
			{
				psurface = psurface->texturechain;
				continue;
			}

			mplane_t* plane = psurface->plane;

			float dot; 
			dot = DotProduct(plane->normal, -sunForward);

			if ((dot < 0) ^ !!(psurface->flags & SURF_PLANEBACK))
			{
				psurface = psurface->texturechain;
				continue;
			}


			int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
			brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

			multidraw_startverts[num_multidraws] = (pbrushface->start_vertex);
			multidraw_numverts[num_multidraws] = (pbrushface->num_vertexes);

			num_multidraws++;

			m_iBSPVertsCounter += pbrushface->num_vertexes;

			psurface = psurface->texturechain;
		}

	}

	glMultiDrawArrays(GL_TRIANGLES, (GLint*)multidraw_startverts, (GLint*)multidraw_numverts, num_multidraws);
	num_multidraws = 0;

	g_GlobalGLState.SetBlend(true);

	s_BSPShader.Uniform1i(eWorldShader_locs[world_shadow], 0);
}

/*
====================
DrawDynamicLightsForWorld

====================
*/
void CBSPRenderer::DrawDynamicLightsForWorld(void)
{

	if (m_pCvarDynamic->value < 1)
		return;

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetDepthWrite(false);
	glDepthFunc(GL_EQUAL);

	if(m_pCvarSunShadowsQuality->value > 0)
		RenderSunShadow();

	float time = engine_cl->time;

	for (auto &dynlight : m_pDynLights)
	{
		if (dynlight->die < time || !dynlight->radius || ( (dynlight->flags & LIGHT_ONLYSHADOWS) && !m_bMainPass))
			continue;

		if (dynlight->visframe)
			if (!IsInPotentiallyVisibleSet(dynlight->visframe))
				continue;

		m_pCurrentDynLight = dynlight.get();
		m_vCurDLightOrigin = m_pCurrentDynLight->origin;

		bool onlyshadows = (m_pCurrentDynLight->flags & LIGHT_ONLYSHADOWS);

		if (onlyshadows)
		{
			float dist = (m_RefParams.vieworg - dynlight->origin).Length();
			if (dist > 768)
				continue;
		}

		if (!m_pCvarShadows->value || !m_bMainPass || !m_pCurrentDynLight->cubedepth)
			if (onlyshadows)
				continue;
		
		if (dynlight->cone_size)
		{
			Vector lightangles = m_pCurrentDynLight->angles;
			AngleVectors(lightangles, &m_vCurSpotForward, NULL, NULL);
			SetupSpotLight();
		}
		else
		{
			SetupDynLight();
			SetDynLightBBox();
		}

		RecursiveWorldNodeLight(BSPWorld_Model::m_pWorldNodes);

		for (int i = 0; i < m_iNumRenderEntities; i++)
		{
			auto ent = m_pRenderEntities[i];

			if(!ent->model)
				continue;
			if (ent->model->type != mod_brush)
				continue;

			if ((ent->curstate.renderfx != 70) && !IsEntityMoved(ent) && !IsEntityTransparent(ent) && ent->visframe == m_iFrameCount)
			{
				if (dynlight->cone_size)
				{
					if (dynlight->frustum.CullBox(ent->curstate.mins, ent->curstate.maxs))
						continue;
				}
				else
				{
					if (CullDynLightBBox(ent->curstate.mins, ent->curstate.maxs))
						continue;
				}

				DrawEntityFacesForLight(ent);
			}
		}

		if (!num_multidraws)
		{
			if (dynlight->cone_size)
				FinishSpotLight();
			else
				FinishDynLight();

			continue;
		}

		glMultiDrawArrays(GL_TRIANGLES, (GLint*)multidraw_startverts, (GLint*)multidraw_numverts, num_multidraws);

		num_multidraws = 0;
		
		if (dynlight->cone_size)
			FinishSpotLight();
		else
			FinishDynLight();
	}

	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	g_GlobalGLState.SetDepthWrite(true);
	glDepthFunc(GL_LEQUAL);
	m_pCurrentDynLight = nullptr;
}


/*
====================
RecursiveWorldNodeLight

====================
*/
void CBSPRenderer::RecursiveWorldNodeLight(clientmnode_t* node)
{
	int side;
	float dot;

	while(true)
	{
		if (node->contents == CONTENTS_SOLID)
			return; // solid

		if (node->visframe != r_visframecount)
			return;

		// buz: visible surfaces already marked
		if (node->contents < 0)
			return;

		if (!m_pCurrentDynLight->cone_size)
		{
			if (CullDynLightBBox(node->minmaxs, node->minmaxs + 3)) // cull from point light bbox
				return;
		}
		else
		{
			if (m_pCurrentDynLight->frustum.CullBox(node->minmaxs, node->minmaxs + 3))
				return;
		}

		// node is just a decision point, so go down the apropriate sides
		// find which side of the node we are on

		mplane_t* plane = node->plane;
		if (plane->type <= PLANE_Z)
		{
			dot = m_vCurDLightOrigin[plane->type] - plane->dist;
		}
		else
		{
			dot = DotProduct(m_vCurDLightOrigin, plane->normal) - plane->dist;
		}

		side = dot >= 0 ? 0 : 1;

		// recurse down the children, front side first
		RecursiveWorldNodeLight(node->children[side]);

		// draw stuff
		int c = node->numsurfaces;
		if (c)
		{
			clientmsurface_t* surf = BSPWorld_Model::m_pWorldSurfaces + node->firstsurface;

			for (; c; c--, surf++)
			{
				if (surf->visframe != m_iFrameCount)
					continue;

				if (surf->flags & SURF_DRAWTURB)
					continue;

				if (surf->flags & SURF_DRAWSKY)
					continue;

				// don't backface underwater surfaces, because they warp
				if (!(surf->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
					continue; // wrong side

				DotProductSub(&dot, &m_vCurDLightOrigin, &surf->plane->normal, &surf->plane->dist);

				if (dot < m_pCurrentDynLight->radius)
				{
					int surfaceIndex = surf - BSPWorld_Model::m_pWorldSurfaces;
					brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

					multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
					multidraw_numverts[num_multidraws] = pbrushface->num_vertexes;
					num_multidraws++;
				}
			}
		}

		// recurse down the back side
		// NOTE: With this while loop, this is identical to just calling
		// RecursiveWorldNodeLight(node->children[!side]);
		node = node->children[!side];
	}

//	for (int i = 0; i < m_iNumTextures; i++)
//	{
//		msurface_t* psurface = m_pNormalTextureList[i].texturechain;
//
//		while (psurface)
//		{
//			float dot;
//	
//			mplane_t* pplane = psurface->plane;
//			DotProductSub(&dot, &m_vCurDLightOrigin, &psurface->plane->normal, &psurface->plane->dist);
//	
//			if (dot > m_pCurrentDynLight->radius)
//			{
//				psurface = psurface->texturechain;
//				continue;
//			}
//	
//			int surfaceIndex = psurface - BSPWorld_Model::m_pWorldSurfaces;
//			brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];
//	
//			faces_.insert({pbrushface->start_vertex, pbrushface->num_vertexes});
//	
//			psurface = psurface->texturechain;
//		}
//	}
}

/*
====================
DynamicLighted

====================
*/
bool CBSPRenderer::DynamicLighted(const Vector& vmins, const Vector& vmaxs)
{
	if (IsEntityTransparent(m_pCurrentEntity))
		return false;

	float time = engine_cl->time;
	for (auto &dynlight : m_pDynLights)
	{
		if (dynlight->die < time || !dynlight->radius)
			continue;

		m_pCurrentDynLight = dynlight.get();
		m_vCurDLightOrigin = m_pCurrentDynLight->origin;

		if (m_pCurrentDynLight->cone_size)
		{
			if (m_pCurrentDynLight->frustum.CullBox((Vector)vmins, (Vector)vmaxs))
				continue;
		}
		else
		{
			SetDynLightBBox();
			if (CullDynLightBBox(vmins, vmaxs))
				continue;
		}

		m_pCurrentDynLight = nullptr;

		return true;
	}
	m_pCurrentDynLight = nullptr;
	return false;
}

/*
====================
DrawDynamicLightsForEntity

====================
*/
void CBSPRenderer::DrawDynamicLightsForEntity(cl_entity_t* pEntity)
{
	Vector mins, maxs;
	int rotated;

	if (m_pCvarDynamic->value < 1 || !HasDynLights())
		return;

	if (pEntity->angles[0] || pEntity->angles[1] || pEntity->angles[2])
	{
		rotated = true;
		for (int i = 0; i < 3; i++)
		{
			mins[i] = -pEntity->model->radius;
			maxs[i] = +pEntity->model->radius;
		}
	}
	else
	{
		rotated = false;
		mins = pEntity->model->mins;
		maxs = pEntity->model->maxs;
	}

	float time = engine_cl->time;

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetDepthWrite(false);
	glDepthFunc(GL_EQUAL);

	for (auto &dynlight : m_pDynLights)
	{
		Vector temp, forward, right, up;

		if (dynlight->die < time || !dynlight->radius)
			continue;

		m_pCurrentDynLight = dynlight.get();
		m_vCurDLightOrigin = m_pCurrentDynLight->origin;

		bool onlyshadows = (m_pCurrentDynLight->flags & LIGHT_ONLYSHADOWS);

		if (onlyshadows)
		{
			float dist = (m_RefParams.vieworg - dynlight->origin).Length();
			if (dist > 768)
				continue;
		}

		if (!m_pCvarShadows->value || !m_bMainPass || !m_pCurrentDynLight->cubedepth)
			if (onlyshadows)
				continue;

		if (m_pCurrentDynLight->cone_size)
		{
			Vector tmins, tmaxs;
			VectorAdd(mins, m_pCurrentEntity->origin, tmins);
			VectorAdd(maxs, m_pCurrentEntity->origin, tmaxs);

			if (m_pCurrentDynLight->frustum.CullBox(tmins, tmaxs))
				continue;

			SetupSpotLight();
		}
		else
		{

			SetDynLightBBox();
			if (CullDynLightBBox(mins, maxs))
				continue;

			SetupDynLight();
		}

		
		VectorSubtract(m_pCurrentDynLight->origin, pEntity->origin, m_vCurDLightOrigin);
		if (rotated)
		{
			AngleVectors(pEntity->angles, &forward, &right, &up);
		
			VectorCopy(m_vCurDLightOrigin, temp);
			m_vCurDLightOrigin[0] = DotProduct(temp, forward);
			m_vCurDLightOrigin[1] = -DotProduct(temp, right);
			m_vCurDLightOrigin[2] = DotProduct(temp, up);
		}


		DrawEntityFacesForLight(pEntity);

		if (dynlight->cone_size)
			FinishSpotLight();
		else
			FinishDynLight();
	}

	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	g_GlobalGLState.SetDepthWrite(true);
	glDepthFunc(GL_LEQUAL);
	m_pCurrentDynLight = nullptr;
}

/*
====================
DrawEntityFacesForLight

====================
*/
void CBSPRenderer::DrawEntityFacesForLight(cl_entity_t* pEntity)
{
	float dot;
	clientmsurface_t* psurf = &BSPWorld_Model::m_pWorldSurfaces[pEntity->model->firstmodelsurface];
	for (int i = 0; i < pEntity->model->nummodelsurfaces; i++, psurf++)
	{
		if (psurf->flags & SURF_DRAWTURB)
			continue;

		if (psurf->flags & SURF_DRAWSKY)
			continue;

		if (psurf->visframe == m_iFrameCount) // visible
		{
			mplane_t* pplane = psurf->plane;
			DotProductSub(&dot, &m_vCurDLightOrigin, &pplane->normal, &pplane->dist);
			if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
				(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
			{
				if (dot < m_pCurrentDynLight->radius)
					DrawPolyFromArray(BSPWorld_Model::m_pWorldSurfaces, psurf);
			}
		}
	}
}

/*
====================
InitSky

====================
*/
void CBSPRenderer::InitSky(void)
{
	m_bDrawSky = true;
	if (m_szSkyName[0] == 0)
	{
		m_bDrawSky = false;
		return;
	}

	const char* szSkySuffixes[] = {"lf", "bk", "rt", "ft", "dn", "up"};

	for (int i = 0; i < 6; i++)
	{
		char szPathS[64];
		char szPathL[64];

		sprintf(szPathL, "gfx/env/%s%s_large.dds", m_szSkyName, szSkySuffixes[i]);
		sprintf(szPathS, "gfx/env/%s%s.tga", m_szSkyName, szSkySuffixes[i]);

		cl_texture_t* pTexture = gTextureLoader.LoadTexture(szPathL, FALSE, false, true);

		if (pTexture)
		{
			m_iSkyTextures[i] = pTexture->iIndex;
		}
		else
		{
			pTexture = gTextureLoader.LoadTexture(szPathS, true, true);

			if (!pTexture)
			{
				m_bDrawSky = false;
				return;
			}

			m_iSkyTextures[i] = pTexture->iIndex;
		}

		// ALWAYS Bind
		glBindTexture(GL_TEXTURE_2D, pTexture->iIndex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
};


/*
====================
DrawSky

====================
*/
void CBSPRenderer::DrawSky(void)
{
	fog_settings_t pSaved;
	static float projection[16];

	if (!m_bDrawSky)
		return;

	s_SkyboxShader.Bind();
	s_pSkyboxMesh->BindMesh();

	glm::mat4 viewrotation = m_ViewMatrix;
	viewrotation[3][0] = viewrotation[3][1] = viewrotation[3][2] = 0;

	s_SkyboxShader.UniformMatrix4fv(eSimpleSkyboxShader_locs[skybox_projviewmatrix], 1, GL_FALSE, glm::value_ptr(m_ProjectionMatrix * viewrotation));
	s_SkyboxShader.Uniform1i(eSimpleSkyboxShader_locs[skybox_skyfog], gHUD.m_pFogSettings.affectsky);
	s_SkyboxShader.Uniform3fv(eSimpleSkyboxShader_locs[skybox_fogcolor], 1, gHUD.m_pFogSettings.color);

	for (int i = 0; i < 6; i++)
	{
		BindGLTexture(GL_TEXTURE0, m_iSkyTextures[i]);
		s_pSkyboxMesh->DrawArrays(i * 6, 6);
	}

	GL_ShaderProgram::ResetShaderBind();


	if (m_vSkyOrigin != Vector(0, 0, 0) && m_pCvar3DSkybox->value && m_bMainPass)
	{
		m_bMainPass = !m_bMainPass;

		Vector oldrenderorigin = m_vRenderOrigin;
		glm::mat4 oldviewmatrix = m_ViewMatrix;
		glm::mat4 oldprojmatrix = m_ProjectionMatrix;
		FrustumCheck oldfrustum = gHUD.viewFrustum;
		ref_params_t oldviewparams = m_RefParams;

		float fov = gHUD.m_iFOV;
		if (!fov)
			return;
		float fovy = fov;

		AdjustFOV(&fov, &fovy, ScreenWidth, ScreenHeight, 0);

		float aspect = (float)ScreenWidth / (float)ScreenHeight;

		m_ProjectionMatrix = glm::perspective(fov, aspect, 1.f, 16384.f);


		m_vRenderOrigin = (m_vSkyOrigin + m_vRenderOrigin * 0.0005f);

		Vector viewangles = m_vViewAngles;
		Vector forward, right, up;
		AngleVectors(viewangles, &forward, &right, &up);

		glm::vec3 cameraForward = glm::vec3(forward.x, forward.y, forward.z);
		glm::vec3 cameraUp = glm::vec3(up.x, up.y, up.z);

		glm::vec3 cameraPos = glm::vec3(m_vRenderOrigin.x, m_vRenderOrigin.y, m_vRenderOrigin.z);
		glm::vec3 cameraTarget = cameraPos + cameraForward;

		clientmleaf_t* skyboxleaf = Mod_PointInLeaf(m_vRenderOrigin);

		R_MarkLeaves(skyboxleaf);

		m_ViewMatrix = glm::lookAt(cameraPos, cameraTarget, cameraUp);

		gHUD.viewFrustum.SetFrustum(viewangles, m_vRenderOrigin, gHUD.m_iFOV, gHUD.m_pFogSettings.end, true);

		fog_settings_t restorefog = gHUD.m_pFogSettings;
		gHUD.m_pFogSettings.end *= 0.2;

		m_RefParams.vieworg = m_vRenderOrigin;
		m_RefParams.forward = forward;
		m_RefParams.right = right;
		m_RefParams.up = up;
		g_StudioRenderer.StudioPreFrame(&m_RefParams);

		DrawWorld(true);

		//fog disabled for studiomdl static props for now, theyre a bit messed up 
		//gHUD.m_pFogSettings.active = 0;

		gPropManager.RenderProps(true);

		gHUD.m_pFogSettings = restorefog;

		m_vRenderOrigin = oldrenderorigin;
		m_ViewMatrix = oldviewmatrix;
		m_ProjectionMatrix = oldprojmatrix;
		gHUD.viewFrustum = oldfrustum;
		m_RefParams = oldviewparams;
		
		m_bMainPass = !m_bMainPass;


		g_StudioRenderer.StudioPreFrame(&m_RefParams);

		//SALSATOBIAS: hackyyyy please improve

		r_oldviewleaf = nullptr;
		R_MarkLeaves(m_pViewLeaf);
	}

	glClear(GL_DEPTH_BUFFER_BIT);
	GL_Mesh::UnbindMesh();
};

/*
====================
CL_AllocDLight

====================
*/
cl_dlight_t* CBSPRenderer::CL_AllocDLight(int key)
{
	int i;
	float time = engine_cl->time;

	// first look for an exact key match
	if (key)
	{
		for (auto &dlight : m_pDynLights)
		{
			if (dlight->key == key)
			{
				GL_ShadowMap* idepth = dlight->depth;
				GL_ShadowMap* cubedepth = dlight->cubedepth;
				memset(dlight.get(), 0, sizeof(cl_dlight_t));
				dlight->key = key;
				dlight->depth = idepth;
				dlight->cubedepth = cubedepth;
				return dlight.get();
			}
		}
	}
	
	// then look for anything else
	for (auto& dlight : m_pDynLights)
	{
		if (dlight->die < time)
		{
			GL_ShadowMap* idepth = dlight->depth;
			GL_ShadowMap* cubedepth = dlight->cubedepth;
			memset(dlight.get(), 0, sizeof(cl_dlight_t));
			dlight->key = key;
			dlight->depth = idepth;
			dlight->cubedepth = cubedepth;
			return dlight.get();
		}
	}

	cl_dlight_t* dlight = m_pDynLights.emplace_back(std::make_unique<cl_dlight_t>()).get();
	GL_ShadowMap* idepth = nullptr;
	GL_ShadowMap* cubedepth = nullptr;
	memset(dlight, 0, sizeof(cl_dlight_t));
	dlight->key = key;
	dlight->depth = idepth;
	dlight->cubedepth = cubedepth;
	dlight->justspawned = false;
	return dlight;
}

/*
====================
DecayLights

====================
*/
void CBSPRenderer::DecayLights(void)
{
	static float lasttime = 0;

	float time = engine_cl->time;
	float frametime = time - lasttime;

	if (frametime > 1)
		frametime = 1;

	if (frametime < 0)
		frametime = 0;

	lasttime = time;

	for (int i = 0; i < m_pDynLights.size();)
	{
		auto &dlight = m_pDynLights[i];
		if (dlight->die < time || !dlight->radius)
		{
			if (!dlight->justspawned)
			{
				if (dlight->depth)
					GL_ShadowMap::DeAllocateShadowMap(dlight->depth);
				else if (dlight->cubedepth)
					GL_ShadowMap::DeAllocateShadowMap(dlight->cubedepth);

				m_pDynLights.erase(m_pDynLights.begin() + i);
				continue;
			}
			else
			{
				dlight->justspawned = false;
			}
		}

		dlight->radius -= frametime * dlight->decay;

		if (dlight->radius < 0)
			dlight->radius = 0;

		i++;
	}
}

/*
====================
MsgSkyMarker_Sky

====================
*/
int CBSPRenderer::MsgSkyMarker_Sky(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_vSkyOrigin.x = READ_COORD();
	m_vSkyOrigin.y = READ_COORD();
	m_vSkyOrigin.z = READ_COORD();

	gHUD.m_pSkyFogSettings.affectsky = true;
	gHUD.m_pSkyFogSettings.end = READ_SHORT();
	gHUD.m_pSkyFogSettings.start = READ_SHORT();
	gHUD.m_pSkyFogSettings.color.x = (float)READ_BYTE() / 255;
	gHUD.m_pSkyFogSettings.color.y = (float)READ_BYTE() / 255;
	gHUD.m_pSkyFogSettings.color.z = (float)READ_BYTE() / 255;
	gHUD.m_pSkyFogSettings.affectsky = (READ_SHORT() == 1) ? false : true;

	if (gHUD.m_pSkyFogSettings.end < 1 && gHUD.m_pSkyFogSettings.start < 1)
		gHUD.m_pSkyFogSettings.active = false;
	else
		gHUD.m_pSkyFogSettings.active = true;

	return 1;
}

/*
====================
MsgSkyMarker_World

====================
*/
int CBSPRenderer::MsgSkyMarker_World(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	m_vSkyWorldOrigin.x = READ_COORD();
	m_vSkyWorldOrigin.y = READ_COORD();
	m_vSkyWorldOrigin.z = READ_COORD();
	m_fSkySpeed = READ_COORD();
	return 1;
}

/*
====================
MsgDynLight

====================
*/
int CBSPRenderer::MsgDynLight(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	cl_dlight_t* dl = CL_AllocDLight(0);

	dl->origin.x = READ_COORD();
	dl->origin.y = READ_COORD();
	dl->origin.z = READ_COORD();
	dl->radius = READ_BYTE() * 64;
	dl->color.x = (float)READ_BYTE() / 255;
	dl->color.y = (float)READ_BYTE() / 255;
	dl->color.z = (float)READ_BYTE() / 255;
	dl->die = READ_FLOAT() + engine_cl->time;
	dl->decay = READ_BYTE() * 10;
	dl->flags |= LIGHT_CASTSHADOWS;
	return 1;
}

/*
====================
Make_ShadowMaps

purpose: Generates all the shadowmaps that are to be projected in the main view pass

====================
*/
void CBSPRenderer::Make_ShadowMaps(void)
{
	m_iNumTotalShadows = 0;

	if (m_pCvarDynamic->value < 1)
		return;

	if (m_pCvarShadows->value < 1)
		return;

	GL_ShaderProgram::ResetShaderBind();

	float time = engine_cl->time;


	glm::mat4 oldprojection = m_ProjectionMatrix;
	glm::mat4 oldviewmatrix = m_ViewMatrix;
	FrustumCheck oldfrustum = gHUD.viewFrustum;

	Vector restoreviewangles = m_vViewAngles;
	Vector restorerenderorigin = m_vRenderOrigin;



	GL_ShadowMap::StartShadowMapping();

	if (m_pCvarSunShadowsQuality->value > 0)
		Generate_Sun_Shadow();

	for (auto &dynlight : m_pDynLights)
	{
		if (dynlight->die < time || !dynlight->radius || !(dynlight->flags & LIGHT_CASTSHADOWS))
			continue;

		if(dynlight->visframe)
			if (!CHECKVISBIT(m_pPVS, dynlight->visframe))
				continue;

		if (dynlight->flags & LIGHT_ONLYSHADOWS)
		{
			float dist = (m_RefParams.vieworg - dynlight->origin).Length();
			if (dist > 768)
				continue;
		}

		m_pCurrentDynLight = dynlight.get();
		if (dynlight->cone_size)
		{
			Generate_Spotlight_Shadow();
			dynlight->depth->SetPosition(dynlight->origin);
		}
		else
		{
			Generate_Pointlight_Shadow();
			dynlight->cubedepth->SetPosition(dynlight->origin);
		}
	}

	m_ProjectionMatrix = oldprojection;
	m_ViewMatrix = oldviewmatrix;
	gHUD.viewFrustum = oldfrustum;

	m_vViewAngles = restoreviewangles;
	m_vRenderOrigin = restorerenderorigin;

	r_oldviewleaf = nullptr;
	R_MarkLeaves(m_pViewLeaf);

	m_pCurrentDynLight = nullptr;

	GL_ShadowMap::EndShadowMapping();

}

cl_dlight_t dummylight;

void CBSPRenderer::Generate_Sun_Shadow()
{
	m_pSunShadowMap->InitRendering(Vector(1, 1, 0));

	m_bSunShadowMapPass = true;

	glm::mat4 oldprojection = m_ProjectionMatrix;
	glm::mat4 oldviewmatrix = m_ViewMatrix;
	FrustumCheck oldfrustum = gHUD.viewFrustum;
	Vector oldviewangles = m_vViewAngles;
	Vector oldrenderorigin = m_vRenderOrigin;

	gHUD.viewFrustum.SetFrustum(m_vViewAngles, m_vRenderOrigin, gHUD.m_iFOV, gHUD.m_pFogSettings.end, true);

	//static Vector sunForward = Vector(0, 0, -1);
	//static Vector sunUp = Vector(-1, 0, 0);
	Vector sunForward, sunUp;
	static float sunRadius = 8192;

	Vector sunAngles = Vector(75, 0, 0);
	if (g_StudioRenderer.m_pCvarSkyVecX->value != 0 && 
		g_StudioRenderer.m_pCvarSkyVecY->value != 0 && 
		g_StudioRenderer.m_pCvarSkyVecZ->value != 0)
	{
		Vector skyvec = Vector(g_StudioRenderer.m_pCvarSkyVecX->value, g_StudioRenderer.m_pCvarSkyVecY->value, g_StudioRenderer.m_pCvarSkyVecZ->value);
		VectorAngles(skyvec, sunAngles);
		sunAngles.x *= -1;
	}
	m_vViewAngles = sunAngles;
	AngleVectors(m_vViewAngles, &sunForward, nullptr, &sunUp);
	VectorAngles(sunForward, m_vViewAngles);
	m_vViewAngles.x *= -1;
	
	glm::vec3 glmSunForward = glm::vec3(sunForward.x, sunForward.y, sunForward.z);
	glm::vec3 glmSunUp = glm::vec3(sunUp.x, sunUp.y, sunUp.z);
	
	m_vRenderOrigin = m_vRenderOrigin + -(sunForward * sunRadius / 4);
	m_pCurrentDynLight = &dummylight;
	m_pCurrentDynLight->origin = m_vRenderOrigin;
	m_pCurrentDynLight->radius = sunRadius;
	glm::vec3 glmvSunPos = glm::vec3(m_vRenderOrigin.x, m_vRenderOrigin.y, m_vRenderOrigin.z);
	m_ProjectionMatrix = glm::ortho(-768.f, 768.f, -768.f, 768.f, 1.0f, sunRadius);
	m_ViewMatrix = glm::lookAt(glmvSunPos, glmvSunPos + glmSunForward, glmSunUp);

	if (m_pCvarSunShadowsQuality->value > 1)
		DrawWorldSolid();
	
	
	g_StudioRenderer.StudioDrawModelsSolid();

	gPropManager.RenderPropsSolid();



	m_ProjectionMatrix = oldprojection;
	m_ViewMatrix = oldviewmatrix;
	gHUD.viewFrustum = oldfrustum;

	m_vViewAngles = oldviewangles;
	m_vRenderOrigin = oldrenderorigin;

	m_pSunShadowMap->FinishRendering();

	m_bSunShadowMapPass = false;
}

/*
====================
Generate_Spotlight_Shadow

purpose: generates a shadow depth texture for the current spotlight.

====================
*/
void CBSPRenderer::Generate_Spotlight_Shadow(void)
{

	if (!m_pCurrentDynLight->depth)
	{
		m_pCurrentDynLight->depth = GL_ShadowMap::AllocateShadowMap(
			GL_ShadowMap::_2DTexture_Storage,
			GL_RG16F,
			DEFAULT_SHADOWMAP_RES, DEFAULT_SHADOWMAP_RES,
			0,
			GL_RG, GL_FLOAT);
	}

	m_pCurrentDynLight->depth->InitRendering(Vector(1, 1, 0));

	gHUD.viewFrustum.SetFrustum(m_pCurrentDynLight->angles, m_pCurrentDynLight->origin, m_pCurrentDynLight->cone_size, m_pCurrentDynLight->radius);

	Vector forward_, up_;
	AngleVectors(m_pCurrentDynLight->angles, &forward_, nullptr, &up_);

	glm::vec3 forward = glm::vec3(forward_.x, forward_.y, forward_.z);
	glm::vec3 up = glm::vec3(up_.x, up_.y, up_.z);


	float fov = m_pCurrentDynLight->cone_size; // degrees
	float aspect = 1.0f;					   // square spotlight texture
	float nearPlane = 1.0f;
	float farPlane = m_pCurrentDynLight->radius;

	m_ProjectionMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane );

	// spotlight view
	glm::vec3 lightPos = glm::vec3(m_pCurrentDynLight->origin.x, m_pCurrentDynLight->origin.y, m_pCurrentDynLight->origin.z);
	glm::vec3 lightDir = glm::vec3(forward.x, forward.y, forward.z);

	m_ViewMatrix = glm::lookAt(lightPos, lightPos + lightDir, up);

	m_ModelMatrix = glm::mat4(1.0f);

	m_vViewAngles = m_pCurrentDynLight->angles;
	m_vRenderOrigin = m_pCurrentDynLight->origin;
	
	if(m_pCurrentDynLight->flags & LIGHT_STUDIOMDL_SHADOW)
	{
		g_StudioRenderer.StudioDrawModelsSolid();

		gPropManager.RenderPropsSolid();
	}

	if ((m_pCurrentDynLight->flags & LIGHT_BRUSH_SHADOW) || (m_pCurrentDynLight->flags & LIGHT_WORLD_SHADOW))
	{
		clientmleaf_t* spotlight_leaf = Mod_PointInLeaf(m_vRenderOrigin);

		R_MarkLeaves(spotlight_leaf);

		DrawWorldSolid();
	}

	r_oldviewleaf = nullptr;

	m_pCurrentDynLight->depth->FinishRendering();

	m_iNumTotalShadows++;

	R_MarkLeaves(m_pViewLeaf);
}

/*
====================
Generate_Pointlight_Shadow

purpose: generates a shadow depth texture for the current point light.

====================
*/
void CBSPRenderer::Generate_Pointlight_Shadow(void)
{
	if (!m_pCurrentDynLight->cubedepth)
	{
		if ((m_pCurrentDynLight->flags & LIGHT_BRUSH_SHADOW) || (m_pCurrentDynLight->flags & LIGHT_WORLD_SHADOW))
			m_pCurrentDynLight->cubedepth = GL_ShadowMap::AllocateShadowMap(
				GL_ShadowMap::_CubeMap_Storage,
				GL_RG16F,
				DEFAULT_SHADOWMAP_RES, DEFAULT_SHADOWMAP_RES,
				0,
				GL_RG, GL_FLOAT);
		else //ev_elight shadows are much cheaper
			m_pCurrentDynLight->cubedepth = GL_ShadowMap::AllocateShadowMap(
				GL_ShadowMap::_CubeMap_Storage,
				GL_R8,
				DEFAULT_SHADOWMAP_RES, DEFAULT_SHADOWMAP_RES,
				0,
				GL_RED, GL_FLOAT);
	}

	static const Vector forwards[] = {
		Vector(1, 0, 0), //positive x
		Vector(-1, 0, 0),//negative x
		Vector(0, 1, 0), //positive y
		Vector(0, -1, 0),//negative y
		Vector(0, 0, 1),//positive z
		Vector(0, 0, -1)//negative z
	};

	static const Vector ups[] = {
		Vector(0, -1, 0), //positive x
		Vector(0, -1, 0),//negative x
		Vector(0, 0, 1),//positive y
		Vector(0, 0, -1),//negative y
		Vector(0, -1, 0),//positive z
		Vector(0, -1, 0)//negative z
	};

	static const Vector cubemap_angles[] = {
		Vector(0, 0, 0),   // positive x
		Vector(0, 180, 0), // negative x
		Vector(0, 90, 0),  // positive y
		Vector(0, 270, 0), // negative y
		Vector(-90, 0, 0), // positive z
		Vector(-270, 0, 0) // negative z
	};

	glm::vec3 lightPos = glm::vec3(m_pCurrentDynLight->origin.x, m_pCurrentDynLight->origin.y, m_pCurrentDynLight->origin.z);

	m_vRenderOrigin = m_pCurrentDynLight->origin;

	if ((m_pCurrentDynLight->flags & LIGHT_BRUSH_SHADOW) || (m_pCurrentDynLight->flags & LIGHT_WORLD_SHADOW))
	{

		r_oldviewleaf = nullptr;

		clientmleaf_t* spotlight_leaf = Mod_PointInLeaf(m_vRenderOrigin);

		R_MarkLeaves(spotlight_leaf);
	}

	constexpr float fov = 90;	   // degrees
	constexpr float aspect = 1.0f; // square spotlight texture
	constexpr float nearPlane = 8.0f;
	float farPlane = m_pCurrentDynLight->radius;
	m_ProjectionMatrix = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);

	for(int i = 0; i < 6; i++)
	{
		m_pCurrentDynLight->cubedepth->InitRendering(Vector(1, 1, 0));
		
		Vector forward_ = forwards[i];
		Vector up_ = ups[i];
		
		m_vViewAngles = m_pCurrentDynLight->angles = cubemap_angles[i];
		
		gHUD.viewFrustum.SetFrustum(m_pCurrentDynLight->angles, m_pCurrentDynLight->origin, 90, m_pCurrentDynLight->radius);
		
		glm::vec3 forward = glm::vec3(forward_.x, forward_.y, forward_.z);
		glm::vec3 up = glm::vec3(up_.x, up_.y, up_.z);
		
		m_ViewMatrix = glm::lookAt(lightPos, lightPos + forward, up);
		
		// start rendering stuff
		if  ( (m_pCurrentDynLight->flags & LIGHT_BRUSH_SHADOW) || (m_pCurrentDynLight->flags & LIGHT_WORLD_SHADOW) )
			DrawWorldSolid();
		
		if (m_pCurrentDynLight->flags & LIGHT_STUDIOMDL_SHADOW)
		{
			g_StudioRenderer.StudioDrawModelsSolid();

			gPropManager.RenderPropsSolid();
		}
		
		m_iNumTotalShadows++;
	}

	m_pCurrentDynLight->cubedepth->FinishRendering();

	if ((m_pCurrentDynLight->flags & LIGHT_BRUSH_SHADOW) || (m_pCurrentDynLight->flags & LIGHT_WORLD_SHADOW))
	{
		R_MarkLeaves(m_pViewLeaf);
	}
}

#define SHADOWMAP_PASS_TIME "ShadowMap_Pass_RenderTime"

/*
====================
DrawWorldSolid

====================
*/
void CBSPRenderer::DrawWorldSolid(void)
{
	// Advance frame count here
	m_iFrameCount++;

	m_pCurrentEntity = gEngfuncs.GetEntityByIndex(0);
	m_vVecToEyes = m_vRenderOrigin;

	auto curdlight = m_pCurrentDynLight;

	auto projviewmatrix = glm::value_ptr(m_ProjectionMatrix * m_ViewMatrix);
	auto light_pos = glm::value_ptr(glm::vec4(curdlight->origin.x, curdlight->origin.y, curdlight->origin.z, curdlight->radius));

	s_BSPSolidShader.Bind();
	s_BSPSolidShader.UniformMatrix4fv(eWorldSolidShader_locs[worldsolid_projviewmatrix], 1, false, projviewmatrix);
	s_BSPSolidShader.UniformMatrix4fv(eWorldSolidShader_locs[worldsolid_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));
	s_BSPSolidShader.Uniform4fv(eWorldSolidShader_locs[worldsolid_light_pos], 1, light_pos);

	m_pBSPMesh->BindMesh();

	BindGLTexture(SURFTEXTURE_TEXUNIT, 0);
	s_BSPSolidShader.Uniform1i(eWorldSolidShader_locs[worldsolid_alphatest], 0); //cunt

	if (!m_bSunShadowMapPass && (m_pCurrentDynLight->flags & LIGHT_WORLD_SHADOW))
	{
		RecursiveWorldNodeSolid(BSPWorld_Model::m_pWorldNodes);

		glMultiDrawArrays(GL_TRIANGLES, (GLint*)multidraw_startverts, (GLint*)multidraw_numverts, num_multidraws);
		num_multidraws = 0;

	}


	//todo: organize entities by distance
	if (g_StudioRenderer.m_pCvarDrawEntities->value && (m_pCurrentDynLight->flags & LIGHT_BRUSH_SHADOW))
	{
		for (int i = 0; i < m_iNumRenderEntities; i++)
		{
			cl_entity_t* ent = m_pRenderEntities[i];
			if (!IsEntityTransparent(ent))
				DrawBrushModelSolid(ent);
		}
	}

	GL_Mesh::UnbindMesh();
}

/*
====================
RecursiveWorldNodeSolid

====================
*/
void CBSPRenderer::RecursiveWorldNodeSolid(clientmnode_t* node)
{
	int c, side;
	mplane_t* plane;
	clientmsurface_t *surf, **mark;
	clientmleaf_t* pleaf;
	float dot;

	while (true)
	{

		if (node->contents == CONTENTS_SOLID)
			return; // solid

		if (node->visframe != r_visframecount && r_visframecount != -2)
			return;

		if (gHUD.viewFrustum.CullBox(node->minmaxs, node->minmaxs + 3))
			return;

		// if a leaf node, draw stuff
		if (node->contents < 0)
		{
			pleaf = (clientmleaf_t*)node;
			mark = pleaf->firstmarksurface;
			c = pleaf->nummarksurfaces;

			if (c)
			{
				do
				{
					(*mark)->visframe = m_iFrameCount;
					mark++;
				} while (--c);
			}
			return;
		}

		// node is just a decision point, so go down the apropriate sides
		// find which side of the node we are on
		plane = node->plane;
		if (plane->type <= PLANE_Z)
		{
			dot = m_vRenderOrigin[plane->type] - plane->dist;
		}
		else
		{
			DotProductSub(&dot, &m_vRenderOrigin, &plane->normal, &plane->dist);
		}

		side = dot >= 0 ? 0 : 1;

		// recurse down the children, front side first
		RecursiveWorldNodeSolid(node->children[side]);

		// draw stuff
		c = node->numsurfaces;
		if (c)
		{
			surf = BSPWorld_Model::m_pWorldSurfaces + node->firstsurface;

			for (int i = 0; i < node->numsurfaces; i++, surf++)
			{
				if (surf->visframe != m_iFrameCount)
					continue;

				// don't backface underwater surfaces, because they warp
				if (!(surf->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
					continue; // wrong side

				if (surf->flags & SURF_DRAWSKY)
					continue;

				if (surf->flags & SURF_DRAWTURB)
					continue;


				int surfaceIndex = surf - BSPWorld_Model::m_pWorldSurfaces;
				brushface_t* pbrushface = m_pSurfacePointersArray[surfaceIndex];

				multidraw_startverts[num_multidraws] = pbrushface->start_vertex;
				multidraw_numverts[num_multidraws] = pbrushface->num_vertexes;

				num_multidraws++;
			}
		}

		// recurse down the back side
		// NOTE: With this while loop, this is identical to just calling
		// RecursiveWorldNodeSolid(node->children[!side]);
		node = node->children[!side];
	}
};

/*
====================
DrawBrushModelSolid

====================
*/
void CBSPRenderer::DrawBrushModelSolid(cl_entity_t* pEntity)
{
	Vector mins, maxs;
	int i;
	clientmsurface_t* psurf;
	float dot;
	mplane_t* pplane;
	bool bRotated = false;

	m_pCurrentEntity = pEntity;
	model_t* pModel = m_pCurrentEntity->model;

	if (m_pCurrentEntity->model == engine_cl->worldmodel || m_pCurrentEntity->model->type != mod_brush)
		return;

	if (m_pCurrentEntity->curstate.rendermode != NULL && m_pCurrentEntity->curstate.renderamt == NULL)
		return;

	if (m_pCurrentEntity->angles[0] || m_pCurrentEntity->angles[1] || m_pCurrentEntity->angles[2])
	{
		bRotated = true;
		for (i = 0; i < 3; i++)
		{
			mins[i] = m_pCurrentEntity->origin[i] - pModel->radius;
			maxs[i] = m_pCurrentEntity->origin[i] + pModel->radius;
		}
	}
	else
	{
		VectorAdd(m_pCurrentEntity->origin, pModel->mins, mins);
		VectorAdd(m_pCurrentEntity->origin, pModel->maxs, maxs);
	}

	if (gHUD.viewFrustum.CullBox(mins, maxs))
		return;

	VectorSubtract(m_vRenderOrigin, m_pCurrentEntity->origin, m_vVecToEyes);

	if (bRotated)
	{
		Vector temp;
		Vector forward, right, up;

		VectorCopy(m_vVecToEyes, temp);
		AngleVectors(m_pCurrentEntity->angles, &forward, &right, &up);
		m_vVecToEyes[0] = DotProduct(temp, forward);
		m_vVecToEyes[1] = DotProduct(temp, right);
		m_vVecToEyes[2] = DotProduct(temp, up);
		m_vVecToEyes[1] = -m_vVecToEyes[1];
	}

	m_pCurrentEntity->visframe = m_iFrameCount;

	glm::mat4 oldmodelmatrix = m_ModelMatrix;

	if (IsEntityMoved(m_pCurrentEntity))
	{

		glm::vec3 entityangles = glm::vec3(m_pCurrentEntity->angles.x, m_pCurrentEntity->angles.y, m_pCurrentEntity->angles.z);
		glm::vec3 viewangles = glm::vec3(m_vViewAngles.x + m_RefParams.punchangle.x, m_vViewAngles.y + m_RefParams.punchangle.y, m_vViewAngles.z + m_RefParams.punchangle.z);
		Vector forward_, up_;
		AngleVectors(Vector(viewangles.x, viewangles.y, viewangles.z), &forward_, nullptr, &up_);

		glm::vec3 forward = glm::vec3(forward_.x, forward_.y, forward_.z);

		glm::mat4 modelview = glm::mat4(1.0f);
		modelview = glm::translate(modelview, glm::vec3(m_pCurrentEntity->origin.x, m_pCurrentEntity->origin.y, m_pCurrentEntity->origin.z));
		modelview = glm::rotate(modelview, glm::radians(entityangles.y), glm::vec3(0.0f, 0.0f, 1.0f));
		modelview = glm::rotate(modelview, glm::radians(entityangles.x), glm::vec3(0.0f, 1.0f, 0.0f));
		modelview = glm::rotate(modelview, glm::radians(entityangles.z), glm::vec3(1.0f, 0.0f, 0.0f));

		m_ModelMatrix = modelview;

		s_BSPSolidShader.UniformMatrix4fv(eWorldSolidShader_locs[worldsolid_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));
	}

	s_BSPSolidShader.Uniform1i(eWorldSolidShader_locs[worldsolid_alphatest], 1); // cunt

	psurf = &BSPWorld_Model::m_pWorldSurfaces[pModel->firstmodelsurface];
	for (i = 0; i < pModel->nummodelsurfaces; i++, psurf++)
	{
		pplane = psurf->plane;
		DotProductSub(&dot, &m_vVecToEyes, &pplane->normal, &pplane->dist);
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			psurf->visframe = m_iFrameCount;

			if (psurf->flags & SURF_DRAWSKY)
				continue;

			if (psurf->flags & SURF_DRAWTURB)
				continue;

			BindGLTexture(SURFTEXTURE_TEXUNIT, psurf->texinfo->texture->gl_texturenum);
			DrawPolyFromArray(BSPWorld_Model::m_pWorldSurfaces, psurf);
		}
	}

	s_BSPSolidShader.Uniform1i(eWorldSolidShader_locs[worldsolid_alphatest], 1); // cunt cunt cunt

	m_ModelMatrix = oldmodelmatrix;

	s_BSPSolidShader.UniformMatrix4fv(eWorldSolidShader_locs[worldsolid_modelmatrix], 1, GL_FALSE, glm::value_ptr(m_ModelMatrix));
}