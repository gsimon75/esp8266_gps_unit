
export default class JsSSE {
    constructor({url, options, on_connected, on_message, on_closed, on_error}) {
        this.url = url;
        this.options = options;
        this.on_connected = on_connected;
        this.on_message = on_message;
        this.on_closed = on_closed;
        this.on_error = on_error;

        this.keep_connecting = true;
        this.keep_reading = true;
    }


    async start() {
        let u8dec = new TextDecoder();
        let linebuf = "";
        this.keep_connecting = true;
        while (this.keep_connecting) {
            try {
                let response = await fetch(this.url, this.options);
                this.keep_reading = this.on_connected ? await this.on_connected(response) : true;
                if (!response.ok) {
                    this.keep_connecting = this.keep_reading;
                    console.log("response not ok");
                }
                else if (this.keep_reading) {
                    var reader = response.body.getReader();

                    while (this.keep_reading) {
                        let chunk = await reader.read();

                        if (chunk.done) {
                            console.log("fetch done"); // tcp connection closed
                            break;
                        }

                        linebuf += u8dec.decode(chunk.value);
                        while (this.keep_reading) {
                            let eolpos = linebuf.indexOf("\n");
                            if (eolpos < 0) {
                                break;
                            }
                            let msg = JSON.parse(linebuf.slice(0, eolpos));
                            linebuf = linebuf.slice(eolpos + 1);
                            if (this.on_message) {
                                this.keep_reading = await this.on_message(msg);
                            }
                        }
                    }
                    if (this.on_closed) {
                        this.keep_connecting = await this.on_closed();
                    }
                }
            }
            catch (err) {
                if ((err instanceof TypeError) && (err.message == "network error")) {
                    // Failed to load resource: net::ERR_INCOMPLETE_CHUNKED_ENCODING
                    // GET https://... net::ERR_INCOMPLETE_CHUNKED_ENCODING 200 (OK)
                    console.log("fetch timeout, reconnecting=" + this.keep_connecting);
                }
                else if (this.on_error) {
                    this.keep_connecting = await this.on_error(err);
                }
                else {
                    throw err;
                }
            }
        }
    }

}
// vim: set sw=4 ts=4 indk= et:
