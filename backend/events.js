const logger = require("./logger").getLogger("event");
const EventEmitter = require("events");

class GPSEventEmitter extends EventEmitter {}

// TODO: do we need separate ones?
const admin_event_emitter = new GPSEventEmitter();
const customer_event_emitter = new GPSEventEmitter();

var keepalive_timer = setInterval(() => {
    // https://stackoverflow.com/questions/56450228/getting-neterr-incomplete-chunked-encoding-200-when-consuming-event-stream-usi
    // Stop bitching, chrome!
    admin_event_emitter.emit("sendit", "keepalive", null);
    customer_event_emitter.emit("sendit", "keepalive", null);
}, 60 * 1000);


function dispatcher(req, res, emitter, filter = null) {
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
        if (etype == null) {
            emitter.removeListener("sendit", handler);
        }
        else if (!filter || filter(etype, data)) {
            setImmediate(() => {
                if (etype !== null) {
                    res.write("event: " + etype + "\ndata: " + JSON.stringify(data) + "\n\n");
                }
                else {
                    res.write("event: end\ndata: " + JSON.stringify(data) + "\n\n");
                    res.end();
                }
            });
        }
    };

    emitter.addListener("sendit", handler);

    // If client closes connection, stop sending events
    res.on("close", () => {
        logger.debug("event for " + req.session.email + ": client dropped me");
        emitter.removeListener("sendit", handler);
        res.end();
    });
}


module.exports = {
    admin_event_emitter,
    customer_event_emitter,
    dispatcher,
}

// vim: set sw=4 ts=4 et:
