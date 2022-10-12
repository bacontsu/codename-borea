#include "hud.h"
#include "cl_util.h"
#include "PlatformHeaders.h"
#include <stdio.h>
#include <stdlib.h>
#include <gl\gl.h>
#include "blur.h"

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif

#define MAX_MOTIONBLUR_FRAME 10

extern float m_iBlurActive;

CBlurTexture::CBlurTexture() {};

void CBlurTexture::Init(int width, int height)
{
    // create a load of blank pixels to create textures with

    unsigned char* pBlankTex = new unsigned char[width * height * 3];

    memset(pBlankTex, 0, width * height * 3);

    // Create the SCREEN-HOLDING TEXTURE
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, width, height, 0, GL_RGBA8, GL_UNSIGNED_BYTE, 0);

    // free the memory
    delete[] pBlankTex;
}

void CBlurTexture::BindTexture(int width, int height)
{
    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);
    glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 0, 0, ScreenWidth, ScreenHeight, 0);
}

void CBlurTexture::Draw(int width, int height)
{

    // enable some OpenGL stuff

    glEnable(GL_TEXTURE_RECTANGLE_NV);
    glColor3f(1, 1, 1);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, 0.1, 100);
    glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);
    glColor4f(r, g, b, alpha);

    if (gEngfuncs.pfnGetCvarFloat("r_blur"))
    {
        glBegin(GL_QUADS);
        DrawQuad(width, height, of);
        glEnd();
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glDisable(GL_TEXTURE_RECTANGLE_NV);
    glEnable(GL_DEPTH_TEST);
}

void CBlurTexture::DrawQuad(int width, int height, int of)
{
    glTexCoord2f(0 - of, 0 - of);
    glVertex3f(0, 1, -1);
    glTexCoord2f(0 - of, height + of);
    glVertex3f(0, 0, -1);
    glTexCoord2f(width + of, height + of);
    glVertex3f(1, 0, -1);
    glTexCoord2f(width + of, 0 - of);
    glVertex3f(1, 1, -1);
}

bool CBlur::AnimateNextFrame(int desiredFrameRate)
{
    static float lastTime = 0.0f;
    float elapsedTime = 0.0;

    // Get current time in seconds  (milliseconds * .001 = seconds)
    float currentTime = GetTickCount() * 0.001f;

    // Get the elapsed time by subtracting the current time from the last time
    elapsedTime = currentTime - lastTime;

    // Check if the time since we last checked is over (1 second / framesPerSecond)
    if (elapsedTime > (1.0f / desiredFrameRate))
    {
        // Reset the last time
        lastTime = currentTime;

        // Return TRUE, to animate the next frame of animation
        return true;
    }

    // We don't animate right now.
    return false;
}

CBlur gBlur;


void CBlur::InitScreen()
{
    blur_pos = 1;
    for (int i = 0; i < MAX_MOTIONBLUR_FRAME; i++)
    {
        m_pTextures[i].Init(ScreenWidth, ScreenHeight);
    }
}

void CBlur::VidInit()
{
    // clear frame buffers datas
    for (int i = 0; i < MAX_MOTIONBLUR_FRAME; i++)
    {
        memset(&gBlur.m_pTextures[i], 0, sizeof(gBlur.m_pTextures[i]));
    }
    m_flNextFrameUpdate = 0;
}

void CBlur::DrawBlur()
{

        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        glEnable(GL_BLEND);

        for (int i = 0; i < MAX_MOTIONBLUR_FRAME; i++)
        {
            m_pTextures[i].r = 1;//was 0.3
            m_pTextures[i].g = 1;//was 0.3
            m_pTextures[i].b = 1;//was 0.3
            m_pTextures[i].Draw(ScreenWidth, ScreenHeight);
            m_pTextures[i].of = 0;
            m_pTextures[i].alpha = 0.9;
        }

        if (m_flNextFrameUpdate < gHUD.m_flTime)
        {
            m_pTextures[m_iFrameCounter].BindTexture(ScreenWidth, ScreenHeight);
            m_iFrameCounter++;
            if (m_iFrameCounter >= 10) m_iFrameCounter = 0;

            m_flNextFrameUpdate = gHUD.m_flTime + (gHUD.m_flTimeDelta);
        }

        // HACK HACK HACK - level change things, timer broken n stuff 
        if (m_flNextFrameUpdate > gHUD.m_flTime + gHUD.m_flTimeDelta) m_flNextFrameUpdate = 0;

        //gEngfuncs.Con_Printf("frame: %f %f\n", gHUD.m_flTime, m_flNextFrameUpdate);
        glDisable(GL_BLEND);

}
