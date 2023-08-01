

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"

#include <windows.h>
#include <gl/gl.h>

#include "r_studioint.h"

extern engine_studio_api_t IEngineStudio;

#define GL_TEXTURE_RECTANGLE_NV 0x84F5

cvar_t* glow_blur_steps = NULL;
cvar_t* glow_darken_steps = NULL;
cvar_t* glow_strength = NULL;
cvar_t* te_bloom_effect = NULL;

bool CBloom::Init(void)
{
    // create a load of blank pixels to create textures with
    unsigned char* pBlankTex = new unsigned char[ScreenWidth * ScreenHeight * 3];
    memset(pBlankTex, 0, ScreenWidth * ScreenHeight * 3);

    // Create the SCREEN-HOLDING TEXTURE
    glGenTextures(1, &g_uiScreenTex);
    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_uiScreenTex);
    glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB8, ScreenWidth, ScreenHeight, 0, GL_RGB8, GL_UNSIGNED_BYTE, pBlankTex);

    // Create the BLURRED TEXTURE
    glGenTextures(1, &g_uiGlowTex);
    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_uiGlowTex);
    glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB8, ScreenWidth / 2, ScreenHeight / 2, 0, GL_RGB8, GL_UNSIGNED_BYTE, pBlankTex);

    // free the memory
    delete[] pBlankTex;

    glow_blur_steps = CVAR_CREATE("glow_blur_steps", "15", FCVAR_ARCHIVE);
    glow_darken_steps = CVAR_CREATE("glow_darken_steps", "5", FCVAR_ARCHIVE);
    glow_strength = CVAR_CREATE("glow_strength", "1", FCVAR_ARCHIVE);

    te_bloom_effect = CVAR_CREATE("te_bloom_effect", "1", FCVAR_ARCHIVE);

    return true;
}

void CBloom::DrawQuad(int width, int height, int ofsX, int ofsY)
{
    glTexCoord2f(ofsX, ofsY);
    glVertex3f(0, 1, -1);
    glTexCoord2f(ofsX, height + ofsY);
    glVertex3f(0, 0, -1);
    glTexCoord2f(width + ofsX, height + ofsY);
    glVertex3f(1, 0, -1);
    glTexCoord2f(width + ofsX, ofsY);
    glVertex3f(1, 1, -1);
}

void CBloom::Draw(void)
{
    // check to see if (a) we can render it, and (b) we're meant to render it

    if (IEngineStudio.IsHardware() != 1)
        return;

    if ((int)glow_blur_steps->value == 0 || (int)glow_strength->value == 0)
        return;

    if (!(int)te_bloom_effect->value)
        return;

    // enable some OpenGL stuff
    glEnable(GL_TEXTURE_RECTANGLE_NV);
    glColor3f(1, 1, 1);
    glDisable(GL_DEPTH_TEST);

    // STEP 1: Grab the screen and put it into a texture

    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_uiScreenTex);
    glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 0, 0, ScreenWidth, ScreenHeight, 0);

    // STEP 2: Set up an orthogonal projection

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, 0.1, 100);

    // STEP 3: Render the current scene to a new, lower-res texture, darkening non-bright areas of the scene
    // by multiplying it with itself a few times.

    glViewport(0, 0, ScreenWidth / 2, ScreenHeight / 2);

    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_uiScreenTex);

    glBlendFunc(GL_DST_COLOR, GL_ZERO);

    glDisable(GL_BLEND);

    glBegin(GL_QUADS);
    DrawQuad(ScreenWidth, ScreenHeight);
    glEnd();

    glEnable(GL_BLEND);

    glBegin(GL_QUADS);
    for (int i = 0; i < (int)glow_darken_steps->value; i++)
        DrawQuad(ScreenWidth, ScreenHeight);
    glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_uiGlowTex);
    glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 0, 0, ScreenWidth / 2, ScreenHeight / 2, 0);

    // STEP 4: Blur the now darkened scene in the horizontal direction.

    float blurAlpha = 1 / (glow_blur_steps->value * 2 + 1);

    glColor4f(1, 1, 1, blurAlpha);

    glBlendFunc(GL_SRC_ALPHA, GL_ZERO);

    glBegin(GL_QUADS);
    DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBegin(GL_QUADS);
    for (int i = 1; i <= (int)glow_blur_steps->value; i++) {
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, -i, 0);
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, i, 0);
    }
    glEnd();

    glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 0, 0, ScreenWidth / 2, ScreenHeight / 2, 0);

    // STEP 5: Blur the horizontally blurred image in the vertical direction.

    glBlendFunc(GL_SRC_ALPHA, GL_ZERO);

    glBegin(GL_QUADS);
    DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBegin(GL_QUADS);
    for (int i = 1; i <= (int)glow_blur_steps->value; i++) {
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, 0, -i);
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2, 0, i);
    }
    glEnd();

    glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 0, 0, ScreenWidth / 2, ScreenHeight / 2, 0);

    // STEP 6: Combine the blur with the original image.

    glViewport(0, 0, ScreenWidth, ScreenHeight);

    glDisable(GL_BLEND);

    glBegin(GL_QUADS);
    DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    glEnd();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glBegin(GL_QUADS);
    for (int i = 1; i < (int)glow_strength->value; i++) {
        DrawQuad(ScreenWidth / 2, ScreenHeight / 2);
    }
    glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_uiScreenTex);

    glBegin(GL_QUADS);
    DrawQuad(ScreenWidth, ScreenHeight);
    glEnd();

    // STEP 7: Restore the original projection and modelview matrices and disable rectangular textures.

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glDisable(GL_TEXTURE_RECTANGLE_NV);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}