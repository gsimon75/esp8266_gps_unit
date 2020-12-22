const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("admin");
const utils = require("../utils");


function op_healthz(req) {
    logger.debug("GET healthz");
    //utils.dump_request(req);
    return Promise.resolve(utils.result(200, "ok"));
}


function op_logout(req) {
    logger.debug("GET logout");
    req.session.uid = req.session.euid = -1;
    return Promise.resolve(utils.result(204));
}


// health check for load balancers
router.get("/healthz",          (req, res, next) => utils.mwrap(req, res, next, () => op_healthz(req)));

// client session handling (mostly for testing)
router.get("/logout",           (req, res, next) => utils.mwrap(req, res, next, () => op_logout(req)));

// admin ops
router.use("/unit", require("./unit"));

module.exports = router;

// vim: set sw=4 ts=4 et:
