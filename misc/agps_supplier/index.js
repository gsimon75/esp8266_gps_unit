const fs = require("fs");
const SerialPort = require("serialport");
const https = require("https");
const axios = require("axios");
const sprintf = require('sprintf-js').sprintf;


const UBX_MAGIC = new Uint8Array([0xb5, 0x62]);

let inbuf = Buffer.alloc(65536);
let wr = 0;

const httpsAgent = new https.Agent({
    keepAlive: true,
    key: fs.readFileSync(process.env.GPS_PKEY || "simulated.p8"), // openssl pkcs8 -in ../../backend/pki/simulated.p8 -inform der -nocrypt -out simulated.p8
    cert: fs.readFileSync(process.env.GPS_CERT || "simulated.crt"), // openssl x509 -in ../../backend/pki/simulated.crt.der -inform der -out simulated.crt
});

const ax = axios.create({ httpsAgent });

/**********************************************************************************************************************
 * AGPS message handling
 *
 * We're maintaining a 2-level cache (type=(ALM|EPH), svid=(1..32)) for the raw * messages. If a new or different data
 * is available for some satellite, we're sending it right away to the backend server.  * (It'll send these down to the
 * units when they start up.)
 */

let agps_cache = {
};

function got_agps(type, svid, message) {
    if (!agps_cache[type]) {
        agps_cache[type] = { };
    }
    if (agps_cache[type][svid] && (agps_cache[type][svid].compare(message) == 0)) {
        return;
    }
    
    agps_cache[type][svid] = Buffer.from(message);
    console.log("New " + type + " for svid=" + svid + ":");
    //hexdump(message);

    ax.post("https://backend.wodeewa.com/v0/agps", { type, svid, message }).catch(error => {
        console.error("AGPS data error=" + error.response.status + ", body='" + error.response.data + "'");
        if (500 <= error.response.status) {
            // retry on next message
            delete agps_cache[type][svid];
        }
    });
}


/**********************************************************************************************************************
 * Dump some buffer
 */

function hexdump(data) {
    let offs = 0;
    for (let len = data.length; len > 0; len -= 0x20) {
        let p = sprintf("%04x:", offs);
        let i;
        for (i = 0; (i < len) && (i < 0x20); ++i) {
            p += sprintf(" %02x", data[offs + i]);
        }
        for (; i < 0x20; ++i) {
            p += "   ";
        }
        p += " ";
        for (i = 0; (i < len) && (i < 0x20); ++i) {
            let c = data[offs + i];
            p += ((0x20 <= c) && (c < 0x80)) ? String.fromCharCode(c) : ".";
        }
        console.log(p);
        offs += 0x20;
    }
}


/**********************************************************************************************************************
 * Got an NMEA message
 *
 * NOTE: We don't need their content, so no further parsing is done
 */

function got_nmea(buffer) {
    // buffer: the nmea message without the leading "$" and the trailing "\r\n"
    let star_pos = buffer.length - 3;
    if (buffer[star_pos] != 0x2a) { // not "*" -> checksum format invalid
        return false;
    }
    let shouldbe = parseInt(buffer.toString("ascii", star_pos + 1), 16);
    if (isNaN(shouldbe)) { // checksum wasn't a hex number
        return false;
    }

    let chksum = 0;
    for (let i = 0; i < star_pos; i++) {
        chksum ^= buffer[i];
    }
    if (chksum != shouldbe) { // checksum mismatch
        console.warn(sprintf("NMEA checksum error: is=%02x, shouldbe=%02x", chksum, shouldbe));
        return false;
    }

    console.log("NMEA: '" + buffer.toString("ascii", 0, star_pos) + "'");
    return true;
}


/**********************************************************************************************************************
 * Got an UBX-AID-* message
 */

function got_AID(buffer) {
    // buffer: a complete and checked UBX message with class = 0x0b = AID-*
    let m_id = buffer[3];
    let payload = buffer.subarray(6, buffer.length - 2);

    if (m_id == 0x30) { // AID-ALM
        let svid = payload[0];
        console.log("AID-ALM: svid=" + svid + ", payload_len=0x" + payload.length.toString(16));
        if (payload.length == 0x28) {
            got_agps("ALM", svid, buffer);
        }
        return true;
    }
    if (m_id == 0x31) { // AID-EPH
        let svid = payload[0];
        console.log("AID-EPH: svid=" + svid + ", payload_len=0x" + payload.length.toString(16));
        if (payload.length == 0x68) {
            got_agps("EPH", svid, buffer);
        }
        return true;
    }
    return false;
}


/**********************************************************************************************************************
 * Got an UBX-*-* message
 */

function got_ubx(buffer) {
    // buffer: a complete ubx message, including the magic, up to the appropriate length
    let ck_a = 0;
    let ck_b = 0;

    for (let i = 2; i < (buffer.length - 2); ++i) {
           ck_a = (ck_a + buffer[i]) & 0xff;
           ck_b = (ck_b + ck_a) & 0xff;
    }
    if ((buffer[buffer.length - 2] != ck_a) || (buffer[buffer.length - 1] != ck_b)) {
        console.warn(sprintf("UBX checksum error: is=[%02x, %02x], shouldbe=[%02x, %02x]", buffer[buffer.length - 2], buffer[buffer.length - 1], ck_a, ck_b));
        return false;
    }

    let m_class = buffer[2];
    let m_id = buffer[3];

    if (m_class == 0x0b) { // AID-*
        if (got_AID(buffer)) {
            return true;
        }
    }
    console.log(sprintf("UBX: class=0x%02x, id=0x%02x, payload_len=0x%04x", m_class, m_id,  buffer.length - 8));
    //hexdump(buffer.subarray(6, buffer.length - 2));
    return true;
}


/**********************************************************************************************************************
 * Construct an UBX-*-* message
 *
 * Used for creating the initializing commands
 */

function ubx_message(m_class, m_id, payload) {
    let buffer = Buffer.alloc(8 + payload.length);
    buffer[0] = UBX_MAGIC[0];
    buffer[1] = UBX_MAGIC[1];
    buffer[2] = m_class;
    buffer[3] = m_id;
    buffer.writeUInt16LE(payload.length, 4);
    payload.copy(buffer, 6);

    let ck_a = 0;
    let ck_b = 0;
    for (let i = 2; i < (buffer.length - 2); ++i) {
           ck_a = (ck_a + buffer[i]) & 0xff;
           ck_b = (ck_b + ck_a) & 0xff;
    }
    buffer[buffer.length - 2] = ck_a;
    buffer[buffer.length - 1] = ck_b;
    return buffer;
}


/**********************************************************************************************************************
 * Init command messages
 *
 * Hot restart, disable all defaults but GPGSA, enable periodic UBX-AID-DATA (it'll dump the UBX-AID-ALM and
 * UBX-AID-EPH settings collected so far).
 */

const init_msgs = [
    ubx_message(0x06, 0x04, Buffer.from([0x00, 0x00, 0x04, 0x00])), // CFG-RST: hotstart
    ubx_message(0x06, 0x01, Buffer.from([0xf0, 0x04, 0x00])), // disable NMEA-RMC
    ubx_message(0x06, 0x01, Buffer.from([0xf0, 0x00, 0x00])), // disable NMEA-GGA
    ubx_message(0x06, 0x01, Buffer.from([0xf0, 0x01, 0x00])), // disable NMEA-GLL
    //ubx_message(0x06, 0x01, Buffer.from([0xf0, 0x02, 0x00])), // disable NMEA-GSA
    ubx_message(0x06, 0x01, Buffer.from([0xf0, 0x03, 0x00])), // disable NMEA-GSV
    ubx_message(0x06, 0x01, Buffer.from([0xf0, 0x05, 0x00])), // disable NMEA-VTG
    //ubx_message(0x06, 0x01, Buffer.from([0x01, 0x02, 0x01])), // enable NAV-POSLLH
    //ubx_message(0x06, 0x01, Buffer.from([0x01, 0x30, 0x01])), // enable NAV-SVINFO
    //ubx_message(0x0b, 0x10, Buffer.from([])), // AID-DATA: poll all aiding data
    ubx_message(0x06, 0x01, Buffer.from([0x0b, 0x10, 0x20])), // enable AID-DATA on every 0x20-th fix
];
var next_init_msg = 0;


/**********************************************************************************************************************
 * Watchdog timer
 *
 * If there is no activity for a certain period, restart initialization
 */

var watchdog_timer = null;

function watchdog_expired() {
    console.error("Watchdog timer expired");
    port.close(() => { port.open(); });

}

function restart_watchdog() {
    if (watchdog_timer != null) {
        clearTimeout(watchdog_timer);
    }
    watchdog_timer = setTimeout(watchdog_expired, 5000);
}


/**********************************************************************************************************************
 * Open the serial port
 *
 * NOTE: autoOpen=false, because we need to register the event handlers first
 */

const port = new SerialPort(process.env.GPS_PORT || "/dev/ttyUSB0", {
    autoOpen: false,
    baudRate: parseInt(process.env.GPS_BAUD || "9600"),
});


/**********************************************************************************************************************
 * Send a message, and if it's sent, wait some delay and send the next one
 */

function send_init_msg() {
    if (next_init_msg < init_msgs.length) {
        console.log("Writing init cmd " + next_init_msg + " :");
        got_ubx(init_msgs[next_init_msg]);
        if (!port.write(init_msgs[next_init_msg])) {
            console.warn("Failed to write init cmd");
            return; // the "drain" event will call us again
        }
        next_init_msg++;
        port.drain(() => {
            setTimeout(send_init_msg, 800);
            restart_watchdog();
        });
    }
}


/**********************************************************************************************************************
 * When the port opens, start initializing it
 */

port.on("open", () => { 
    console.log("event=open");
    next_init_msg = 0;
    send_init_msg();
});


/**********************************************************************************************************************
 * Incoming data handler
 *
 * Collect the chunks in `inbuf`, try to parse NMEA and UBX messages from its start, drop the junk (if any) and
 * preserve unfinished messages to be completed by the upcoming chunks.
 */

port.on("data", chunk => {
    //console.log("Got data " + chunk.length + " bytes");
    chunk.copy(inbuf, wr);
    wr += chunk.length;

    let buf = inbuf.subarray(0, wr); // need to limit its length for buf.indexOf(...)

    let rd = 0;
    while (rd < wr) {

        if (buf[rd + 0] == 0x24) { // "$": nmea message at the beginning (may be incomplete/invalid)
            let pos_crlf = buf.indexOf("\r\n", rd);
            if (pos_crlf < 0) { // incomplete
                break;
            }
            // complete
            if (got_nmea(buf.subarray(rd + 1, pos_crlf))) {
                restart_watchdog();
            }
            rd = pos_crlf + 2;
            continue;
        }
        
        if (buf[rd + 0] == UBX_MAGIC[0]) { // ubx message at the beginning (may be incomplete/invalid)
            if ((rd + 1) == wr) { // no following byte yet -> incomplete
                break;
            }
            if (buf[rd + 1] == UBX_MAGIC[1]) { // still may be ubx
                if (wr <= (rd + 5)) { // too short to contain a length field -> incomplete
                    break;
                }
                let m_len = buf.readUInt16LE(rd + 4);
                if ((rd + 8 + m_len) <= wr) { // complete
                    if (got_ubx(buf.subarray(rd, rd + m_len + 8))) {
                        restart_watchdog();
                    }
                    rd += 8 + m_len;
                    continue;
                }
                // incomplete
                break;
            }
        }

        // junk at the beginning -> try to find the beginning of a recognized message
        let pos_nmea = buf.indexOf("$", rd);
        let pos_ubx = buf.indexOf(UBX_MAGIC, rd);

        if (pos_nmea < 0) {
            rd = pos_ubx;
        }
        else if (pos_ubx < 0) {
            rd = pos_nmea;
        }
        else if (pos_ubx < pos_nmea) {
            rd = pos_ubx;
        }
        else {
            rd = pos_nmea;
        }

        if (rd < 0) { // all junk, skip it
            rd = wr;
            break;
        }

    } // while (rd < wr)

    if (rd < wr) { // there is an incomplete message from rd
        if (rd != 0) {
            inbuf.copy(inbuf, 0, rd, wr);
            wr -= rd;
        }
    }
    else { // all processed, next write may go to the start of inbuf
        wr = 0;
    }
});


/**********************************************************************************************************************
 * Other handlers, just in case
 */

port.on("drain", () => { console.log("event=drain");});
port.on("error", (err) => { console.log("event=error: " + JSON.stringify(err));});
port.on("close", () => { console.log("event=close");});


/**********************************************************************************************************************
 * And now let it roll :D !
 */

port.open();
