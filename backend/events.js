const logger = require("./logger").getLogger("event");
const utils = require("./utils");
const EventEmitter = require("events");

const emitter = new EventEmitter();
const fetch_emitter = new EventEmitter();

var keepalive_timer = setInterval(() => {
    // https://stackoverflow.com/questions/56450228/getting-neterr-incomplete-chunked-encoding-200-when-consuming-event-stream-usi
    // Stop bitching, chrome!
    emitter.emit("sendit", "keepalive", null);
    fetch_emitter.emit("sendit", { keepalive: true });
}, 60 * 1000);


function dispatcher(req, res, filter = null) {
    utils.require_client(req);
    logger.debug("event for " + req.session.email + ": start");

    // NOTE: to prevent client-side from further reconnecting, send 204
    // "Also, there will be no reconnection if the response has an incorrect Content-Type or its HTTP status differs from 301, 307, 200 and 204."
    res.setHeader("Content-Type", "text/event-stream");
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("X-Accel-Buffering", "no");
    utils.add_cors_response_headers(res, req.headers.origin);
    res.flushHeaders(); // flush the headers to establish SSE with client

    let handler = (etype, data) => {
        logger.debug("event for " + req.session.email + ": etype=" + etype + ", data=" + JSON.stringify(data));
        if (etype == null) {
            emitter.removeListener("sendit", handler);
        }
        if (!filter || filter(etype, data)) {
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


function fetch_dispatcher(req, res, filter = null) {
    utils.require_client(req);
    logger.debug("fetch_event for " + req.session.email + ": start");

    // NOTE: to prevent client-side from further reconnecting, send 204
    // "Also, there will be no reconnection if the response has an incorrect Content-Type or its HTTP status differs from 301, 307, 200 and 204."
    res.setHeader("Content-Type", "text/x-json-stream");
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("X-Accel-Buffering", "no");
    utils.add_cors_response_headers(res, req.headers.origin);
    res.flushHeaders(); // flush the headers to establish SSE with client

    let handler = (data) => {
        logger.debug("fetch_event for " + req.session.email + ", data=" + JSON.stringify(data));
        if (data == null) {
            fetch_emitter.removeListener("sendit", handler);
        }
        if (!filter || filter(data)) {
            setImmediate(() => {
                logger.debug("fetch_event sending " + req.session.email + ", data=" + JSON.stringify(data));
                res.write(JSON.stringify(data) + "\n");
                if (data === null) {
                    res.end();
                }
            });
        }
    };

    fetch_emitter.addListener("sendit", handler);

    // If client closes connection, stop sending events
    res.on("close", () => {
        logger.debug("event for " + req.session.email + ": client dropped me");
        fetch_emitter.removeListener("sendit", handler);
        res.end();
    });
}


module.exports = {
    emitter,
    fetch_emitter,
    dispatcher,
    fetch_dispatcher,
}

// vim: set sw=4 ts=4 et:
