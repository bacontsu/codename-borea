#include <filesystem>
#include "stdio.h"
#include "stdlib.h"
#include <string>
#include <vector>

#include <codecvt>
#include <locale>
#include <string>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "com_model.h"

// imgui
#include "PlatformHeaders.h"
#include <Psapi.h>
#include "SDL2/SDL.h"
#include <gl/GL.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_sdl.h"

#include "bsprenderer.h"

SDL_Window* mainWindow;
SDL_GLContext mainContext;

CImguiManager g_ImGUIManager;


ImFont* subtitlefont = nullptr;
ImFont* titlefont = nullptr;

#define STB_IMAGE_IMPLEMENTATION

#define FONT_BASESCALEWIDTH 1366.f
#define FONT_BASESCALEHEIGHT 768.f
#define FONT_SCALEFACTOR 20.f
#include "stb_image.h"

static float getTitleTextScale(){
	return (ScreenWidth/FONT_BASESCALEWIDTH) * FONT_SCALEFACTOR;
}

static std::string getCvarFont(cvar_t* fontcvar)
{
	std::string fontfile = std::string(fontcvar->string);
	if (fontfile.find(".ttf") == std::string::npos)
		return "Liberation Serif.ttf";

	return fontfile;
}

// Simple helper function to load an image into a OpenGL texture with common settings
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width = nullptr, int* out_height = nullptr, int filter = GL_LINEAR, int wrap = GL_CLAMP_TO_EDGE)
{
	// Load from file
	int image_width = 0;
	int image_height = 0;
	unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
		return false;

	// Create a OpenGL texture identifier
	GLuint image_texture;
	glGenTextures(1, &image_texture);
	glBindTexture(GL_TEXTURE_2D, image_texture);

	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap); // This is required on WebGL for non power-of-two textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap); // Same

	// Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
	stbi_image_free(image_data);

	*out_texture = image_texture;

	if (out_width)
		*out_width = image_width;
	if (out_height)
		*out_height = image_height;

	return true;
}

void PRECACHE_IMAGE(std::string name, GLuint* texture, int* x = nullptr, int* y = nullptr, int filter = GL_LINEAR, int wrap = GL_CLAMP_TO_EDGE)
{
	std::string path = gEngfuncs.pfnGetGameDirectory() + std::string("/resource/") + name;
	bool pathCheck = LoadTextureFromFile(path.c_str(), texture, x, y, filter, wrap);

	/*
	if (std::filesystem::exists(path))
		gEngfuncs.Con_DPrintf("%s is found!\n", path.c_str());
	else
		gEngfuncs.Con_DPrintf("%s cant be found!\n", path.c_str());
		*/
	//	IM_ASSERT(pathCheck);
}

void ClientImGui_HookedDraw()
{
	//moved to pfnFrameRender2 in cdll_int

	//g_ImGUIManager.Draw();
	//SDL_GL_SwapWindow(mainWindow);
}

int ClientImGui_EventWatch(void* data, SDL_Event* event)
{
	return ImGui_ImplSDL2_ProcessEvent(event);
}

void ClientImGui_Init()
{

	ImGui_ImplOpenGL2_Init();
	ImGui_ImplSDL2_InitForOpenGL(mainWindow, ImGui::GetCurrentContext());

	SDL_AddEventWatch(ClientImGui_EventWatch, nullptr);

}

// image loading
int binX, binY, NewGameSizeX, NewGameSizeY, ExitSizeX, ExitSizeY, startSizeX, startSizeY, cancelSizeX, cancelSizeY, nextSizeX, nextSizeY,
	backSizeX, backSizeY;
GLuint newgame = 0;
GLuint exitbtn = 0;
GLuint startbtn;
GLuint cancelbtn;
GLuint nextbtn, backbtn;

GLuint thumbnail = 0;
GLuint thumbnail2 = 0;
GLuint thumbnail3 = 0;
GLuint thumbnail4 = 0;
GLuint thumbnail5 = 0;
GLuint thumbnail6 = 0;

GLuint noise1 = 0;
GLuint noise2 = 0;

std::vector<subtitlelist_t> subtitles_vector_client;

void CImguiManager::ReloadSubtitles()
{
	int iFlags = 0;
	char szFlag[32];
	char szSentence[32];
	char szText[1024];
	char szTime[32];

	int iSize = NULL;

	std::string path = "sound/";
	path += gEngfuncs.pfnGetCvarString("r_subtitleslang");

	std::vector<std::byte> textfile = FileSystem_LoadFileIntoBuffer(path.c_str(), FileContentFormat::Text);
	iSize = textfile.size();
	char* pFile = (char*)textfile.data();

	if (textfile.empty())
	{
		gEngfuncs.Con_Printf("Could not load subtitles!\n");
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	ImFontConfig config;
	config.OversampleH = 2; // Horizontal oversampling
	config.OversampleV = 1; // Vertical oversampling
	config.GlyphExtraSpacing.x = 1.0f; // Extra spacing between glyphs
	static const ImWchar full_glyph_range[] = {
		0x0020, 0xFFFF,
		0,
	};

	std::string fontpath = gEngfuncs.pfnGetGameDirectory() + std::string("/resource/CustomFontFiles/" + getCvarFont(r_subtitlesfont));
	subtitlefont = io.Fonts->AddFontFromFileTTF(fontpath.c_str(), 20.0f, &config, full_glyph_range);

	fontpath = gEngfuncs.pfnGetGameDirectory() + std::string("/resource/CustomFontFiles/" + getCvarFont(r_titlesfont));
	titlefont = io.Fonts->AddFontFromFileTTF(fontpath.c_str(), 20.0f, &config, full_glyph_range);


	ImGui_ImplOpenGL2_DestroyFontsTexture();

	subtitles_vector_client.clear();

	int i = NULL;

	auto ReadToken = [&](char* dest, int maxlen) -> bool
		{
			int j = 0;

			if (i >= iSize || pFile[i] == '\n' || pFile[i] == '\r')
				return false;

			if (pFile[i] == '/' && pFile[i + 1] == '/')
			{
				if (i == 0)
					return false;
				else if (pFile[i - 1] == '\n')
					return false;
			}

			// Skip whitespace — but stop at newlines
			while (i < iSize && (pFile[i] == ' ' || pFile[i] == '\t'))
				i++;

			if (i >= iSize || pFile[i] == '\n' || pFile[i] == '\r')
				return false;

			bool quoted = false;

			if (pFile[i] == '"')
			{
				quoted = true;
				i++;
			}

			while (i < iSize)
			{
				if (quoted)
				{
					if (pFile[i] == '"')
					{
						i++;
						break;
					}
				}
				else
				{
					if (pFile[i] == ' ' || pFile[i] == '\n' || pFile[i] == '\r')
						break;
				}

				if (j < maxlen - 1)
				{
					char test = pFile[i];

					//shitty and hacky utf-8 conversion
					if ((byte)pFile[i] == 0xe2)
					{
						if ((byte)pFile[i + 1] == 0x80)
						{
							if ((byte)pFile[i + 2] == 0x99)
							{
								test = '\'';
								i += 2;
							}
							else if ((byte)pFile[i + 2] == 0x9c)
							{
								test = '\"';
								i += 2;
							}
							else if ((byte)pFile[i + 2] == 0x9d)
							{
								test = '\"';
								i += 2;
							}
							else if ((byte)pFile[i + 2] == 0xa6)
							{
								dest[j++] = '.';
								dest[j++] = '.';
								dest[j++] = '.';
								i += 2;
								i++;
							}
							else if ((byte)pFile[i + 2] == 0x93)
							{
								test = '-';
								i += 2;
							}
						}
					}

					dest[j++] = test;
				}
				i++;
			}

			dest[j] = 0;

			//while (i < iSize && (pFile[i] == ' ' || pFile[i] == '\n' || pFile[i] == '\r'))
			//	i++;

			return true;
		};

	while (1)
	{
		// Reset
		iFlags = 0;

		if (i >= iSize)
			break;

		if (!ReadToken(szSentence, sizeof(szSentence)))
		{
			if (pFile[i] == '/')
			{
				while (i < iSize)
				{
					if (pFile[i] != '\n' && pFile[i] != '\r')
						i++;
					else
						break;
				}
				continue;
			}

			while (i < iSize && (pFile[i] == '\n' || pFile[i] == '\r'))
				i++;
			continue;
		}

		if (!ReadToken(szText, sizeof(szText)))
			break;

		if (!ReadToken(szTime, sizeof(szTime)))
			break;

		subtitlelist_t newsubtitle{};
		strcpy_s(newsubtitle.sentence, szSentence);
		strcpy_s(newsubtitle.text, szText);
		newsubtitle.staytime = atof(szTime);
		subtitles_vector_client.push_back(newsubtitle);
	}

}

int __MsgFunc_AddSubtitle(const char* pszName, int iSize, void* pbuf)
{
	char subtitles[1024];

	BEGIN_READ(pbuf, iSize);

	const char* text = READ_STRING();
	float staytime = READ_FLOAT();

	if (!text)
		return 1;

	for (auto subtitles_inlist : subtitles_vector_client)
	{
		if (!strstr(text, subtitles_inlist.sentence))
			continue;

		strcpy(subtitles, subtitles_inlist.text);
	}

	g_ImGUIManager.AddSubtitle(subtitles, staytime);

	return 1;
}

extern SDL_Window* hlWindow;

bool CImguiManager::Init()
{
	HOOK_MESSAGE(AddSubtitle);
	mainWindow = hlWindow;
	// mainContext = SDL_GL_CreateContext(mainWindow);


	r_subtitles = CVAR_CREATE("r_subtitles", "1", FCVAR_ARCHIVE);
	r_subtitleslang = CVAR_CREATE("r_subtitleslang", "subtitles_en.txt", FCVAR_ARCHIVE);
	r_subtitlesfont = CVAR_CREATE("r_subtitlesfont", "Liberation Serif.ttf", FCVAR_ARCHIVE);
	r_titlesfont = CVAR_CREATE("r_titlesfont", "Liberation Serif.ttf", FCVAR_ARCHIVE);


	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

	// For Overdraw
	ClientImGui_Init();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	return true;
}

bool CImguiManager::VidInit()
{
	m_iNumTexts = 0;
	memset(m_sTexts, 0, sizeof(m_sTexts));

	ReloadSubtitles();

	return true;
}

void CImguiManager::Draw()
{
	// draw
	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(mainWindow);
	ImGui::NewFrame();

	DrawSpeeds();

	DrawSubtitles();

	DrawExtra();

	// glViewport( 0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y );
	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

	// update discord here
	g_DiscordRPC.Update();
}

void CImguiManager::DrawSpeeds()
{
	if (!gBSPRenderer.m_pCvarSpeeds || gBSPRenderer.m_pCvarSpeeds->value <= 0 )
		return;

	if (!gParticleEngine.m_pCvarParticleDebug)
		return;

	int width = 75;

	if (gBSPRenderer.m_pCvarSpeeds->value == 1.0f)
		width += 165;

	if (gParticleEngine.m_pCvarParticleDebug->value == 1.0f)
		width += 150;


	// setup
	bool is_open;
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoMove;

	// get resolution
	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 350 , 100));
	ImGui::SetNextWindowSize(ImVec2(250, width));

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// override imgui styles
	ImVec4* colours = ImGui::GetStyle().Colors;
	colours[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.4f);

	ImGui::Begin("Codename Borea", &is_open, window_flags);
	ImGui::SetWindowFontScale(1.5f);
	ImGui::TextColored(ImVec4(0.76f, 0.62f, 0.2f, 1), "The Last Goodbye");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::Text("Renderer: ReTrinity");
	std::string version = "OpenGL: " + (std::string)(const char*)glGetString(GL_VERSION);
	ImGui::Text(version.c_str());

	if(gBSPRenderer.m_pCvarSpeeds->value == 1.0f)
	{
		static float flLastTime;
		float flCurTime = engine_cl->time;
		float flFrameTime = flCurTime - flLastTime;
		flLastTime = flCurTime;

		// prevent divide by zero
		if (flFrameTime <= 0)
			flFrameTime = 1;

		if (flFrameTime > 1)
			flFrameTime = 1;

		int iFPS = 1 / flFrameTime;

		ImGui::Text("");
		ImGui::SetWindowFontScale(1.2f);
		ImGui::Text("Polygons:");
		ImGui::SetWindowFontScale(1.0f);
		//ImGui::Text((std::string("Wpolys: ") + std::to_string(gBSPRenderer.m_iWorldPolyCounter)).c_str());
		ImGui::Text((std::string("Epolys: ") + std::to_string(gBSPRenderer.m_iBrushPolyCounter)).c_str());
		ImGui::Text((std::string("Studio polys: ") + std::to_string(gBSPRenderer.m_iStudioPolyCounter)).c_str());
		ImGui::Text((std::string("Particles: ") + std::to_string(gParticleEngine.m_iNumParticles)).c_str());
		//ImGui::Text((std::string("Foliages: ") + std::to_string(gBSPRenderer.m_iTotalFoliage)).c_str());
		//ImGui::Text((std::string("Cables: ") + std::to_string(gBSPRenderer.m_iCable)).c_str());
		ImGui::Text((std::string("FPS: ") + std::to_string(iFPS)).c_str());
	}

	if (gParticleEngine.m_pCvarParticleDebug->value == 1.0f)
	{
		//gEngfuncs.Con_Printf("Created Particles: %i, Freed Particles %i, Active Particles: %i\nCreated Systems: %i, Freed Systems: %i, Active Systems: %i\n\n",
			//m_iNumCreatedParticles, m_iNumFreedParticles, m_iNumCreatedParticles - m_iNumFreedParticles, m_iNumCreatedSystems, m_iNumFreedSystems, m_iNumCreatedSystems - m_iNumFreedSystems);
		ImGui::Text("");
		ImGui::SetWindowFontScale(1.2f);
		ImGui::Text("Particles:");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Text((std::string("Created Particles: ") + std::to_string(gParticleEngine.m_iNumCreatedParticles)).c_str());
		ImGui::Text((std::string("Freed Particle: ") + std::to_string(gParticleEngine.m_iNumFreedParticles)).c_str());
		ImGui::Text((std::string("Active Particles: ") + std::to_string(gParticleEngine.m_iNumCreatedParticles - gParticleEngine.m_iNumFreedParticles)).c_str());
		ImGui::Text((std::string("Created Systems: ") + std::to_string(gParticleEngine.m_iNumCreatedSystems)).c_str());
		ImGui::Text((std::string("Freed Systems: ") + std::to_string(gParticleEngine.m_iNumFreedSystems)).c_str());
		ImGui::Text((std::string("Active Systems: ") + std::to_string(gParticleEngine.m_iNumCreatedSystems - gParticleEngine.m_iNumFreedSystems)).c_str());
	}

	ImGui::End();
}

void CImguiManager::SubtitleLifeLogic()
{
	float curtime = engine_cl->time;
	float lasttime = engine_cl->oldtime;
	float deltatime = curtime - lasttime;
	for (int i = 0; i < m_iNumTexts;)
	{
		if (m_sTexts[i].time_to_die > curtime)
		{
			if (m_sTexts[i].fadeout)
			{
				m_sTexts[i].fade -= deltatime;
			}
			else if (m_sTexts[i].fade < 1.0)
			{
				m_sTexts[i].fade += deltatime;
				if (m_sTexts[i].fade > 1.0)
					m_sTexts[i].fade = 1.0;
			}
			i++;
			continue;
		}

		if(!m_sTexts[i].fadeout)
		{
			m_sTexts[i].time_to_die = curtime + 0.5;
			m_sTexts[i].fadeout = true;
			i++;
			continue;
		}

		for (int j = i + 1; j < m_iNumTexts; j++)
		{
			m_sTexts[j - 1] = m_sTexts[j];
		}

		m_sTexts[m_iNumTexts - 1] = subtitles_t{};
		m_iNumTexts--;
	}
}

void CImguiManager::DrawSubtitles()
{
	SubtitleLifeLogic();

	if (m_iNumTexts == 0 || !r_subtitles->value || !subtitlefont)
		return;

	// setup
	bool is_open;
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoScrollbar;
	window_flags |= ImGuiWindowFlags_NoScrollWithMouse;

	// get resolution
	ImGuiIO& io = ImGui::GetIO();

	//int boxexpand = m_iNumTexts - 1;
	//
	//for (int i = 0; i < m_iNumTexts; i++)
	//{
	//	//text wraps around at 58 characters
	//	float wraps = strlen(m_sTexts[i].text) / 58;
	//	if (wraps > 0.999)
	//	{
	//		boxexpand += wraps;
	//	}
	//}

	int height = io.DisplaySize.y * (0.1);
	int totalheight = 0;
	int width = io.DisplaySize.x * 0.5;

	for (int i = 0; i < m_iNumTexts; i++)
	{
		// Measure the size of the text with wrapping
		ImVec2 textSize = ImGui::CalcTextSize(
			m_sTexts[i].text.c_str(),
			nullptr,
			false,
			width
		);
	
		totalheight += textSize.y;
	}
	
	if (height < totalheight * 2)
	{
		height += (totalheight * 2 - height);
	}

	int xpos = io.DisplaySize.x / 2 - (width / 2);
	int ypos = io.DisplaySize.y - (height)-10;

	ImGui::SetNextWindowPos(ImVec2(xpos, ypos));

	ImGui::SetNextWindowSize(ImVec2(width, height));

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// override imgui styles
	ImVec4* colours = ImGui::GetStyle().Colors;
	colours[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.4f);

	ImGui::Begin("Subtitles", &is_open, window_flags);
	ImGui::SetWindowFontScale(1.2f);

	ImGui::PushTextWrapPos(width);

	ImGui::PushFont(subtitlefont);

	for(int i = 0; i < m_iNumTexts; i++)
	{
		float fade = m_sTexts[i].fade;
		ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.4f, 1 * fade), m_sTexts[i].text.c_str());
	}

	ImGui::PopFont();
	ImGui::PopTextWrapPos();

	ImGui::End();
}

void CImguiManager::AddSubtitle(const char subtitle[256], float staytime)
{
	if (m_iNumTexts >= MAX_SUBTITLES_AT_ONCE)
		return;

	m_sTexts[m_iNumTexts].text = subtitle;
	m_sTexts[m_iNumTexts].time_to_die = engine_cl->time + staytime;
	m_sTexts[m_iNumTexts].fade = 0;
	m_iNumTexts++;
}

struct drawchars_t
{
	int x, y;
	std::string character;
	int r, g, b;
};
static std::vector< drawchars_t> s_drawcmds;

void CImguiManager::DrawExtra()
{
	bool is_open;
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoScrollbar;
	window_flags |= ImGuiWindowFlags_NoScrollWithMouse;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// override imgui styles
	ImVec4* colours = ImGui::GetStyle().Colors;
	colours[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(ScreenWidth, ScreenHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGui::Begin("Titles", &is_open, window_flags);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	for (const auto& cmd : s_drawcmds)
	{
		draw_list->AddText(titlefont, getTitleTextScale(), ImVec2(cmd.x, cmd.y), IM_COL32(cmd.r, cmd.g, cmd.b, (cmd.r + cmd.g + cmd.b) / 3.f), &cmd.character[0], &cmd.character[0] + cmd.character.size());
	}

	ImGui::PopStyleVar();
	ImGui::End();

	s_drawcmds.clear();
}
float CImguiManager::DrawCharacter(int x, int y, const char* character, int r, int g, int b)
{
	s_drawcmds.push_back({ x, y, character, r, g, b });
	return x + titlefont->CalcTextSizeA(getTitleTextScale(), FLT_MAX, 0.0f, &character[0], &character[strlen(character)]).x;
}

float CImguiManager::GetTextWidth(const char* text)
{
	return titlefont->CalcTextSizeA(getTitleTextScale(), FLT_MAX, 0.0f, &text[0], &text[strlen(text)]).x;
}