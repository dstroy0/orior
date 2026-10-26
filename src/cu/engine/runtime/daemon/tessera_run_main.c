// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_run_main.c: tessera_run's main
#include "tessera_run_internal.h"

_Alignas(8) static const
    char s_run_usage[] = "  tessera_run: --processors <count> [--name <text>] [--child] -- <command> [arguments]\n"
                         "  tessera_run: --parent <records> [--name <text>] -- <command> [arguments]\n";
_Alignas(8) static const
    char s_run_unsubmitted[] = "  tessera_run: the job could not be submitted (its signum, or the daemon's place)\n";

int main(int count, char **arguments)
{
    RunRequest request;
    memset(&request, 0, sizeof(request));
    if (!run_arguments(count, arguments, &request))
    {
        fputs(s_run_usage, stderr);
        return RUN_FAILED;
    }
    const unsigned long long mask = tessera_self_host_mask();
    // a processor count is at most sixty-four, represented whole
    const unsigned long long given = (unsigned long long)engine_word_population(mask);
    if ((mask == 0ull) || (request.processors > given))
    {
        fprintf(stderr, "  tessera_run: requests %llu processors, and the host gives jobs %llu\n", request.processors,
                given);
        return RUN_FAILED;
    }
    const char *const label = (request.name != NULL) ? request.name : arguments[request.command];
    if (request.parent != NULL)
    {
        return run_child(&request, arguments, count, mask, label);
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    char daemon[ENGINE_PATH_CAPACITY];
    TesseraJobSubmit submit;
    memset(&submit, 0, sizeof(submit));
    if (!run_daemon_path(daemon, sizeof(daemon)) || !run_signum(&request, arguments, count, &submit.signum, &error))
    {
        fputs(s_run_unsubmitted, stderr);
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
        fprintf(stderr, "  tessera_run: %s was not admitted by the host's daemon (%s)\n", label, daemon);
        return RUN_FAILED;
    }
    const unsigned long long waited = run_now() - submitted_at;
    fprintf(stderr,
            "  tessera_run: %s admitted after %llu.%03llu s, %llu.%03llu of %llu processors reserved, on processors "
            "0x%llx at below normal priority\n",
            label, waited / RUN_MILLION, (waited % RUN_MILLION) / RUN_THOUSAND, ticket.granted / TESSERA_HOST_PROCESSOR,
            ticket.granted % TESSERA_HOST_PROCESSOR, given, mask);
    RunChannel channel;
    memset(&channel, 0, sizeof(channel));
    int started_count = count - request.command;
    char **const watched = run_channel_open(&channel, &request, arguments + request.command, count - request.command,
                                            &submit.signum, &started_count);
    char *const *const words = (watched != NULL) ? watched : (arguments + request.command);
    RunChild child;
    memset(&child, 0, sizeof(child));
    int code = RUN_NOT_STARTED;
    // the whole processors of the reservation, and never none: a job reserves its tracked peak where that is more than
    // it named, and the count of whole ones in it is what the command may run on
    const unsigned long long whole = ticket.granted / TESSERA_HOST_PROCESSOR;
    if (!run_processors_named((whole != 0ull) ? whole : 1ull))
    {
        fprintf(stderr, "  tessera_run: the processors granted were not handed on to %s\n", label);
    }
    const unsigned long long started = run_now();
    if (run_start(&child, words, started_count, mask, &channel))
    {
        run_wait(&child, &channel, client, &ticket, &error);
        code = run_finish(&child);
    }
    else
    {
        fprintf(stderr, "  tessera_run: %s could not be started\n", arguments[request.command]);
    }
    free(watched);
    run_channel_close(&channel);
    const unsigned long long ran = run_now() - started;
    if (tessera_job_release(client, &ticket, &error) == 0L)
    {
        fprintf(stderr, "  tessera_run: %s released after %llu.%03llu s, exit %d, peak %llu.%03llu processors\n", label,
                ran / RUN_MILLION, (ran % RUN_MILLION) / RUN_THOUSAND, code, ticket.last_peak / TESSERA_HOST_PROCESSOR,
                ticket.last_peak % TESSERA_HOST_PROCESSOR);
    }
    else
    {
        fprintf(stderr, "  tessera_run: %s ended with exit %d, and the daemon did not answer its release\n", label,
                code);
    }
    return code;
}
