const chai          = require("chai");
const expect        = chai.expect;
const tokens        = require("./tokens");
const ServerManager = require("./server_manager");

chai.use(require("chai-http"));
chai.use(require("chai-json"));
chai.use(require("chai-like"));
chai.use(require("chai-things"));

// junk chaining words: to be been is that which and has have with at of same but does still

describe("Constants/Global", function() {
    var mgr = new ServerManager.Keeper("../index", before, beforeEach, afterEach, after);

    it("whoami anonymous", function(done) {
        chai.request(mgr.server).keepOpen().get("/v0/whoami").end((err, res) => {
            ServerManager.dump(err, res);
            expect(res).status(200).json;
            expect(JSON.parse(res.text)).like({uid: -1, euid: -1, is_admin: false, is_trainer: false});
            done();
        });
    });

    if (tokens.expired) {

        it("whoami with expired token", function(done) {
            chai.request(mgr.server).keepOpen().get("/v0/whoami").set("Authorization", tokens.expired).end((err, res) => {
                ServerManager.dump(err, res);
                expect(res).status(403);
                done();
            });
        });

        it("client operation with expired token", function(done) {
            chai.request(mgr.server).keepOpen().get("/v0/unit/test-1").set("Authorization", tokens.expired).end((err, res) => {
                ServerManager.dump(err, res);
                expect(res).status(403);
                done();
            });
        });

        it("unit operation with expired token", function(done) {
            chai.request(mgr.server).keepOpen().post("/v0/trace").set("Authorization", tokens.expired).end((err, res) => {
                ServerManager.dump(err, res);
                expect(res).status(403);
                done();
            });
        });

    }

    if (tokens.client) {

        it("whoami with client token", function(done) {
            chai.request(mgr.server).keepOpen().get("/v0/whoami").set("Authorization", tokens.client).end((err, res) => {
                ServerManager.dump(err, res);
                expect(res).status(200).json;
                expect(JSON.parse(res.text)).like({uid: tokens.client_id, euid: tokens.client_id);
                done();
            });
        });

        it("cliebnt operation with client token", function(done) {
            chai.request(mgr.server).keepOpen().get("/v0/unit/test-1").set("Authorization", tokens.client).end((err, res) => {
                ServerManager.dump(err, res);
                expect(res).status(200);
                done();
            });
        });

        it("unit operation with client token", function(done) {
            chai.request(mgr.server).keepOpen().post("/v0/trace").set("Authorization", tokens.client).end((err, res) => {
                ServerManager.dump(err, res);
                expect(res).status(403);
                done();
            });
        });

    }

});

// vim: set ts=4 sw=4 et:
