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

#pragma once

#include "PlatformHeaders.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "dlight.h"
#include "parsemsg.h"
#include "cvardef.h"
#include "textureloader.h"
#include "rendererdefs.h"
#include "opengl_utils/GL_VertexArrayObject.h"
#include "opengl_utils/GL_ShaderProgram.h"

#include "BSPModel_Gen.h"

#include <memory>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#undef clamp

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "stb_image_write.h"

//==============================
//		BSP RENDERER DEFS
//
//==============================
#define MAX_DECALTEXTURES 128
#define MAX_CUSTOMDECALS 4096
#define MAX_STATICDECALS 1024
#define MAX_GROUPENTRIES 64
#define MAX_DECAL_MSG_CACHE 256
#define MAX_DECAL_GROUPS 256
#define MAX_LIGHTMAPS 256
#define MAX_LIGHTSTYLES 64
#define MAX_STYLESTRING 64
#define MAX_DYNLIGHTS 64
#define MAX_MAP_DETAILOBJECTS 512
#define MAX_MAP_LEAFS 65534
#define DEPTHMAP_RESOLUTION 256
#define MAX_MAP_TEXTURES 1024
#define LIGHTMAP_RESOLUTION 1024

#define BLOCK_SIZE 128

#define LIGHTMAP_NUMCOLUMNS (LIGHTMAP_RESOLUTION / BLOCK_SIZE)
#define LIGHTMAP_NUMROWS (LIGHTMAP_RESOLUTION / BLOCK_SIZE)
#define MAX_SPOTLIGHT_TEXTURES 16

#define MAX_GOLDSRC_DLIGHTS 32
#define MAX_GOLDSRC_ELIGHTS 64

#define SURF_PLANEBACK 2
#define SURF_DRAWSKY 4
#define SURF_DRAWSPRITE 8
#define SURF_DRAWTURB 0x10
#define SURF_DRAWTILED 0x20
#define SURF_DRAWBACKGROUND 0x40
#define SURF_UNDERWATER 0x80
#define SURF_DONTWARP 0x100

#define TEXTURE_SCROLL (1 << 0)
#define TEXTURE_SPECULAR (1 << 1)

#define BLOCKLIGHTS_SIZE (18 * 18)
#define BACKFACE_EPSILON 0.01

#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2

#define LIGHTMAP_TEXUNIT GL_TEXTURE0
#define SURFTEXTURE_TEXUNIT GL_TEXTURE1
#define SPOTLIGHT_TEXUNIT GL_TEXTURE2
#define SHADOWMAP_TEXUNIT GL_TEXTURE3
#define CUBEMAPSHADOW_TEXUNIT GL_TEXTURE4

#define DEFAULT_SHADOWMAP_RES 256

#define CHECKVISBIT( vis, b )		((b) >= 0 ? (byte)((vis)[(b) >> 3] & (1 << ((b) & 7))) : (byte)false )
#define SETVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] |= (1 << ((b) & 7))) : (byte)false )
#define CLEARVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] &= ~(1 << ((b) & 7))) : (byte)false )

#define LIGHT_STUDIOMDL_SHADOW 2 << 0	// cast shadows from studiomdl entities (renderfx = 1)
#define LIGHT_BRUSH_SHADOW 2 << 1		// casts shadows from non-static brush entities (renderfx = 2)
#define LIGHT_WORLD_SHADOW 2 << 2		// casts shadows from static world brushes (the entire world basically) (renderfx = 3)

#define LIGHT_CLIENT_SUNSHADOW 2 << 2 // casts shadows from static world brushes (the entire world basically) (renderfx = 3)

#define LIGHT_ONLYSHADOWS 2 << 3

#define LIGHT_CASTSHADOWS (LIGHT_STUDIOMDL_SHADOW | LIGHT_BRUSH_SHADOW | LIGHT_WORLD_SHADOW)

//========================================
//			BSP RENDERER STRUCTS
//
//========================================

struct Default3DTextureVert_t
{
	Vector pos;
	float texcoord[2];
	float lmcoord[2];
	byte _padding[12];

	GL_DECLARE_ATTRIBLIST();
};

struct Default3DVert_t
{
	Vector pos;
	Vector normal;
	float texcoord[2];

	GL_DECLARE_ATTRIBLIST();
};

struct brushvertex_t
{
	Vector pos;
	Vector normal;

	float fogcoord;
	float texcoord[2];
	float speculartexcoord[2];
	float lightmaptexcoord[2];

	byte pad[12];

	GL_DECLARE_ATTRIBLIST();
};

struct brushface_t
{
	int index;
	int start_vertex;
	int num_vertexes;

	Vector normal;
	Vector s_tangent;
	Vector t_tangent;
};

#define DECALFLAG_TRANSDECAL (1 << 0)
struct decalgroupentry_t
{
	char szName[64];
	int gl_texid;
	int xsize, ysize;
	int flags;
	struct decalgroup_t* group;
};
struct decalgroup_t
{
	char szName[64];
	int iSize;
	decalgroupentry_t entries[MAX_GROUPENTRIES];
};

typedef struct customdecalvert_s
{
	Vector position;
	float texcoord[2];
	float lmcoord[2];
} customdecalvert_t;

typedef struct customdecalpoly_s
{
	customdecalvert_t* pverts;
	int numverts;

	clientmsurface_t* surface;
	cl_entity_t* entity;
} customdecalpoly_t;

typedef struct customdecal_s
{
	customdecalpoly_t* polys;
	int inumpolys;

	const decalgroupentry_t* texinfo;

	Vector normal;
	Vector position;
	float life;
} customdecal_t;

struct decal_msg_cache
{
	Vector pos;
	Vector normal;
	char name[32];
	int persistent;
	int fromwad;
	float angle;
};

struct clientsurfdata_t
{
	float cached_light[MAXLIGHTMAPS];

	texture_t* regtexture;

	int light_s;
	int light_t;
};

typedef struct
{
	int length;
	char map[MAX_STYLESTRING];
} lightstyle_t;

struct detailobject_t
{
	Vector mins;
	Vector maxs;

	int firstsurface;
	int numsurfaces;

	short leafnums[MAX_ENT_LEAFS * 2];
	int numleafs;

	int visframe;
	int rendermode;
};

struct cl_dlight_t
{
	Vector origin;
	Vector color;
	Vector angles;

	float radius;
	float die;
	float decay;
	bool justspawned; //NEW!! for flashlights
	int flags;
	int key;

	GL_ShadowMap* depth;
	GL_ShadowMap* cubedepth; //cubemap shadowmap (for pointlights), probably should be made a separate entity.

	int visframe;

	// spotlight specific:
	float cone_size;
	FrustumCheck frustum;
	int textureindex;
};

class GL_FBOHandler;
class GL_RBOHandler;
class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;
class GL_Mesh;

/*
====================
CBSPRenderer

====================
*/
class CBSPRenderer
{
public:
	void Init(void);
	void VidInit(void);
	void Shutdown(void);
	void SetupRenderer(void);
	void SetupPreFrame(ref_params_t* vieworg);
	void CheckTextures(void);

	void Make_ShadowMaps(void);
	void Generate_Sun_Shadow(void);
	void Generate_Spotlight_Shadow(void);
	void Generate_Pointlight_Shadow(void);
	void DrawWorldSolid(void);

	void RecursiveWorldNodeSolid(clientmnode_t* node);
	void DrawBrushModelSolid(cl_entity_t* pEntity);

	bool IsInPotentiallyVisibleSet(int visframe) { return CHECKVISBIT(m_pPVS, visframe); };

	Vector TriWorldToScreen(Vector point);

	void RendererRefDef(ref_params_t* pparams);
	void DrawNormalTriangles(bool draw_world = true);
	void DrawNormalTriangles_Cheap(bool draw_world = true, bool draw_ents = true);
	void DrawTransparentTriangles(void);
	void RenderFirstPass();
	void RenderFinalPasses();
	void RenderWireframe(void);
	void DrawWorld(bool m_bSkyBox = false);

	void SetClippingPlane(const mplane_t& plane);

	void RenderSunShadow(void);

	void LoadWADDecals(void);

	void GetRenderEnts(void);
	void AddEntity(cl_entity_t* pEntity);
	int FilterEntities(int type, struct cl_entity_s* pEntity, const char* modelname); //

	void DecayLights(void);
	bool HasDynLights(void);
	void GetAdditionalLights(void);
	cl_dlight_t* CL_AllocDLight(int key);

	int MsgDynLight(const char* pszName, int iSize, void* pbuf);

	void SetupDynLight(void);
	void FinishDynLight(void);
	void SetDynLightBBox(void);
	void SetupSpotLight(void);
	void FinishSpotLight(void);
	bool LightCanShadow(void);
	int CullDynLightBBox(Vector mins, Vector maxs);

	void CreateTextures(void);

	void FreeBuffer(void);
	void GenerateVertexArray(void);

	void DrawBrushModel(cl_entity_t* pEntity, bool bStatic = false);
	void RecursiveWorldNode(clientmnode_t* node);

	void UpdateLightStylesLM();

	void SurfaceToChain(clientmsurface_t* psurfbase, clientmsurface_t* s);
	void DrawPolyFromArray(clientmsurface_t* pbaseptr, clientmsurface_t* psurf);

	bool DynamicLighted(const Vector& vmins, const Vector& vmaxs);
	void DrawDynamicLightsForWorld(void);
	void RecursiveWorldNodeLight(clientmnode_t* node);
	void DrawDynamicLightsForEntity(cl_entity_t* pEntity);
	void DrawEntityFacesForLight(cl_entity_t* pEntity);

	void InitSky(void);
	void DrawSky(void);
	void RemoveSky(void);
	int MsgSkyMarker_Sky(const char* pszName, int iSize, void* pbuf);
	int MsgSkyMarker_World(const char* pszName, int iSize, void* pbuf);

	void ClearSurfaceDrawChain(void);

	bool ExtensionSupported(const char* ext);

	void AnimateLight(void);
	void UploadLightmaps(void);
	void BuildLightmap(clientmsurface_t* surf, int surfindex, color24* out);
	void AddLightStyle(int iNum, const char* szStyle);

	void BindGLTexture(GLenum texture, GLuint id);

	texture_t* TextureAnimation(texture_t* base, int frame);

public:
	void DrawDecals(bool m_bTransPass = false);
	void LoadDecals(void);
	void DeleteDecals(void);

	decalgroup_t* FindGroup(const char* _name);
	cl_texture_t* LoadDecalTexture(const char* texname);
	decalgroupentry_t* GetRandomDecal(decalgroup_t* group);
	decalgroupentry_t* FindDecalByName(const char* szName);

	bool CullDecalBBox(Vector mins, Vector maxs);
	void CreateDecal(Vector endpos, Vector pnormal, const char* name, int persistent = 0, bool fromwad = false, float angle = 0);
	void RecursiveCreateDecal(mnode_t* node, decalgroupentry_t* texptr, customdecal_t* pDecal, Vector endpos, Vector pnormal, float angle = 0);
	void DecalSurface(clientmsurface_t* surf, decalgroupentry_t* texptr, cl_entity_t* pEntity, customdecal_t* pDecal, Vector endpos, Vector pnormal, float angle = 0);

	int MsgCustomDecal(const char* pszName, int iSize, void* pbuf);

	void CreateCachedDecals(void);
	void DrawSingleDecal(customdecal_t* decal, std::vector<Default3DTextureVert_t>& decalvertlist, bool m_bTransPass = false, bool *bNeedsBufferUpdate = nullptr);

	customdecal_t* AllocDecal(void);
	customdecal_t* AllocStaticDecal(void);

	void GetUpRight(Vector forward, Vector& up, Vector& right);
	int ClipPolygonByPlane(const Vector* arrIn, int numpoints, Vector normal, Vector planepoint, Vector* arrOut);
	void FindIntersectionPoint(const Vector& p1, const Vector& p2, const Vector& normal, const Vector& planepoint, Vector& newpoint);

public:

	enum QuadDebug_Sections
	{
		quad_FullScreen = 0,
		quad_TopRight = 6,
		quad_BottomRight = 12,
		quad_BottomLeft = 18,
		quad_TopLeft = 24
		
	};

	brushvertex_t* m_pBufferData;
	brushface_t* m_pFacesExtraData;

	GL_Mesh* m_pBSPMesh;
	GL_Mesh* m_p2DScreenMesh;

	GL_TextureHandler* m_pMainFBOTexture;
	GL_FBOHandler* m_pMainFBO;
	GL_RBOHandler* m_pMainRBO;
	
	int m_iNumTotalShadows; // total number of shadowmaps made in this frame

	cl_entity_t* m_pCurrentEntity;
	cl_dlight_t* m_pCurrentDynLight;
	byte* m_pPVS;
	clientmleaf_t* m_pViewLeaf;

	cl_entity_t* m_pWorldEnt;

	dlight_t* m_pFirstDLight;
	dlight_t* m_pFirstELight;

	ref_params_t m_RefParams;

	int m_iTotalVertCount;
	int m_iTotalFaceCount;
	int m_iTotalTriCount;

	int m_iTexPointer[4];
	int m_iEnvStates[4];
	int m_iTUSupport;

	int m_iFrameCount;

	cl_texture_t* m_pFlashlightTextures[MAX_SPOTLIGHT_TEXTURES];
	int m_iNumFlashlightTextures;

	bool m_bDrawSky;
	bool m_bMirroring;
	bool m_bLightShadow;
	bool m_bMainPass;
	bool m_bSunShadowMapPass;

	bool m_bReloaded;
	bool m_bGotAdditional;
	bool m_bGotStaticLights;

	bool m_bFullBright;

	bool m_bLoaded_decal_wad = false;

	Vector m_vRenderOrigin;
	Vector m_vViewAngles;
	Vector m_vVecToEyes;

	Vector m_vSkyOrigin;
	Vector m_vSkyWorldOrigin;

	Vector m_vCurDLightOrigin;
	Vector m_vCurSpotForward;

	cvar_t* m_pCvarSpeeds;
	cvar_t* m_pCvarDynamic;
	cvar_t* m_pCvarDrawWorld;
	cvar_t* m_pCvarLightStyles;
	cvar_t* m_pCvarWireFrame;
	cvar_t* m_pCvarShadows;
	cvar_t* m_pCvarFlashLightDepthRes;
	cvar_t* m_pCvarLightmapDebug;
	cvar_t* m_pCvar3DSkybox;
	cvar_t* m_pCvarSunShadowsQuality;
	cvar_t* m_pCvarBlurShadows;

	cvar_t* m_pCvarBlacknwhite;
	cvar_t* m_pCvarFilmGrainStrength;


	cvar_t* lightgamma;
	cvar_t* texgamma;
	cvar_t* r_fullbright;
	cvar_t* gl_fog;
	cvar_t* gl_widescreen_yfov;

	Vector m_vDLightMins;
	Vector m_vDLightMaxs;

	float m_fSkySpeed;

	cl_entity_t* m_pRenderEntities[MAXRENDERENTS];
	int m_iNumRenderEntities;

	std::vector<std::unique_ptr<cl_dlight_t>> m_pDynLights;

	GL_ShadowMap* m_pSunShadowMap;

	mlight_t m_pModelLights[MAXRENDERENTS];
	int m_iNumModelLights;

	lightstyle_t m_pLightStyles[MAX_LIGHTSTYLES];
	int m_iLightStyleValue[MAX_LIGHTSTYLES];

	color24 m_pBlockLights[BLOCKLIGHTS_SIZE];
	int m_iNumLightmaps;

	color24 m_pEngineLightmaps[MAX_LIGHTMAPS * BLOCK_SIZE * BLOCK_SIZE];
	GLuint m_iEngineLightmapIndex;

	brushface_t** m_pSurfacePointersArray;

	clientsurfdata_t* m_pSurfaces;
	int m_iNumSurfaces;

	int m_iSkyTextures[6];

	int m_iBSPVertsCounter;
	int m_iBrushPolyCounter;  // bmodel poly counter
	int m_iStudioPolyCounter; // studiomodel poly counter

	double m_fShadowGenerationTime;
	double m_fMainWorldRenderTime;

	int m_iSunShadow_Strength;
	int m_iSunShadow_FadeDist;

	char m_szSkyName[64];
	char m_szMapName[64];

	texture_t m_pNormalTextureList[MAX_MAP_TEXTURES];
	int m_iNumTextures;


	GL_ShaderProgram *m_SimpleSkyboxShader;
	static GL_ShaderProgram m_FilterShader;
	static GL_ShaderProgram m_BlacknwhiteShader;

	glm::mat4 m_ProjectionMatrix; //	fov, aspect, near, far
	glm::mat4 m_ViewMatrix;  //	camera position, camera angles
	glm::mat4 m_ModelMatrix;		//	moving entities


public:
	std::vector<std::unique_ptr<customdecal_t>> m_pDecals;
	std::vector<std::unique_ptr<customdecal_t>> m_pStaticDecals;
	std::vector<std::unique_ptr<decal_msg_cache>> m_pMsgCache;
	std::vector<std::unique_ptr<decalgroup_t>> m_pDecalGroups;
	std::vector<decalgroupentry_t> m_WADDecals;

	Vector m_vDecalMins;
	Vector m_vDecalMaxs;
};
extern CBSPRenderer gBSPRenderer;