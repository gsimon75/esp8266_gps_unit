const EventEmitter = require("events");

class AdminEventEmitter extends EventEmitter {}

const admin_event_emitter = new AdminEventEmitter();

var fake_msg_timer = setInterval(() => {
    // https://stackoverflow.com/questions/56450228/getting-neterr-incomplete-chunked-encoding-200-when-consuming-event-stream-usi
    // Stop bitching, chrome!
    admin_event_emitter.emit("sendit", "keepalive", null);
}, 60 * 1000);

module.exports = {
    admin_event_emitter,
}

// vim: set sw=4 ts=4 et:
