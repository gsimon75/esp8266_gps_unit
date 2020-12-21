## FIXMEs

- id arg in District ops req body won't be mandatory and will be ignored -> remove them from the tests
- ditto for Nutrient



## To restart the server between tests or to keep it

Restart: clean instance every time, no possibility of a polluted internal stat
Keep: performance (a lot of it), less init logs


### To keep the server

```
describe("Language idempotent", function() {
    loghack.start("discard");
    var server = require("../index");
    
    afterEach(function(done) {
        if (this.currentTest.state === "failed") {
            loghack.flush();
        }
        loghack.stop();
        done();
    });

    after(function(done) {
        server.close().then(() => {
            delete require.cache[require.resolve("../index")];
            done();
        });
    });

    it("get all languages", function(done) {
        chai.request(server).keepOpen().get("/v0/language").end((err, res) => {
            expect(res).status(200).json;
            expect(JSON.parse(res.text)).include.something.like({id: "eng", name: "English"});
            done();
        });
    });
    // ...
});
```

### To restart the server for every test

```
describe("Language clean-start", function() {
    var server;

    beforeEach(function(done) {
        loghack.start("discard");
        server = require("../index");
        done();
    });

    afterEach(function(done) {
        server.close().then(() => {
            delete require.cache[require.resolve("../index")];
            if (this.currentTest.state === "failed") {
                loghack.flush();
            }
            loghack.stop();
            done();
        });
    });

    it("delete test language", function(done) {
        chai.request(server).keepOpen().delete("/v0/language/xyz").end((err, res) => {
            expect(res).status(204);
            done();
        });
    });

});
```

