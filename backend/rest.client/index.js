const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("client");
const utils = require("../utils");
const events = require("../events");
const cache = require("../cache");

const fba = require("firebase-admin");


function op_healthz(req) {
    logger.debug("GET healthz");
    //utils.dump_request(req);
    return "ok";
}


function op_whoami(req) {
    logger.debug("GET whoami");
    const result = {
        email: req.session.email,
        name: req.session.name,
        provider: req.session.provider,
    };
    logger.debug("Session vars: " + JSON.stringify(result));
    return result;
}


function op_logout(req) {
    logger.debug("GET logout");
    req.session.destroy();
    return "ok";
}


// health check for load balancers
router.get("/healthz",          (req, res, next) => utils.mwrap(req, res, next, () => op_healthz(req)));

// client session handling (mostly for testing)
router.get("/whoami",           (req, res, next) => utils.mwrap(req, res, next, () => op_whoami(req)));
router.get("/logout",           (req, res, next) => utils.mwrap(req, res, next, () => op_logout(req)));

// client ops
router.use("/station", require("./station"));
router.use("/unit", require("./unit"));

router.use("/station", require("./station"));

function event_filter(session, etype, u) {
    if (etype == "unit") {
        if (u.user == session.email) {
            return true; // notify the user about his units
        }
        if ((u.status == "available") || (u.status == "charging")) {
            return true; // notify the user about publicly available units
        }
    }
    return false; // strict policy: all blocked unless permitted explicitely
}

router.get("/event", (req, res) => events.dispatcher(req, res, (etype, u) => event_filter(req.session, etype, u)));

module.exports = router;

// vim: set sw=4 ts=4 et:
