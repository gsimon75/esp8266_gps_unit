const express = require("express");
const router = express.Router();
const db = require("../database");
const logger = require("../logger").getLogger("admin");
const utils = require("../utils");
const events = require("../events");

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
        is_admin: req.session.is_admin,
        is_technician: req.session.is_technician,
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
router.use("/user", require("./user"));
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

// FIXME: for testing: send FCM data msgs periodically (this will be done on data changes)
/*
router.ws("/event", function(ws, req) {
    logger.debug("Incoming req: " + JSON.stringify(req));
    ws.on("message", function(msg) {
        logger.debug("Incoming msg: " + JSON.stringify(msg));
        ws.send(msg);
    });
});
*/

router.get("/event", (req, res) => {
    logger.debug("event for " + req.session.email + ": start");

    // NOTE: to prevent client-side from further reconnecting, send 204
    // "Also, there will be no reconnection if the response has an incorrect Content-Type or its HTTP status differs from 301, 307, 200 and 204."
    res.setHeader("Content-Type", "text/event-stream");
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("X-Accel-Buffering", "no");
    res.setHeader("Access-Control-Allow-Origin", "*");
    res.flushHeaders(); // flush the headers to establish SSE with client

    let handler = (etype, data) => {
        logger.debug("event for " + req.session.email + ": got data to send, etype=" + etype + ", data=" + JSON.stringify(data));
        setImmediate(() => {
            if (etype !== null) {
                res.write("event: " + etype + "\ndata: " + JSON.stringify(data) + "\n\n");
            }
            else {
                res.write("event: end\ndata: " + JSON.stringify(data) + "\n\n");
                res.end();
            }
        });
        if (etype == null) {
            events.admin_event_emitter.removeListener("sendit", handler);
        }
    };

    events.admin_event_emitter.addListener("sendit", handler);

    // If client closes connection, stop sending events
    res.on("close", () => {
        logger.debug("event for " + req.session.email + ": client dropped me");
        events.admin_event_emitter.removeListener("sendit", handler);
        res.end();
    });
});

module.exports = router;

// vim: set sw=4 ts=4 et:
