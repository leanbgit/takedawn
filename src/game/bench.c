/*
 * bench.c - BO3_BENCH: a fixed race played by itself, for measuring the
 * runtime (the toolkit roadmap's step 2; docs/split4.md, Performance).
 *
 *   BO3_BENCH=4p   Multiplayer > Race, four players (BO3_SPLIT4 must be on)
 *   BO3_BENCH=2p   Multiplayer > Race, two players
 *   BO3_BENCH=1p   Single Event > Race
 *
 * Always USA's first track, four rivals' worth of cars. The title is played
 * from its start on a new profile that is never saved (the shader
 * pre-build's way to the main menu, shaderbuild.c), driven by the toolkit's
 * in-process scene driver (xbox_autoplay.h): every step waits on the title
 * (a file it opens, a number of its flips), never on the clock. Unlike a
 * pre-build scene it is a normal run: a window, the player's settings, real
 * time. In the race every human holds the accelerator; the AI drives what
 * BO3_SPLIT4_AI gives it.
 *
 * The measured part is the flips between the marks "bench start" (warm-up
 * after GO) and "bench end" (BO3_BENCH_FLIPS later, default 2400). The
 * marks go to BO3_BENCH_OUT/marks.txt with the executor's flip number
 * (the traces' flip column) and the time;
 * the traces the run was started with (RECOMP_TRACE=frametime,vk_frame,
 * pb_split, by the launcher, run-bench.cmd) are cut to that window by
 * tools/perf/bench_compare.py. At "bench end" the process exits.
 *
 * Needs RECOMP_PAD_LIVE (the file the presses go to), an empty
 * RECOMP_SAVE_DIR and BO3_UNLOCK_ALL=1: the launcher sets them.
 *
 * The frame check (roadmap step 3, tools/perf/frame_check.sh,
 * frame_compare.py): BO3_BENCH_SHOTS=1 dumps the frame three flips after
 * every mark (RECOMP_FB_DUMP, RECOMP_FB_DUMP_AT=@<file>) and lists them in
 * shots.txt; with BO3_LOCKSTEP=1 (hirefresh.c: one simulation step an
 * update, the game's random generator held until the race) two runs show
 * the same race, so two builds' frames can be compared.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#if defined(_WIN32)
#include <process.h>                    /* _exit */
#else
#include <unistd.h>                     /* _exit */
#endif

#include "xbox_autoplay.h"
#include "xbox_crash.h"                 /* g_xbox_flip_count */
#include "usb/usb_gamepad.h"            /* usb_gamepad_live_taken */
#include "input/input_backend.h"        /* xbox_InputSetBlocked */
#include "nv2a_pb_state.h"                /* nv2a_pb_gpu_state: the executor's flip, the traces' */

#define BENCH_MAX 96

static xap_step s_st[BENCH_MAX];
static char s_txt[BENCH_MAX][40];
static int s_n;
static char s_out[1024];
static char s_marks[1100];
static int s_on = -1;
static int s_shots;                     /* BO3_BENCH_SHOTS: a frame at every mark */
static char s_dumpat[1100], s_shotlist[1100];

static void add(xap_op op, const char *s, const char *s2, int n)
{
    if (s_n >= BENCH_MAX - 1)
        return;
    s_st[s_n].op = op;
    s_st[s_n].s = s;
    s_st[s_n].s2 = s2;
    s_st[s_n].n = n;
    s_n++;
}

static void press(const char *what, int flips)
{
    if (s_n >= BENCH_MAX - 1)
        return;
    snprintf(s_txt[s_n], sizeof s_txt[0], "%s", what);
    add(XAP_PRESS, s_txt[s_n], NULL, flips);
}

static double now_ms(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void note(const char *what)
{
    FILE *f = fopen(s_marks, "a");
    if (f) {
        /* the executor's flip number: the one the traces' flip columns
         * count (g_xbox_flip_count also moves while the title is paused) */
        fprintf(f, "%u %.1f %s\n", (unsigned)nv2a_pb_gpu_state()->flips, now_ms(), what);
        fclose(f);
    }
    fprintf(stderr, "[BENCH] %s (flip %u)\n", what, (unsigned)g_xbox_flip_count);
}

/* BO3_BENCH_SHOTS=1: the frame three flips after a mark is dumped (the
 * toolkit's fb_dump_at reads the flips from s_dumpat as it grows; the
 * executor's flip, as the marks), and shots.txt says which mark it is. */
static void shot(const char *what)
{
    unsigned flip = (unsigned)nv2a_pb_gpu_state()->flips + 3u;
    FILE *f = fopen(s_dumpat, "a");
    if (f) {
        fprintf(f, "%u\n", flip);
        fclose(f);
    }
    f = fopen(s_shotlist, "a");
    if (f) {
        fprintf(f, "%u %s\n", flip, what);
        fclose(f);
    }
}

static void gpu_note(void);

static void bench_mark(void *ctx, const char *text, int thousandths)
{
    (void)ctx;
    (void)thousandths;
    note(text ? text : "");
    if (text && !strcmp(text, "bench 3/4"))
        gpu_note();
    if (s_shots && text && strcmp(text, "bench end"))
        shot(text);
}

/* The GPU's software methods and progress (roadmap step 4), for the log:
 * what arrived and what was done about it. Said at "bench 3/4": the last
 * lines before the exit do not always reach the log. */
static void gpu_note(void)
{
    uint32_t seen[32], handled[32];
    int i;
    nv2a_pb_swmethod_counts(seen, handled);
    fprintf(stderr, "[BENCH] DMA_GET moved at sync points %u times;"
            " software methods (case: seen/handled):",
            nv2a_pb_progress_gets());
    for (i = 0; i < 32; i++)
        if (seen[i])
            fprintf(stderr, " %d: %u/%u", i, seen[i], handled[i]);
    fprintf(stderr, "\n");
    fflush(stderr);
}

static void bench_done(void *ctx, int ok, const char *why)
{
    char line[200];
    (void)ctx;
    snprintf(line, sizeof line, "done %s%s%s", ok == 1 ? "ok" : ok < 0 ? "cancelled" : "fail",
             why ? " " : "", why ? why : "");
    note(line);
    fflush(NULL);
    _exit(ok == 1 ? 0 : 3);
}

int bo3_bench_on(void)
{
    if (s_on < 0) {
        const char *b = getenv("BO3_BENCH");
        s_on = b && (!strcmp(b, "1p") || !strcmp(b, "2p") || !strcmp(b, "4p"));
    }
    return s_on;
}

/* The steps for BO3_BENCH; 0 when it names nothing or the launcher's
 * variables are missing. */
static int build(void)
{
    const char *mode = getenv("BO3_BENCH"), *out = getenv("BO3_BENCH_OUT");
    const char *fl = getenv("BO3_BENCH_FLIPS");
    int flips = fl && atoi(fl) > 0 ? atoi(fl) : 2400;
    int players = mode[0] - '0';

    if (!getenv("RECOMP_PAD_LIVE") || !out || !*out)
        return 0;
    snprintf(s_out, sizeof s_out, "%s", out);
    snprintf(s_marks, sizeof s_marks, "%s/marks.txt", s_out);

    /* To the main menu, on empty saves (shaderbuild.c's BOOT_TO_MENU, up to
     * the main menu): the logos, the intro film (START skips it), Press
     * START, New Profile, the name as offered, "save your profile?" No,
     * "Autosave will be disabled" Yes. The menus take no input while they
     * slide in and open no file to wait on: 150 flips a press. */
    add(XAP_WAIT_FILE, "cri_rw30.xmv", NULL, 0);
    add(XAP_WAIT_FILE, "titles30.xmv", NULL, 0);
    add(XAP_PRESS_UNTIL, "start:300", "_femain.xmv", 60);
    add(XAP_WAIT_ANCHOR, NULL, NULL, 300);
    press("start:600", 150);                    /* Select Option: New Profile */
    press("a:400", 150);                        /* the keyboard, cursor on Done */
    press("a:400", 150);                        /* save your profile? No */
    press("a:400", 150);                        /* Autosave will be disabled: No */
    press("up:200", 30);                        /* Yes */
    press("a:400", 240);                        /* the main menu, on World Tour */
    add(XAP_MARK, "main menu", NULL, 200);

    if (players == 1) {
        press("down:200", 30);                  /* Single Event */
        press("a:400", 120);                    /* Select Mode */
    } else {
        press("down:200", 30);
        press("down:200", 30);                  /* Multiplayer */
        press("a:400", 120);                    /* Select Mode */
    }
    press("a:400", 120);                        /* Race: Select Options */
    press("right:200", 30);                     /* Region: Random > USA */
    press("down:200", 30);
    press("right:200", 30);                     /* Track: Random > the first */
    add(XAP_PRESS_UNTIL, "a:400", "car1.bgv", 150);
    add(XAP_MARK, "garage", NULL, 300);
    add(XAP_WAIT_ANCHOR, NULL, NULL, 240);
    add(XAP_MARK, "car select", NULL, 320);
    if (players == 1) {
        /* the car: again until the track is opened (the training film) */
        add(XAP_PRESS_UNTIL, "a:400", "streamed.dat", 150);
    } else {
        int p;
        press("a:400", 180);                    /* player 1's car */
        for (p = 2; p <= players; p++) {
            char b[16];
            snprintf(b, sizeof b, "p%d-a:400", p);
            press(b, p < players ? 180 : 0);    /* player p's turn */
        }
        add(XAP_WAIT_FILE, "streamed.dat", NULL, 0);
    }
    add(XAP_MARK, "track loaded", NULL, 500);
    /* GO: the race opens sound\ident0.awd then (about 700 flips after the
     * track, in a four-player race on the user's PC); a one-player race's
     * crash sound banks (shaderbuild.c waits on those) are not opened in a
     * Multiplayer race. At most 1500 flips: then the run goes on anyway. */
    add(XAP_SYNC, NULL, NULL, 0);
    add(XAP_WAIT_FILE, "sound\\ident", NULL, 1500);
    add(XAP_MARK, "race", NULL, 600);
    {
        /* Every human on the accelerator for the rest of the run (the
         * hold outlasts it); player 1 on pad 1. */
        int p;
        press("rt:600000", 0);
        for (p = 2; p <= players; p++) {
            char b[24];
            snprintf(b, sizeof b, "p%d-rt:600000", p);
            press(b, 0);
        }
    }
    add(XAP_WAIT_FLIPS, NULL, NULL, 150);
    add(XAP_MARK, "race +150", NULL, 650);
    add(XAP_WAIT_FLIPS, NULL, NULL, 150);       /* up to speed */
    add(XAP_MARK, "bench start", NULL, 700);
    /* the measured flips in quarters, a mark at each (a frame for the
     * frame check; nothing else for the measurement) */
    add(XAP_WAIT_FLIPS, NULL, NULL, flips / 4);
    add(XAP_MARK, "bench 1/4", NULL, 775);
    add(XAP_WAIT_FLIPS, NULL, NULL, flips / 4);
    add(XAP_MARK, "bench 2/4", NULL, 850);
    add(XAP_WAIT_FLIPS, NULL, NULL, flips / 4);
    add(XAP_MARK, "bench 3/4", NULL, 925);
    add(XAP_WAIT_FLIPS, NULL, NULL, flips - 3 * (flips / 4));
    add(XAP_MARK, "bench end", NULL, 1000);
    add(XAP_END, NULL, NULL, 0);
    return 1;
}

void bo3_bench_start(void)
{
    static xap_run run;
    FILE *f;

    if (!bo3_bench_on())
        return;
    if (!build()) {
        fprintf(stderr, "[BENCH] BO3_BENCH needs RECOMP_PAD_LIVE and BO3_BENCH_OUT"
                " (start it with run-bench.cmd)\n");
        return;
    }
    f = fopen(s_marks, "w");
    if (f) {
        fprintf(f, "# flip ms what (BO3_BENCH=%s)\n", getenv("BO3_BENCH"));
        fclose(f);
    }
    {
        /* BO3_BENCH_SHOTS=1 with RECOMP_FB_DUMP=<prefix> and
         * RECOMP_FB_DUMP_AT=@<file> (run-bench.cmd shots, frame_check.sh) */
        const char *sh = getenv("BO3_BENCH_SHOTS"), *at = getenv("RECOMP_FB_DUMP_AT");
        if (sh && *sh == '1' && getenv("RECOMP_FB_DUMP") && at && at[0] == '@') {
            s_shots = 1;
            snprintf(s_dumpat, sizeof s_dumpat, "%s", at + 1);
            snprintf(s_shotlist, sizeof s_shotlist, "%s/shots.txt", s_out);
            f = fopen(s_shotlist, "w");
            if (f)
                fclose(f);
        }
    }
    xbox_InputSetBlocked(1);            /* a touch of the keyboard is not the run's */
    run.steps = s_st;
    run.pad_file = getenv("RECOMP_PAD_LIVE");
    run.pad_taken = usb_gamepad_live_taken;
    run.wait_timeout_s = 300;
    run.mark = bench_mark;
    run.done = bench_done;
    if (!xbox_AutoplayStart(&run)) {
        fprintf(stderr, "[BENCH] the scene driver did not start\n");
        return;
    }
    note("bench begins");
}
