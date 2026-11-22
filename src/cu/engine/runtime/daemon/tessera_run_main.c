// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_run_main.c: tessera_run's main
#include "tessera_run_internal.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "memoria_operor/memoria_linea.h"

_Alignas(8) static const
    char s_run_usage[] = "  tessera_run: --processors <count> [--name <text>] [--child] -- <command> [arguments]\n"
                         "  tessera_run: --parent <records> [--name <text>] -- <command> [arguments]\n";
_Alignas(8) static const
    char s_run_unsubmitted[] = "  tessera_run: the job could not be submitted (its signum, or the daemon's place)\n";

int program_main(int count, char **arguments)
{
    RunRequest request;
    mmgr_bytes_set(&request, 0, sizeof(request));
    if (!run_arguments(count, arguments, &request))
    {
        {
            const char *const error_text = s_run_usage;
            platform_terminal_write(1, error_text,
                                    (unsigned long long)mmgr_text_length(error_text, PLATFORM_TEXT_TO_TERMINATOR));
        }
        return RUN_FAILED;
    }
    const unsigned long long mask = tessera_self_host_mask();
    // a processor count is at max sixty-four, represented whole
    const unsigned long long given = (unsigned long long)engine_word_population(mask);
    if ((mask == 0ULL) || (request.processors > given))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  tessera_run: requests ");
            mmgr_line_decimal(&engine_note.line, request.processors, 1u);
            mmgr_line_put(&engine_note.line, " processors, and the host gives jobs ");
            mmgr_line_decimal(&engine_note.line, given, 1u);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return RUN_FAILED;
    }
    const char *const label = (request.name != NULL) ? request.name : arguments[request.command];
    if (request.parent != NULL)
    {
        return run_child(&request, arguments, count, mask, label);
    }
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    char daemon[ENGINE_PATH_CAPACITY];
    TesseraJobSubmit submit;
    mmgr_bytes_set(&submit, 0, sizeof(submit));
    if (!run_daemon_path(daemon, sizeof(daemon)) || !run_signum(&request, arguments, count, &submit.signum, &error))
    {
        {
            const char *const error_text = s_run_unsubmitted;
            platform_terminal_write(1, error_text,
                                    (unsigned long long)mmgr_text_length(error_text, PLATFORM_TEXT_TO_TERMINATOR));
        }
        return RUN_FAILED;
    }
    submit.declared = request.processors * TESSERA_HOST_PROCESSOR;
    submit.grace_microseconds = RUN_RUNNING_MICROSECONDS;
    submit.sweep_microseconds = RUN_SWEEP_MICROSECONDS;
    submit.idle_microseconds = RUN_IDLE_MICROSECONDS;
    // processors named past the signum's tracked peak are not reserved for requesting: the job waits only for capacity.
    // It still reserves the tracked peak where that is more than it named
    submit.override_budget = 1u;
    submit.daemon_path = daemon;
    submit.error = &error;
    const unsigned long long submitted_at = run_now();
    TesseraClient *client = NULL;
    TesseraTicket ticket;
    if (tessera_job_submit(&submit, &client, &ticket) != 0L)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  tessera_run: ");
            mmgr_line_put(&engine_note.line, label);
            mmgr_line_put(&engine_note.line, " was not admitted by the host's daemon (");
            mmgr_line_put(&engine_note.line, daemon);
            mmgr_line_put(&engine_note.line, ")");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return RUN_FAILED;
    }
    const unsigned long long waited = run_now() - submitted_at;
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: ");
        mmgr_line_put(&engine_note.line, label);
        mmgr_line_put(&engine_note.line, " admitted after ");
        mmgr_line_decimal(&engine_note.line, waited / RUN_MILLION, 1u);
        mmgr_line_put(&engine_note.line, ".");
        mmgr_line_decimal(&engine_note.line, (waited % RUN_MILLION) / RUN_THOUSAND, 3u);
        mmgr_line_put(&engine_note.line, " s, ");
        mmgr_line_decimal(&engine_note.line, ticket.granted / TESSERA_HOST_PROCESSOR, 1u);
        mmgr_line_put(&engine_note.line, ".");
        mmgr_line_decimal(&engine_note.line, ticket.granted % TESSERA_HOST_PROCESSOR, 3u);
        mmgr_line_put(&engine_note.line, " of ");
        mmgr_line_decimal(&engine_note.line, given, 1u);
        mmgr_line_put(&engine_note.line, " processors reserved, on processors 0x");
        mmgr_line_hex(&engine_note.line, mask, 1u);
        mmgr_line_put(&engine_note.line, " at below normal priority");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    RunChannel channel;
    mmgr_bytes_set(&channel, 0, sizeof(channel));
    int started_count = count - request.command;
    char **const watched = run_channel_open(&channel, &request, arguments + request.command, count - request.command,
                                            &submit.signum, &started_count);
    char *const *const words = (watched != NULL) ? watched : (arguments + request.command);
    RunChild child;
    mmgr_bytes_set(&child, 0, sizeof(child));
    int code = RUN_NOT_STARTED;
    // the whole processors of the reservation, and never none: a job reserves its tracked peak where that is more than
    // it named, and the count of whole ones in it is what the command may run on
    const unsigned long long whole = ticket.granted / TESSERA_HOST_PROCESSOR;
    if (!run_processors_named((whole != 0ULL) ? whole : 1ULL))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: the processors granted were not passed on to ");
        mmgr_line_put(&engine_note.line, label);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    const unsigned long long started = run_now();
    if (run_start(&child, words, started_count, mask, &channel))
    {
        run_wait(&child, &channel, client, &ticket, &error);
        code = run_finish(&child);
    }
    else
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: ");
        mmgr_line_put(&engine_note.line, arguments[request.command]);
        mmgr_line_put(&engine_note.line, " could not be started");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    free(watched);
    run_channel_close(&channel);
    const unsigned long long ran = run_now() - started;
    if (tessera_job_release(client, &ticket, &error) == 0L)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: ");
        mmgr_line_put(&engine_note.line, label);
        mmgr_line_put(&engine_note.line, " released after ");
        mmgr_line_decimal(&engine_note.line, ran / RUN_MILLION, 1u);
        mmgr_line_put(&engine_note.line, ".");
        mmgr_line_decimal(&engine_note.line, (ran % RUN_MILLION) / RUN_THOUSAND, 3u);
        mmgr_line_put(&engine_note.line, " s, exit ");
        mmgr_line_decimal_signed(&engine_note.line, code);
        mmgr_line_put(&engine_note.line, ", peak ");
        mmgr_line_decimal(&engine_note.line, ticket.last_peak / TESSERA_HOST_PROCESSOR, 1u);
        mmgr_line_put(&engine_note.line, ".");
        mmgr_line_decimal(&engine_note.line, ticket.last_peak % TESSERA_HOST_PROCESSOR, 3u);
        mmgr_line_put(&engine_note.line, " processors");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: ");
        mmgr_line_put(&engine_note.line, label);
        mmgr_line_put(&engine_note.line, " ended with exit ");
        mmgr_line_decimal_signed(&engine_note.line, code);
        mmgr_line_put(&engine_note.line, ", and the daemon did not answer its release");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return code;
}
