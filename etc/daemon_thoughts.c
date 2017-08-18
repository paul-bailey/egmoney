
static void
signal_handler(int sig)
{
        /* TODO: Safely handle SIGHUP */
}

static int
parse_args(int argc, char **argv)
{
        static const struct option options[] = {
                { "help",    no_argument, NULL, '?' },
                { NULL, 0, NULL, '\0' },
        }

        for (;;) {
                int opt = getopt_long(argc, argv, "?", options, NULL);
                if (opt < 0)
                        break;

                switch (opt) {
                case '?':
                        usage(stdout, argv[0]);
                        exit(EXIT_SUCCESS);
                        break;
                default:
                        usage(stderr, argv[0]);
                        exit(EXIT_FAILURE);
                        break;
                }
        }
}

static int
initialize(void)
{
        if (configure_syslog() < 0)
                return -1;
        if (configure_server_socket() < 0)
                return -1;
        /* TODO: set up signal handler */
        if (daemon(0, 0)) {
                logerror("Cannot daemonize: %s", strerror(errno));
                fprintf(stderr, "Cannot daemonize: %s", strerror(errno));
                return -1;
        }
        return 0;
}

static int
mainloop(void)
{
}

int
main(int argc, char *argv[])
{
        int ret;

        parse_args(argc, argv);
        if (initialize() != 0) {
                logerror("Failed during initialization: %s", strerror(-ret));
                return 1;
        }

        logdebug("Waiting for connections.\n");
        do {
                ret = mainloop();
        } while (!ret);

        return ret;
}
