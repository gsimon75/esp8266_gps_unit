const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("client");
const utils = require("../utils");


function op_healthz(req) {
    logger.debug("GET healthz");
    //utils.dump_request(req);
    return "ok";
}


function op_whoami(req) {
    logger.debug("GET whoami");
    return {
        uid: req.session.uid,
        euid: req.session.euid,
    };
}


function op_logout(req) {
    logger.debug("GET logout");
    req.session.uid = req.session.euid = -1;
    return null;
}


// health check for load balancers
router.get("/healthz",          (req, res, next) => utils.mwrap(req, res, next, () => op_healthz(req)));

// client session handling (mostly for testing)
router.get("/whoami",           (req, res, next) => utils.mwrap(req, res, next, () => op_whoami(req)));
router.get("/logout",           (req, res, next) => utils.mwrap(req, res, next, () => op_logout(req)));

// client ops
//router.use("/station", require("./station"));
router.use("/available", require("./available")); // available units


module.exports = router;

// vim: set sw=4 ts=4 et:
