const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("user");
const utils = require("../utils");

const STATUS_VALUES = [ "offline", "charging", "available", "in_use" ];
const MAX_RESULT_RECORDS = 1024;


function filtered_pipeline(req) {
    const nf = ("num" in req.query) ? parseInt(req.query.num) : MAX_RESULT_RECORDS ;

    var idx_sort_dir = -1;
    var filter = utils.av();

    if ("from" in req.query) {
        if (filter.emain === undefined) filter.email = {}
        filter.email.$gt = req.query.from;
        idx_sort_dir = 1;
    }
    if ("until" in req.query) {
        filter.email.$lt = req.query.until;
        idx_sort_dir = -1;
    }

    if ("is_admin" in req.query) {
        filter.is_admin.$eq = utils.s2bool(req.query.is_admin);
    }
    if ("is_technician" in req.query) {
        filter.is_technician.$eq = utils.s2bool(req.query.is_technician);
    }
    
    if ("email" in req.params) {
        filter.email.$eq = req.params.email;
    }

    var pipe = [
        { $match: filter },
        { $sort: { email: idx_sort_dir }, },
        { $limit: nf },
    ];
    return pipe;
}


function op_get(req) {
    logger.debug("op_get()");
    utils.require_admin(req);
    const pipe = filtered_pipeline(req);
    logger.debug("pipe=" + JSON.stringify(pipe));
    return db.cursor_all(db.users().aggregate(pipe));
}


function op_upsert(req) {
    logger.debug("op_upsert()");
    utils.require_admin(req);
    const email = req.params.email;
    if (!email || (email == "")) {
        throw utils.error(400, "Missing email");
    }
    const new_doc = {
        email,
        is_admin: utils.s2bool(req.body.is_admin),
        is_technician: utils.s2bool(req.body.is_technician),
    };

    return db.users().replaceOne({
        email,
    },
    new_doc, {
        upsert: true,
    }).then(() => null);
}


function op_update(req) {
    logger.debug("op_upsert()");
    utils.require_admin(req);
    const email = req.params.email;
    if (!email || (email == "")) {
        throw utils.error(400, "Missing email");
    }
    let update_doc = utils.av();
    if (req.body.is_admin !== undefined) {
        update_doc.$set.is_admin = utils.s2bool(req.body.is_admin);
    }
    if (req.body.is_technician !== undefined) {
        update_doc.$set.is_technician = utils.s2bool(req.body.is_technician);
    }
    if (!("$set" in update_doc)) {
        return null; // nothing requested -> 204
    }

    return db.users().updateOne({
        email,
    },
    update_doc, {
    }).then(() => null);
}


// FIXME: handle banned/disabled accounts
router.get("/",                 (req, res, next) => utils.mwrap(req, res, next, () => op_get(req)));
router.get("/:email",           (req, res, next) => utils.mwrap(req, res, next, () => op_get(req)));
// post: creating with server-assigned id makes no sense
router.put("/:email",           (req, res, next) => utils.mwrap(req, res, next, () => op_upsert(req)));
router.patch("/:email",         (req, res, next) => utils.mwrap(req, res, next, () => op_update(req)));

module.exports = router;

// vim: set sw=4 ts=4 et:
