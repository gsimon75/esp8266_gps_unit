
function start(policy) {
    if (!("loghack" in console)) {
        console.loghack = {
            policies: [],
            loggers: [],
            buffers: [],
        };
    }
    console.loghack.policies.unshift(policy);
    console.loghack.loggers.unshift(console.log);
    console.loghack.buffers.unshift("");
    console.log = (msg) => {
        if (console.loghack.buffers[0] === "") {
            console.loghack.buffers[0] += msg;
        }
        else {
            console.loghack.buffers[0] += "\n" + msg;
        }
    };
}

function flush() {
    if (console.loghack.buffers[0] !== "") {
        console.loghack.loggers[0](console.loghack.buffers[0]);
        console.loghack.buffers[0] = "";
    }
}
function discard() {
    console.loghack.buffers[0] = "";
}

function stop() {
    var policy = console.loghack.policies.shift();
    if (policy === "flush")
        flush();
    console.loghack.buffers.shift();
    console.log = console.loghack.loggers.shift();
}

exports.start = start;
exports.flush = flush;
exports.discard = discard;
exports.stop = stop;

// vim: set ts=4 sw=4 et:
