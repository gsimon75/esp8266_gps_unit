const loghack   = require("./loghack");

exports.Keeper = class {
    constructor(serverModule, before, beforeEach, afterEach, after) {
        const self = this;
        this.serverModule = serverModule;

        before(function (done) {
            loghack.start("discard");
            self.server = require(self.serverModule);
            done();
        });

        afterEach(function (done) {
            if (this.currentTest.state === "failed") {
                loghack.flush();
            }
            else {
                loghack.discard();
            }
            done();
        });

        after(function(done) {
            self.server.close().then(() => {
                delete require.cache[require.resolve(self.serverModule)];
                loghack.stop();
                done();
            });
        });
    }
}

exports.Restarter = class {
    constructor(serverModule, before, beforeEach, afterEach, after) {
        const self = this;
        this.serverModule = serverModule;

        before(function(done) {
            self.server = null;
            done();
        });

        beforeEach(function(done) {
            loghack.start("discard");
            self.server = require("../index");
            done();
        });

        afterEach(function(done) {
            self.server.close().then(() => {
                delete require.cache[require.resolve(self.serverModule)];
                if (this.currentTest.state === "failed") {
                    loghack.flush();
                }
                loghack.stop();
                done();
            });
        });
    }
}

exports.set_cookies = function(res) {
    return res.headers["set-cookie"].map(h => h.replace(/;.*/, "")).join(";");
}

exports.dump = function(err, res) {
    console.log("res=" + JSON.stringify(res));
    console.log("err=" + JSON.stringify(err));
}

/* Usage:

const ServerManager = require("./server_manager");
// ...

describe("Language idempotent", function() {
    var mgr = new ServerManager.Keeper("../index", before, beforeEach, afterEach, after);
    // ... or ServerManager.Restarter

    it("get all languages", function(done) {
        chai.request(mgr.server).keepOpen().get("/v0/language").end((err, res) => {
            expect(res).status(200).json;
            expect(JSON.parse(res.text)).include.something.like({id: "eng", name: "English"});
            done();
        });
    });
    
    // ...

});
*/

// vim: set ts=4 sw=4 et:
