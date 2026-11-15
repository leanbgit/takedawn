/*
 * settings.c - the four-player split screen from settings.toml.
 *
 * The overlay's settings file (xbox_settings.h) keeps keys it does not know,
 * so a [splitscreen] section in it is read here, after the toolkit's own
 * settings, into the variables the split screen and the runtime read:
 *
 *   [splitscreen]
 *   players = 4            # 2 (the title's own split screen) or 4
 *   ai_players = "none"    # who of players 3 and 4 the game drives:
 *                          #   "none", "3", "4" or "34"
 *   keyboard_player = 1    # the keyboard drives player 1 (default); 3 or 4:
 *                          #   in the race the arrow keys drive that player
 *   cars = "copy"          # players 3's and 4's cars until they pick one:
 *                          #   "copy", "random" or names (docs/split4.md)
 *   run_ahead = true       # the GPU a frame behind the game (60 fps in 4P)
 *
 * players = 4 sets BO3_SPLIT4, four emulated controllers (RECOMP_USB_PADS)
 * and, unless run_ahead = false, RECOMP_RUN_AHEAD. A variable already in the
 * environment wins over the file, as for every other setting (run-4p.cmd
 * and the like keep working). Only the North America disc's program has the
 * four-player code; the others say so and ignore the section.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "region.h"
#include "recomp_cfg.h"
#include "platform/recomp_env.h"

/* An environment variable, unless the user set it already. */
static void s4_setenv_keep(const char *name, const char *value)
{
    const char *cur = getenv(name);
    if (cur && *cur)
        return;
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void split4_settings_load(const char *path)
{
    char err[256];
    recomp_cfg *c;
    long long players;
    const char *s;

    if (!path || !*path)
        return;
    {
        FILE *probe = fopen(path, "rb");
        if (!probe)
            return;
        fclose(probe);
    }
    c = recomp_cfg_load_file_flags(path, RCFG_REOPEN_TABLES, err, sizeof err);
    if (!c)
        return;                         /* the toolkit's loader said why */
    if (recomp_cfg_type_of(c, "splitscreen.players") == RCFG_NONE) {
        recomp_cfg_free(c);
        return;
    }
    players = recomp_cfg_int(c, "splitscreen.players", 2);
#if (defined(BO3_REGION_PAL) && BO3_REGION_PAL) || (defined(BO3_REGION_PAL_ENSV) && BO3_REGION_PAL_ENSV)
    if (players == 4)
        fprintf(stderr, "[SPLIT4] splitscreen.players = 4: only the North America"
                " disc's program has the four-player split screen; this is the %s"
                " one, so two players as shipped\n", BO3_REGION_NAME);
    recomp_cfg_free(c);
    return;
#endif
    if (players != 4) {
        if (players != 2)
            fprintf(stderr, "[SPLIT4] splitscreen.players = %lld: 2 or 4; two players\n",
                    players);
        recomp_cfg_free(c);
        return;
    }
    s4_setenv_keep("BO3_SPLIT4", "4");
    s4_setenv_keep("RECOMP_USB_PADS", "4");
    if (recomp_cfg_bool(c, "splitscreen.run_ahead", 1))
        s4_setenv_keep("RECOMP_RUN_AHEAD", "1");

    s = recomp_cfg_string(c, "splitscreen.ai_players", "none");
    if (!strcmp(s, "3") || !strcmp(s, "4") || !strcmp(s, "34"))
        s4_setenv_keep("BO3_SPLIT4_AI", s);
    else if (strcmp(s, "none"))
        fprintf(stderr, "[SPLIT4] splitscreen.ai_players = \"%s\": \"none\", \"3\","
                " \"4\" or \"34\"; none\n", s);

    /* The keyboard is player 1's, with or beside player 1's controller (four
     * controllers is the usual set-up), unless keyboard_player gives the
     * arrows to player 3 or 4 in the race. BO3_SPLIT4_KEYS unset would give
     * them to player 3 (stage.c), so "0" says player 1 out loud. */
    {
        long long k = 1;
        if (recomp_cfg_type_of(c, "splitscreen.keyboard_player") == RCFG_INT)
            k = recomp_cfg_int(c, "splitscreen.keyboard_player", 1);
        if (k != 1 && k != 3 && k != 4) {
            fprintf(stderr, "[SPLIT4] splitscreen.keyboard_player = %lld: 1, 3 or 4;"
                    " 1\n", k);
            k = 1;
        }
        {
            char v[2] = { (char)(k == 1 ? '0' : '0' + k), 0 };
            s4_setenv_keep("BO3_SPLIT4_KEYS", v);
        }
    }

    s = recomp_cfg_string(c, "splitscreen.cars", "");
    if (*s)
        s4_setenv_keep("BO3_SPLIT4_CARS", s);
    recomp_env_reload();                /* RECOMP_USB_PADS is read from the cache */

    fprintf(stderr, "[SPLIT4] settings: four players (%s), run-ahead %s, AI %s,"
            " arrow keys player %s\n", path,
            getenv("RECOMP_RUN_AHEAD") && *getenv("RECOMP_RUN_AHEAD") != '0' ? "on" : "off",
            getenv("BO3_SPLIT4_AI") ? getenv("BO3_SPLIT4_AI") : "none",
            getenv("BO3_SPLIT4_KEYS") && *getenv("BO3_SPLIT4_KEYS") > '0' ? getenv("BO3_SPLIT4_KEYS") : "1");
    recomp_cfg_free(c);
}
