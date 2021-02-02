const fs = require("fs");
const parse = require("xml-parser");
const https = require("https");
const axios = require("axios");

const unit_name = "Simulated";
const unit_nonce = 42;
const gpx_file = "gpx/20200930-173651.gpx";
// /const gpx_file = "gpx/test.gpx";
const report_interval = 5000;

const httpsAgent = new https.Agent({
    keepAlive: true,
    key: fs.readFileSync("simulated.p8"), // openssl pkcs8 -in ../../backend/pki/simulated.p8 -inform der -nocrypt -out simulated.p8
    cert: fs.readFileSync("simulated.crt"), // openssl x509 -in ../../backend/pki/simulated.crt.der -inform der -out simulated.crt
});

const ax = axios.create({ httpsAgent });

async function main() {
    let obj = parse(fs.readFileSync(gpx_file, "utf8"));
    if (obj.root.name !== "gpx") {
        console.error("Root element is not <gpx ...>");
        process.exit(1);
    }

    let trk = null;
    for (let c of obj.root.children) {
        if (c.name === "trk") {
            trk = c;
            break;
        }
    }
    if (!trk) {
        console.error("There is no <trk> within <gpx>");
        process.exit(1);
    }

    let name = null;
    let trkseg = null;
    for (let c of trk.children) {
        if (c.name === "name") {
            name = c.content;
        }
        else if (c.name == "trkseg") {
            trkseg = c;
        }
        if (name && trkseg) {
            break;
        }
    }
    if (!trkseg) {
        console.error("There is no <trkseg> within <trk>");
        process.exit(1);
    }


    let track = [];
    for (let c of trkseg.children) {
        if (c.name === "trkpt") {
            let trkpt = {
                lat: parseFloat(c.attributes.lat),
                lon: parseFloat(c.attributes.lon),
            };
            for (let cc of c.children) {
                if (cc.name === "ele") {
                    // trkpt.ele = parseFloat(cc.content); // NOTE: could parse it, but don't care
                }
                else if (cc.name === "time") {
                    trkpt.time = Math.round(new Date(cc.content).getTime() / 1000);
                }
                else if (cc.name === "extensions") {
                    for (let ccc of cc.children) {
                        if (ccc.name === "speed") {
                            trkpt.spd = parseFloat(ccc.content);
                        }
                    }
                }
            }
            track.push(trkpt);
        }
    }

    let N = track.length - 1; // reserve the last point so there always will be an (i+1)-th point too

    // normalize the timestamps
    let T = track[0].time;
    track.forEach(t => { t.time -= T });

    T = track[N].time; // the duration of the track
    if (name) {
        console.log("Processing '" + name + "', " + N + " steps in " + T + " sec");
    }
    // track.forEach(t => { console.log(JSON.stringify(t)); });

    let idx = 0; // index of the track point last passed
    while (true) {
        let sleep = new Promise(r => setTimeout(r, report_interval)); // let it run while we're doing our things

        // current time
        let now = Math.round(new Date().getTime() / 1000);
        // current time, folded down to track-time
        let tnow = Math.round(new Date().getTime() / 1000) % T;
        // find the appropriate track step, most probably the current one or the next, but don't take it granted (like at wrap-around)
        while (!( (track[idx].time <= tnow) && (tnow < track[idx + 1].time)) ) {
            idx = (idx + 1) % N;
        }
        // interpolate between the passed and the upcoming points
        let p = (tnow - track[idx].time) / (track[idx + 1].time - track[idx].time);
        let sim = {
            unit: unit_name,
            nonce: unit_nonce,
            time: now,
            lat: p * track[idx].lat + (1 - p) * track[idx + 1].lat,
            lon: p * track[idx].lon + (1 - p) * track[idx + 1].lon,
            spd: p * track[idx].spd + (1 - p) * track[idx + 1].spd,
            azi: 0,
            bat: 2987,
        };
        console.log(JSON.stringify(sim));
        ax.post("https://backend.wodeewa.com/v0/report", sim).then(result => {
            console.log("result=" + result.status + ", body='" + result.data + "'");
        }).catch(error => {
            console.error("error=" + error.response.status + ", body='" + error.response.data + "'");
        });

        // now wait for the remainder of that sleep
        await sleep;
    }
}


main()

// vim: set ts=4 sw=4 indk= et:
