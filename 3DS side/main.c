/*
 * Tor3DS Interactive Bridge Client
 * Copyright (C) 2026 DisLoPik
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- Screen Size Constants ---
#define TOP_WIDTH      400
#define TOP_HEIGHT     240
#define BOTTOM_WIDTH   320
#define BOTTOM_HEIGHT  240

// --- Global UI State ---
char pc_ip[64] = "192.168.1.100"; // Generic fallback IP
char current_url[256] = "duckduckgo.com";
int scroll_y = 0; 
int is_loading = 0;
char status_message[128] = "Ready to browse.";

// --- Text Buffers ---
C2D_TextBuf dynamic_text_buf;

// --- Link Map Data ---
#define MAX_LINKS 200
typedef struct { int x, y, w, h; char url[256]; } LinkMap;
LinkMap page_links[MAX_LINKS];
int link_count = 0;

// --- Texture Memory ---
C3D_Tex page_tex;
Tex3DS_SubTexture page_subtex;
C2D_Image page_img;

// ==========================================
// CONFIGURATION LOADING
// ==========================================
void load_config(void) {
    // Attempt to open the config file on the SD card
    FILE* file = fopen("sdmc:/3ds/Tor3DS/config.txt", "r");
    if (file) {
        // Read up to 63 characters into the pc_ip variable
        if (fscanf(file, "%63s", pc_ip) != 1) { 
            // If the file is empty or unreadable, revert to the fallback
            strcpy(pc_ip, "192.168.1.100"); 
        }
        fclose(file);
    } else {
        // If the file doesn't exist, we notify the user via the status screen
        snprintf(status_message, sizeof(status_message), "Warning: No config.txt found on SD card.");
    }
}

// ==========================================
// UTILITY: TEXT RENDERING
// ==========================================
void draw_string(C2D_TextBuf buf, float x, float y, float scale, u32 color, const char* fmt, ...) {
    char text_space[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text_space, sizeof(text_space), fmt, args);
    va_end(args);

    C2D_Text text_obj;
    C2D_TextParse(&text_obj, buf, text_space);
    C2D_TextOptimize(&text_obj);
    C2D_DrawText(&text_obj, C2D_WithColor, x, y, 0.5f, scale, scale, color);
}

// ==========================================
// THE MORTON SWIZZLER 
// ==========================================
u32 morton_interleave(u32 x, u32 y) {
    u32 i = (x & 7) | ((y & 7) << 8);
    i = (i ^ (i << 2)) & 0x1313;
    i = (i ^ (i << 1)) & 0x1515;
    i = (i | (i >> 7)) & 0x3F;
    return i;
}

void load_image_to_texture(u8* linear_data, int img_w, int img_h) {
    u32* dest = (u32*)page_tex.data;
    for (int y = 0; y < img_h; y++) {
        for (int x = 0; x < img_w; x++) {
            u32 block_idx = (x / 8) + (y / 8) * (512 / 8);
            u32 offset = block_idx * 64 + morton_interleave(x, y);
            
            u8 r = linear_data[(y * img_w + x) * 4 + 0];
            u8 g = linear_data[(y * img_w + x) * 4 + 1];
            u8 b = linear_data[(y * img_w + x) * 4 + 2];
            u8 a = linear_data[(y * img_w + x) * 4 + 3];
            
            dest[offset] = (r << 24) | (g << 16) | (b << 8) | a; // RGBA8 format for GPU
        }
    }
}

// ==========================================
// NETWORK FETCHERS
// ==========================================
Result fetch_file(const char* endpoint, char* buffer, u32 buffer_size, u32* bytes_read) {
    httpcContext context;
    char request_url[512];
    snprintf(request_url, sizeof(request_url), "http://%s:5000/%s?url=%s", pc_ip, endpoint, current_url);

    Result ret = httpcOpenContext(&context, HTTPC_METHOD_GET, request_url, 0);
    if (R_SUCCEEDED(ret) && R_SUCCEEDED(httpcBeginRequest(&context))) {
        u32 statuscode = 0;
        httpcGetResponseStatusCode(&context, &statuscode);
        if (statuscode == 200) {
            httpcDownloadData(&context, (u8*)buffer, buffer_size - 1, bytes_read);
            buffer[*bytes_read] = '\0';
        } else {
            ret = -1; // Force a fail state if not 200 OK
        }
    }
    httpcCloseContext(&context);
    return ret;
}

void load_page() {
    is_loading = 1;
    scroll_y = 0;
    link_count = 0;
    snprintf(status_message, sizeof(status_message), "Routing through Tor...");

    u32 buffer_size = 500000; 
    char* net_buffer = (char*)malloc(buffer_size);
    u32 bytes_read = 0;

    // 1. Fetch Links
    if (R_SUCCEEDED(fetch_file("fetch_links", net_buffer, buffer_size, &bytes_read))) {
        char* line = strtok(net_buffer, "\n");
        while (line != NULL && link_count < MAX_LINKS) {
            sscanf(line, "%d|%d|%d|%d|%255s", 
                &page_links[link_count].x, &page_links[link_count].y, 
                &page_links[link_count].w, &page_links[link_count].h, 
                page_links[link_count].url);
            link_count++;
            line = strtok(NULL, "\n");
        }
    }

    snprintf(status_message, sizeof(status_message), "Rendering graphics...");

    // 2. Fetch Image
    if (R_SUCCEEDED(fetch_file("fetch_image", net_buffer, buffer_size, &bytes_read))) {
        int w, h, channels;
        u8* pixels = stbi_load_from_memory((u8*)net_buffer, bytes_read, &w, &h, &channels, 4);
        if (pixels) {
            load_image_to_texture(pixels, w, h);
            stbi_image_free(pixels);
            snprintf(status_message, sizeof(status_message), "Connection to Tor Network is secure.");
        } else {
            snprintf(status_message, sizeof(status_message), "Error decoding image data.");
        }
    } else {
        snprintf(status_message, sizeof(status_message), "Failed to reach PC server at %s.", pc_ip);
    }
    
    free(net_buffer);
    is_loading = 0;
}

// ==========================================
// MAIN LOOP
// ==========================================
int main(int argc, char **argv) {
    gfxInitDefault();
    httpcInit(0);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    dynamic_text_buf = C2D_TextBufNew(4096);

    // Read the IP address from the SD card immediately after booting
    load_config();

    // Initialize the GPU Texture
    C3D_TexInit(&page_tex, 512, 1024, GPU_RGBA8);
    C3D_TexSetFilter(&page_tex, GPU_LINEAR, GPU_NEAREST);
    page_subtex.width = 512; page_subtex.height = 1024;
    page_subtex.left = 0.0f; page_subtex.top = 1.0f; page_subtex.right = 1.0f; page_subtex.bottom = 0.0f;
    page_img.tex = &page_tex;
    page_img.subtex = &page_subtex;

    u32 clr_bg       = C2D_Color32(0xF5, 0xF5, 0xF7, 0xFF);
    u32 clr_dark_bar = C2D_Color32(0x3A, 0x1F, 0x5D, 0xFF);
    u32 clr_text_w   = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 clr_text_d   = C2D_Color32(0x22, 0x22, 0x22, 0xFF);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        touchPosition touch;
        hidTouchRead(&touch);

        if (kDown & KEY_START) break;

        C2D_TextBufClear(dynamic_text_buf);

        // --- Hardware Scrolling ---
        if (kHeld & KEY_DDOWN && scroll_y < (1024 - 145)) scroll_y += 5;
        if (kHeld & KEY_DUP && scroll_y > 0) scroll_y -= 5;

        // --- Touch Inputs ---
        if (kDown & KEY_TOUCH && !is_loading) {
            int tx = touch.px;
            int ty = touch.py;

            // Address Bar Trigger
            if (tx >= 10 && tx <= 310 && ty >= 10 && ty <= 40) {
                SwkbdState swkbd;
                swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
                swkbdSetHintText(&swkbd, "Enter target URL...");
                if (swkbdInputText(&swkbd, current_url, sizeof(current_url)) != SWKBD_BUTTON_NONE) {
                    load_page();
                }
            }
            
            // Image Link Clicking Logic
            if (ty >= 50 && ty <= 195) {
                int real_y = (ty - 50) + scroll_y; 
                int real_x = tx - 10;
                
                for (int i = 0; i < link_count; i++) {
                    if (real_x >= page_links[i].x && real_x <= page_links[i].x + page_links[i].w &&
                        real_y >= page_links[i].y && real_y <= page_links[i].y + page_links[i].h) {
                        
                        strcpy(current_url, page_links[i].url);
                        load_page();
                        break;
                    }
                }
            }
        }

        // --- RENDER PIPELINE ---
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        // --- TOP SCREEN ---
        C2D_TargetClear(top_target, clr_bg);
        C2D_SceneBegin(top_target);
        C2D_DrawRectSolid(0, 0, 0.1f, TOP_WIDTH, TOP_HEIGHT, clr_dark_bar);
        draw_string(dynamic_text_buf, 20, 200, 0.60f, clr_text_w, "Tor3DS Interactive Bridge");
        draw_string(dynamic_text_buf, 20, 220, 0.50f, clr_text_w, "%s", status_message);

        // --- BOTTOM SCREEN ---
        C2D_TargetClear(bottom_target, clr_bg);
        C2D_SceneBegin(bottom_target);

        // 1. Draw the webpage image (Background depth: 0.5f)
        C2D_DrawImageAt(page_img, 10, 50 - scroll_y, 0.5f, NULL, 1.0f, 1.0f);

        // 2. Draw UI blocks OVER the image to crop it (Foreground depth: 0.1f)
        C2D_DrawRectSolid(0, 0, 0.1f, BOTTOM_WIDTH, 50, clr_dark_bar);
        C2D_DrawRectSolid(10, 10, 0.05f, 300, 30, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));
        draw_string(dynamic_text_buf, 15, 18, 0.45f, clr_text_d, "%s", current_url);

        C2D_DrawRectSolid(0, 200, 0.1f, BOTTOM_WIDTH, 40, clr_dark_bar);
        
        C3D_FrameEnd(0);
    }

    C3D_TexDelete(&page_tex);
    C2D_TextBufDelete(dynamic_text_buf);
    C2D_Fini();
    C3D_Fini();
    httpcExit();
    gfxExit();
    return 0;
}
