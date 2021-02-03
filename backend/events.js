const EventEmitter = require("events");

class AdminEventEmitter extends EventEmitter {}

const admin_event_emitter = new AdminEventEmitter();


module.exports = {
    admin_event_emitter,
}

// vim: set sw=4 ts=4 et:
