/*
 * robin_cli.c
 *
 * Handles the UI on the command line.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "robin_api.h"
#include "robin_cli.h"


/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_CLI, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_CLI, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_CLI, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_CLI, fmt, ## args)


/*
 * Local types and macros
 */

#define ROBIN_CLI_EMAIL_LEN 64

typedef enum robin_cli_cmd_ret {
    ROBIN_CMD_ERR = -1,
    ROBIN_CMD_OK = 0,
    ROBIN_CMD_QUIT
} robin_cli_cmd_ret_t;

typedef struct robin_cli {
    /* Robin readline */
    char *buf;
    size_t len;

    /* Robin Command */
    int argc;
    char **argv;

    /* Robin User */
    int logged;
    char email[ROBIN_CLI_EMAIL_LEN];
} robin_cli_t;

typedef struct robin_cli_cmd {
    char *name;
    char *usage;
    char *desc;
    robin_cli_cmd_ret_t (*fn)(robin_cli_t *cli);
} robin_cli_cmd_t;

#define ROBIN_CLI_CMD_FN(name, cli) \
    robin_cli_cmd_ret_t rc_cmd_##name(robin_cli_t *cli)
#define ROBIN_CLI_CMD_FN_DECL(name) static ROBIN_CLI_CMD_FN(name,)
#define ROBIN_CLI_CMD_ENTRY(cmd_name, cmd_usage, cmd_desc) { \
    .name = #cmd_name,                                       \
    .usage = cmd_usage,                                      \
    .desc = cmd_desc,                                        \
    .fn = rc_cmd_##cmd_name                                  \
}
#define ROBIN_CLI_CMD_ENTRY_NULL { \
    .name = NULL,                  \
    .usage = NULL,                 \
    .desc = NULL,                  \
    .fn = NULL                     \
}


/*
 * Robin CLI Command functions declaration
 */

ROBIN_CLI_CMD_FN_DECL(help);
ROBIN_CLI_CMD_FN_DECL(register);
ROBIN_CLI_CMD_FN_DECL(login);
ROBIN_CLI_CMD_FN_DECL(logout);
ROBIN_CLI_CMD_FN_DECL(quit);


/*
 * Local data
 */

static robin_cli_cmd_t robin_cmds[] = {
    ROBIN_CLI_CMD_ENTRY(help, "",
                         "print this help"),
    ROBIN_CLI_CMD_ENTRY(register, "<email> <password>",
                         "register to Robin with email and password"),
    ROBIN_CLI_CMD_ENTRY(login, "<email> <password>",
                         "login to Robin with email and password"),
    ROBIN_CLI_CMD_ENTRY(logout, "",
                         "logout from Robin"),
    ROBIN_CLI_CMD_ENTRY(quit, "",
                         "terminate the connection with the server"),
    ROBIN_CLI_CMD_ENTRY_NULL /* terminator */
};

static robin_cli_t *robin_cli;

/*
 * Local functions
 */

static robin_cli_t *rcli_alloc()
{
    robin_cli_t *cli;

    cli = calloc(1, sizeof(robin_cli_t));
    if (!cli) {
        err("calloc: %s", strerror(errno));
        return NULL;
    }

    return cli;
}

static void rcli_free(robin_cli_t *cli)
{
    if (cli->buf) {
        dbg("cli_free: buf=%p", cli->buf);
        free(cli->buf);
    }

    if (cli->argv) {
        dbg("cli_free: argv=%p", cli->argv);
        free(cli->argv);
    }

    dbg("cli_free: cli=%p", cli);
    free(cli);
}

static int rcli_cmdparse(robin_cli_t *cli)
{
    char *start_arg, *end_arg;
    int last = 0;

    cli->argc = 0;

    start_arg = cli->buf;
    do {
        if (*start_arg == ' ') {
            /* discard continuos whitespaces */
            while (*start_arg == ' ')
                start_arg++;
        }

        if (*start_arg == '\0') {
            /* no more arguments */
            return 0;
        } else if (*start_arg == '"') {
            /* if next arg starts with double quotes, search for the closing
            * double quotes to store the whole string as one argument
            */
            end_arg = strchr(++start_arg, '"');
            if (!end_arg)
                return 0;
        } else
            end_arg = strchr(start_arg, ' ');

        if (end_arg)
            *end_arg = '\0';
        else
            last = 1;

        cli->argv = realloc(cli->argv, (cli->argc + 1) * sizeof(char *));
        if (!cli->argv) {
            err("realloc: %s", strerror(errno));
            return -1;
        }

        dbg("rcli_cmdparse: arg #%d: %s", cli->argc, start_arg);
        cli->argv[cli->argc++] = start_arg;  /* store new arg */

        start_arg = end_arg + 1;
    } while (!last);

    return 0;
}


/*
 * Robin CLI Command function definitions
 */

ROBIN_CLI_CMD_FN(help, cli)
{
    robin_cli_cmd_t *cmd;

    if (cli->argc != 1) {
        err("invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    for (cmd = robin_cmds; cmd->name != NULL; cmd++)
        printf("%-10s %-20s\t%s\n", cmd->name, cmd->usage, cmd->desc);

    return ROBIN_CMD_OK;
}

ROBIN_CLI_CMD_FN(register, cli)
{
    char *email, *psw;
    int ret;

    dbg("%s", cli->argv[0]);

    if (cli->argc != 3) {
        warn("invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    email = cli->argv[1];
    psw = cli->argv[2];

    dbg("%s: email=%s psw=%s", cli->argv[0], email, psw);

    ret = robin_api_register(email, psw);
    if (ret < 0) switch (-ret) {
        case 1:
            err("could not register the new user into the system");
            return ROBIN_CMD_ERR;

        case 2:
            printf("invalid email/password format\n");
            return ROBIN_CMD_OK;

        case 3:
            printf("user %s is already registered\n", email);
            return ROBIN_CMD_OK;

        default:
            err("unexpected error occurred");
            return ROBIN_CMD_ERR;
    }

    printf("user registered successfully\n");

    return ROBIN_CMD_OK;
}

ROBIN_CLI_CMD_FN(login, cli)
{
    char *email, *psw;
    int ret;

    dbg("%s", cli->argv[0]);

    if (cli->argc != 3) {
        warn("invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    email = cli->argv[1];
    psw = cli->argv[2];

    dbg("%s: email=%s psw=%s", cli->argv[0], email, psw);

    if (cli->logged) {
        printf("you are already logged in as %s\n", cli->email);
        return ROBIN_CMD_OK;
    }

    ret = robin_api_login(email, psw);
    if (ret < 0) switch (-ret) {
        case 1:
            err("server error, could not perform login");
            return ROBIN_CMD_ERR;

        case 2:
            printf("you are already logged in as %s\n", email);
            cli->logged = 1;
            strcpy(cli->email, email);
            return ROBIN_CMD_OK;

        case 3:
            printf("%s is already logged in from another client\n", email);
            return ROBIN_CMD_OK;

        case 4:
            printf("invalid email\n");
            return ROBIN_CMD_OK;

        case 5:
            printf("invalid password\n");
            return ROBIN_CMD_OK;

        default:
            err("unexpected error occurred");
            return ROBIN_CMD_ERR;
    }

    /* save login information */
    cli->logged = 1;
    strcpy(cli->email, email);

    printf("login successfull\n");

    return ROBIN_CMD_OK;
}

ROBIN_CLI_CMD_FN(logout, cli)
{
    int ret;

    dbg("%s", cli->argv[0]);

    if (cli->argc != 1) {
        warn("invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    if (!cli->logged) {
        printf("you must login first\n");
        return ROBIN_CMD_OK;
    }

    ret = robin_api_logout();
    if (ret < 0) switch (-ret) {
        case 1:
            err("server error, could not perform login");
            return ROBIN_CMD_ERR;

        case 2:
            printf("you must login first\n");
            cli->logged = 0;
            *(cli->email) = '\0';
            return ROBIN_CMD_OK;

        default:
            err("unexpected error occurred");
            return ROBIN_CMD_ERR;
    }

    /* save login information */
    cli->logged = 0;
    *(cli->email) = '\0';

    printf("logout successfull\n");

    return ROBIN_CMD_OK;
}

ROBIN_CLI_CMD_FN(quit, cli)
{
    dbg("%s", cli->argv[0]);

    if (cli->argc != 1) {
        warn("invalid number of arguments");
        return ROBIN_CMD_OK;
    }

    return ROBIN_CMD_QUIT;
}


/*
 * Exported functions
 */

void robin_cli_manage(int fd)
{
    ssize_t nread;
    robin_cli_t *cli;
    robin_cli_cmd_t *cmd;

    /* setup the context for this CLI */
    cli = rcli_alloc();
    if (!cli)
        goto manager_early_quit;

    robin_cli = cli;
    robin_api_init(fd);

    while (1) {
        if (cli->logged)
            printf("robin (%s)> ", cli->email);
        else
            printf("robin> ");
        nread = getline(&cli->buf, &cli->len, stdin);
        if (nread < 0) {
            if (errno == 0) {
                warn("EOF reached");
                goto manager_quit;
            } else {
                err("getline: %s", strerror(errno));
                goto manager_quit;
            }
        }

        /* substitute \n or \r\n with \0 */
        if (nread > 1 && cli->buf[nread - 2] == '\r')
            cli->buf[nread - 2] = '\0';
        else
            cli->buf[nread - 1] = '\0';

        dbg("command received: \"%s\"", cli->buf);

        /* parse the command in argc-argv form and store it in cli */
        if (rcli_cmdparse(cli) < 0) {
            err("rc_cmdparse: failed to parse command");
            goto manager_quit;
        }

        /* blank line */
        if (cli->argc < 1)
            continue;

        /* search for the command */
        for (cmd = robin_cmds; cmd->name != NULL; cmd++) {
            if (!strcmp(cli->argv[0], cmd->name)) {
                /* execute cmd and evaluate the returned value */
                switch (cmd->fn(cli)) {
                    case ROBIN_CMD_OK:
                        break;

                    case ROBIN_CMD_ERR:
                        err("failed to execute the requested command");
                    case ROBIN_CMD_QUIT:
                        goto manager_quit;
                }
                break;
            }
        }

        if (cmd->name == NULL)
            warn("invalid command; type help for the list of availble commands");
    }

manager_quit:
    rcli_free(robin_cli);
    robin_cli = NULL;
    robin_api_free();
manager_early_quit:
    printf("CLI manager has been closed\n");
}

void robin_cli_terminate(void)
{
    robin_cli_t *cli = robin_cli;

    if (cli)
        rcli_free(cli);

    robin_api_free();
}
