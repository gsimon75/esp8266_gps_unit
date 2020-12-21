const log4js = require("log4js");

log4js.configure({
    appenders: {
        logfile: {
            type: "fileSync",
            filename: "gps_tracker.backend.log",
            layout: {
                type: "pattern",
                pattern: "%d [%p] %m",
            },
        },
        stderr: {
            type: "stderr"
        },
    },
    categories: {
        default: {
            "level": "debug",
            appenders: [ "logfile", "stderr" ],
        },
    },
});

module.exports = log4js;

// vim: set ts=4 sw=4 et:
