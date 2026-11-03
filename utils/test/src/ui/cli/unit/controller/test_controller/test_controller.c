// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_controller.c: the controller's menus as it publishes them: Reduce Motion shown on while the saved setting or the
// system asks for less motion, its command setting it on or off as its word says and else turning it from what shows
#include "ui/cli/src/controller/controller_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the state Reduce Motion shows in the View menu opened on the bar, the menus closed again after
static int reduce_motion_checked(void)
{
    CliMenus *const bar = (CliMenus *)EMBED_CALL(cli_model_commands.menus_alloc, CliModelCtx, .at = 0u);
    TEST_ASSERT_NOT_NULL(bar);
    unsigned int title = 0u;
    while ((title < bar->titles) && (strcmp(bar->title[title], "View") != 0))
    {
        title += 1u;
    }
    TEST_ASSERT_LESS_THAN_UINT(bar->titles, title);
    g_cli_cells->persistent_buf_release(bar, &g_cli_controller_cells);
    const CliInput pressed = {.kind = CLI_INPUT_POINTER_DOWN, .hit = CLI_HIT_MENU_TITLE, .index = title};
    EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = &pressed);
    CliMenus *const menus = (CliMenus *)EMBED_CALL(cli_model_commands.menus_alloc, CliModelCtx, .at = 0u);
    TEST_ASSERT_NOT_NULL(menus);
    int checked = -2;
    for (unsigned int at = 0u; (menus->levels != 0u) && (at < menus->level[0].rows); at += 1u)
    {
        checked = (strcmp(menus->level[0].row[at].label, "Reduce Motion") == 0) ? menus->level[0].row[at].checked
                                                                                 : checked;
    }
    g_cli_cells->persistent_buf_release(menus, &g_cli_controller_cells);
    const CliInput away = {.kind = CLI_INPUT_POINTER_DOWN, .hit = CLI_HIT_NONE};
    EMBED_CALL(cli_model_commands.menu_input, CliModelCtx, .input = &away);
    return checked;
}

// the system's wish for less motion told the controller, as the window tells it
static void system_wish(unsigned int reduced)
{
    const CliInput told = {.kind = CLI_INPUT_MOTION_SYSTEM, .index = reduced};
    cli_controller_model_input(&told, NULL);
}

// the reduce-motion command run with its word, NULL for none
static void reduce_motion_run(const char *word)
{
    const char *const words[1] = {word};
    EMBED_CALL(cli_model_commands.command_run, CliModelCtx, .path = "reduce-motion", .words = words,
               .at = (word != NULL) ? 1u : 0u);
}

void test_reduce_motion_shows_on_while_the_saved_setting_or_the_system_asks_for_less_motion(void)
{
#if defined(_WIN32)
    TEST_ASSERT_EQUAL_INT(0, _putenv_s("ORIOR_HOME", "build/harness/controller_home"));
#else
    TEST_ASSERT_EQUAL_INT(0, setenv("ORIOR_HOME", "build/harness/controller_home", 1));
#endif
    TEST_ASSERT_TRUE(EMBED_CALL(cli_model_commands.catalog_read, CliModelCtx, .at = 0u));
    reduce_motion_run("off");
    system_wish(0u);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, reduce_motion_checked(), "off while neither asks for less motion");
    system_wish(1u);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "on while the system asks for less motion");
    reduce_motion_run(NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(),
                                  "turned from on, the saved setting is off and the system's wish still shows");
    system_wish(0u);
    TEST_ASSERT_EQUAL_INT(0, reduce_motion_checked());
    reduce_motion_run("on");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "its word on sets it on");
    reduce_motion_run("on");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "on again stays on");
    reduce_motion_run("off");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, reduce_motion_checked(), "its word off sets it off");
    reduce_motion_run(NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, reduce_motion_checked(), "with no word it turns from what shows");
    reduce_motion_run("off");
}
