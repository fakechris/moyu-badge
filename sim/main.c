// sim/main.c —— PC runner: SDL window 240x320 + keyboard-as-buttons.
// Keys: Up/Down arrows = UP/DOWN, Enter/Space = OK (hold >0.9s = LONG), Esc = quit.
// Headless self-QA: --script "down ok wait:6000" --shot out.rgb565
//   tokens: up down ok okl wait:MS shot | snapshot = active screen RGB565 240x320.
#include "lvgl.h"
#include "agent_status.h"
#include "app_shell.h"
#include "bsp_button.h"
#include "demo_deskpet.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_main(void);

static uint32_t s_down_ms[3];
static bool s_down[3];

static int key_to_btn(SDL_Keycode k)
{
    if (k == SDLK_UP) return BSP_BTN_UP;
    if (k == SDLK_DOWN) return BSP_BTN_DOWN;
    if (k == SDLK_RETURN || k == SDLK_SPACE) return BSP_BTN_OK;
    return -1;
}

static void pump(uint32_t ms)
{
    uint32_t end = SDL_GetTicks() + ms;
    while ((int32_t)(end - SDL_GetTicks()) > 0) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) exit(0);
        }
        uint32_t now = SDL_GetTicks();
        static uint32_t last;
        if (!last) last = now;
        lv_tick_inc(now - last);
        last = now;
        lv_timer_handler();
        SDL_Delay(5);
    }
}

static int save_shot(const char *path)
{
    static uint8_t buf[240u * 320u * 2u];
    lv_draw_buf_t db;
    lv_draw_buf_init(&db, 240, 320, LV_COLOR_FORMAT_RGB565, 240 * 2, buf, sizeof(buf));
    if (lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &db) != LV_RESULT_OK) {
        printf("snapshot failed\n");
        return 1;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("cannot write %s\n", path);
        return 1;
    }
    fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    printf("shot saved %s (%zu bytes)\n", path, sizeof(buf));
    return 0;
}

static void tap(bsp_btn_t b, bsp_btn_ev_t ev)
{
    sim_inject_key(b, BSP_BTN_PRESS);
    pump(60);
    sim_inject_key(b, ev);
    pump(300);
}

static void ensure_game_mode(void)
{
    if (app_shell_mode() != APP_MODE_GAME) app_shell_enter(APP_MODE_GAME);
}

static int run_script(const char *script, const char *shot)
{
    char *s = strdup(script);
    char *tok = strtok(s, " ");
    while (tok) {
        if (!strcmp(tok, "up")) tap(BSP_BTN_UP, BSP_BTN_CLICK);
        else if (!strcmp(tok, "down")) tap(BSP_BTN_DOWN, BSP_BTN_CLICK);
        else if (!strcmp(tok, "ok")) tap(BSP_BTN_OK, BSP_BTN_CLICK);
        else if (!strcmp(tok, "okl")) tap(BSP_BTN_OK, BSP_BTN_LONG);
        else if (!strcmp(tok, "okd")) tap(BSP_BTN_OK, BSP_BTN_DOUBLE);
        else if (!strcmp(tok, "upl")) tap(BSP_BTN_UP, BSP_BTN_LONG);
        else if (!strcmp(tok, "dnl")) tap(BSP_BTN_DOWN, BSP_BTN_LONG);
        else if (!strncmp(tok, "wait:", 5)) pump((uint32_t)atoi(tok + 5));
        else if (!strncmp(tok, "floor:", 6)) {
            ensure_game_mode();
            demo_deskpet_debug_floor((unsigned)atoi(tok + 6));
        } else if (!strncmp(tok, "art:", 4)) {
            ensure_game_mode();
            demo_deskpet_debug_art(tok + 4);
        }
        else if (!strcmp(tok, "shot")) {
            if (save_shot(shot ? shot : "shot.rgb565")) return 1;
        } else if (!strcmp(tok, "agent")) {
            // agent <slot> <src> <state> <progress|-> <name> <text..._...>
            // (text is ONE token; _ renders as space, since script is
            // space-split and strtok(NULL,"") would swallow the script tail)
            char *sl = strtok(NULL, " ");
            char *sr = strtok(NULL, " ");
            char *st = strtok(NULL, " ");
            char *pr = strtok(NULL, " ");
            char *nm = strtok(NULL, " ");
            char *tx = strtok(NULL, " ");
            agent_source_t src = ASRC_GENERIC;
            if (sr && !strcmp(sr, "codex")) src = ASRC_CODEX;
            else if (sr && !strcmp(sr, "dsh")) src = ASRC_DSH;
            else if (sr && !strcmp(sr, "opencode")) src = ASRC_OPENCODE;
            agent_state_t state = AST_IDLE;
            if (st && !strcmp(st, "working")) state = AST_WORKING;
            else if (st && !strcmp(st, "needs")) state = AST_NEEDS_YOU;
            else if (st && !strcmp(st, "review")) state = AST_REVIEW;
            else if (st && !strcmp(st, "failed")) state = AST_FAILED;
            else if (st && !strcmp(st, "done")) state = AST_CELEBRATE;
            uint8_t prog = 255;
            if (pr && strcmp(pr, "-")) prog = (uint8_t)atoi(pr);
            char textbuf[64];
            snprintf(textbuf, sizeof(textbuf), "%s", tx ? tx : "");
            for (char *p = textbuf; *p; p++)
                if (*p == '_') *p = ' ';
            extern int64_t esp_timer_get_time(void);
            agent_status_set(sl ? (uint8_t)atoi(sl) : 0, src, state, prog,
                             nm ? nm : "", textbuf,
                             (uint64_t)(esp_timer_get_time() / 1000));
            printf("[AGENT] slot=%s state=%s\n", sl ? sl : "?", st ? st : "?");
        } else printf("unknown token %s\n", tok);
        tok = strtok(NULL, " ");
    }
    free(s);
    if (shot) return save_shot(shot);
    return 0;
}

int main(int argc, char **argv)
{
    const char *script = NULL, *shot = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
    }
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    lv_init();
    if (!lv_sdl_window_create(240, 320)) {
        printf("SDL window failed\n");
        return 1;
    }

    app_main();
    if (script) {
        pump(800);
        int rc = run_script(script, shot);
        SDL_Quit();
        return rc;
    }
    printf("DeskPet sim running: arrows + Enter/Space, hold = LONG, Esc quits\n");

    uint32_t last = SDL_GetTicks();
    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quit = true;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                int b = key_to_btn(e.key.keysym.sym);
                if (b >= 0) {
                    s_down[b] = true;
                    s_down_ms[b] = SDL_GetTicks();
                    sim_inject_key((bsp_btn_t)b, BSP_BTN_PRESS);
                }
            }
            if (e.type == SDL_KEYUP) {
                int b = key_to_btn(e.key.keysym.sym);
                if (b >= 0 && s_down[b]) {
                    s_down[b] = false;
                    uint32_t held = SDL_GetTicks() - s_down_ms[b];
                    sim_inject_key((bsp_btn_t)b,
                                   held > 900 ? BSP_BTN_LONG : BSP_BTN_CLICK);
                }
            }
        }
        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last);
        last = now;
        lv_timer_handler();
        SDL_Delay(5);
    }
    SDL_Quit();
    return 0;
}
