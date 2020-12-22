const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("backend");
const utils = require("../utils");


function op_healthz(req) {
    logger.debug("GET healthz");
    //utils.dump_request(req);
    return Promise.resolve(utils.result(200, "ok"));
}


// health check for load balancers
router.get("/healthz",          (req, res, next) => utils.mwrap(req, res, next, () => op_healthz(req)));

// unit ops
//router.use("/report", require("./report"));


module.exports = router;

// vim: set sw=4 ts=4 et:
