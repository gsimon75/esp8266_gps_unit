const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("admin");
const utils = require("../utils");

const fba = require("firebase-admin");

const fcm_server_key = "AAAAUs-s504:APA91bGVx_Pt069CtyDVGeATpLXU8XUhuAjHBBGzfVrlXPZAOyuWSNoRYKvrTNW9TafH-k9YYmpVZjVFecUyqqN8RNxqfZAEX8hi2besxaVa8YA104q-WVrTIMfKaq7lEEv6XhCIly34";
const fcm_sender_id = 355671533390;
const fcm_public_key = "BDqga4b_taIPZB7XpFUw2CjbDRP8tZu1lKjNUOQS1JyJRpA_5GN35aa-EbaSznWmaaa5a8gcGKgg36rMtLA_F5o";
const fcm_private_key = "W4y43tBa-0i1bFVLEkwKLNRY9O3Ro3-22gN--0e4VF0";


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
        is_admin: req.session.is_admin,
        is_technician: req.session.is_technician,
        provider: req.session.provider,
    };
    logger.debug("Session vars: " + JSON.stringify(result));
    return result;
}


function op_logout(req) {
    logger.debug("GET logout");
    return utils.promisify(req.session.destroy);
}


function op_subscribe(req) {
    logger.debug("POST subscribe");
    const currentToken = req.body.fcm_reg_token;
    logger.debug("currentToken=" + currentToken);
    fba.messaging().subscribeToTopic([ currentToken ], "admin_t").then(response => {
        // See the MessagingTopicManagementResponse reference documentation for the contents of response.
        logger.debug("Successfully subscribed to topic: " + JSON.stringify(response));
    }).catch(error => {
        logger.error("Error subscribing to topic: " + error);
    });
    return "ok";
}



// health check for load balancers
router.get("/healthz",          (req, res, next) => utils.mwrap(req, res, next, () => op_healthz(req)));

// client session handling (mostly for testing)
router.get("/whoami",           (req, res, next) => utils.mwrap(req, res, next, () => op_whoami(req)));
router.get("/logout",           (req, res, next) => utils.mwrap(req, res, next, () => op_logout(req)));
router.post("/subscribe",       (req, res, next) => utils.mwrap(req, res, next, () => op_subscribe(req)));

// administration of units
router.use("/unit", require("./unit"));

/*
 * 1. Where are the units? -> List of units: id, name, last location, status, charge, user
 * Narrowing: 
 * - by date (until (def: now), days, hours, minutes, seconds (def: 5 min)
 * - by status (offline, charging, available, in_use)
 * Endpoint: GET /unit/status?until=20210131T112233Z&hours=3&status=available
 * Endpoint: GET /unit/status/Unit%201
 *
 * 2. Where was a unit? -> List of unit positions (location, charge)
 * - by date (until (def: now), days, hours, minutes, seconds (def: 5 min)
 * Endpoint: GET /unit/trace/Unit%201?until=20210131T112233Z&hours=3
 *
 * 3. History of a unit -> List of unit state transitions  (location, status, charge, user)
 * Narrowing: 
 * - by date (until (def: now), days, hours, minutes, seconds (def: latest only)
 * Endpoint: GET /unit/history/Unit%201?until=20210131T112233Z&hours=3
 *
 */

// administration of users
//router.use("/user", require("./user"));
/*
 * - list users
 * - add/delete new user
 * - set/clear technician status
 * - set/clear admin status (cannot clear it on self)
 * - ? ban/disable users ?
 * - ? manage user credits (if there will be such) ?
 */

// administration of stations
router.use("/station", require("./station"));
/*
 * - list stations
 * - add/delete stations
 * - manage station: name, loc, capacity, in_use
 */

module.exports = router;

// vim: set sw=4 ts=4 et:
