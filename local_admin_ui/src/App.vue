<template>
    <v-app id="app">
        <v-app-bar color="primary" short dark>
            <v-icon>{{ mdiScooterElectric }}</v-icon>
            <v-toolbar-title class="ma-2">Configure GPS Unit</v-toolbar-title>
            <v-spacer/>

            <template v-slot:extension>
                <v-tabs v-model="tab" align-with-title>
                    <v-tabs-slider color="yellow"></v-tabs-slider>
                    <v-tab key="wifi">WiFi</v-tab>
                    <v-tab key="ssl">SSL</v-tab>
                    <v-tab key="ota">OTA</v-tab>
                    <v-tab key="data">Data</v-tab>
                    <v-tab key="reboot">Reboot</v-tab>
                </v-tabs>
            </template>
        </v-app-bar>

        <v-alert v-model="show_error" dismissible type="error"> {{ error_message }} </v-alert>
        <v-alert v-model="show_notification" dismissible type="success"> {{ notification_message }} </v-alert>

        <v-tabs-items v-model="tab" class="main">

            <v-tab-item key="wifi">
                <v-list>

                    <v-list-item>
                        <v-select
                            v-model="wifi.ssid"
                            :items="wifi.scanned_ssids"
                            label="Scanned SSID"
                            :append-outer-icon="mdiReload"
                            @click:append-outer="rescan_ssids"
                        />
                    </v-list-item>

                    <v-list-item>
                        <v-text-field
                            v-model="wifi.ssid"
                            label="SSID"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="update_ssid"
                        />
                    </v-list-item>

                    <v-list-item>
                        <v-text-field
                            v-model="wifi.password"
                            label="Password"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="update_password"
                        />
                    </v-list-item>

                </v-list>
            </v-tab-item>

            <v-tab-item key="ssl">
                <v-list>

                    <v-list-item>
                        <v-file-input show-size
                            v-model="ssl.pkey"
                            label="Unit Private Key"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="upload_pkey"
                        />
                    </v-list-item>

                    <v-list-item>
                        <v-file-input show-size
                            v-model="ssl.cert"
                            label="Unit Certificate"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="upload_cert"
                        />
                    </v-list-item>

                    <v-list-item>
                        <v-file-input show-size
                            v-model="ssl.cacert"
                            label="CA Certificate"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="upload_cacert"
                        />
                    </v-list-item>

                </v-list>
            </v-tab-item>

            <v-tab-item key="ota">
                <v-list>

                    <v-list-item>
                        <v-text-field
                            v-model="ota.url"
                            label="Server URL"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="update_ota_url"
                        />
                    </v-list-item>

                </v-list>
            </v-tab-item>

            <v-tab-item key="data">
                <v-list>

                    <v-list-item>
                        <v-text-field
                            v-model="server.url"
                            label="Data Server URL"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="update_data_url"
                        />
                    </v-list-item>

                    <v-list-item>
                        <v-text-field
                            type="number" min="1" max="3600"
                            v-model="server.time_threshold"
                            label="Time threshold (sec)"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="update_data_time_threshold"
                        />
                    </v-list-item>

                    <v-list-item>
                        <v-text-field
                            type="number" min="1" max="1000"
                            v-model="server.distance_threshold"
                            label="Distance threshold (m)"
                            :append-outer-icon="mdiUpload"
                            @click:append-outer="update_data_distance_threshold"
                        />
                    </v-list-item>

                </v-list>
            </v-tab-item>

            <v-tab-item key="reboot">
                <v-list>

                    <v-list-item>
                        <v-btn color="error" @click="reboot" rounded block>Reboot</v-btn>
                    </v-list-item>

                </v-list>
            </v-tab-item>

        </v-tabs-items>

    </v-app>
</template>

<script>

import { mdiUpload, mdiReload, mdiScooterElectric } from "@mdi/js";

export default {
    name: "app",
    data: () => ({
        mdiUpload, mdiReload, mdiScooterElectric,
        tab: null,
        show_error: false,
        error_message: null,
        show_notification: false,
        notification_message: null,
        wifi: {
            scanned_ssids: [ "alpha", "beta", "gamma", "delta" ],
            ssid: null,
            password: null,
        },
        ssl: {
            pkey: null,
            cert: null,
            cacert: null,
        },
        ota: {
            url: "https://ota.wodeewa.com/out/gps-unit.desc",
        },
        server: {
            url: "https://alpha.wodeewa.com/gps-reports",
            time_threshold: 30,
            distance_threshold: 100,
        },
    }),
    methods: {
        error: function(msg) {
            this.error_message = msg;
            this.show_error = true;
        },
        notification: function(msg) {
            this.notification_message = msg;
            this.show_notification = true;
        },
        reboot: function() {
            this.$http.post("/rest/reboot", {ssid: this.wifi.ssid}).then(() => {
                this.notification("Reboot initiated");
            }).catch(err => {
                this.error("Reboot failed: " + err);
            });
        },
        rescan_ssids: function() {
            this.wifi.scanned_ssids = [];
            this.$http.get("/rest/wifi/ssids").then(resp => {
                if (!resp || !resp.data) {
                    this.error("SSID list is empty");
                    return;
                }
                resp.data.forEach((ssid) => {
                    this.wifi.scanned_ssids.push(ssid);
                });
                this.notification("SSID list rescanned");
            }).catch(err => {
                this.error("SSID rescan failed: " + err);
            });
        },
        update_ssid: function() {
            this.$http.post("/rest/wifi/ssid", {ssid: this.wifi.ssid}).then(() => {
                this.notification("SSID updated");
            }).catch(err => {
                this.error("SSID update failed: " + err);
            });
        },
        update_password: function() {
            this.$http.post("/rest/wifi/password", {password: this.wifi.password}).then(() => {
                this.notification("Password updated");
            }).catch(err => {
                this.error("Password update failed: " + err);
            });
        },
        upload_pkey: function() {
            this.$http.post("/rest/ssl/pkey", this.ssl.pkey, { headers: {"Content-Type": "application/pkcs8"} } ).then(() => {
                this.notification("SSL pkey updated");
            }).catch(err => {
                this.error("SSL pkey update failed: " + err);
            });
        },
        upload_cert: function() {
            this.$http.post("/rest/ssl/cert", this.ssl.cert, { headers: {"Content-Type": "application/x-x509-user-cert"} } ).then(() => {
                this.notification("SSL cert updated");
            }).catch(err => {
                this.error("SSL cert update failed: " + err);
            });
        },
        upload_cacert: function() {
            this.$http.post("/rest/ssl/cacert", this.ssl.cacert, { headers: {"Content-Type": "application/x-x509-ca-cert"} } ).then(() => {
                this.notification("SSL CA cert updated");
            }).catch(err => {
                this.error("SSL CA cert update failed: " + err);
            });
        },
        update_ota_url: function() {
            this.$http.post("/rest/ota/url", {ota_url: this.ota.url}).then(() => {
                this.notification("OTA URL updated");
            }).catch(err => {
                this.error("OTA URL update failed: " + err);
            });
        },
        update_data_url: function() {
            this.$http.post("/rest/server/url", {data_url: this.server.url}).then(() => {
                this.notification("DataServer URL updated");
            }).catch(err => {
                this.error("DataServer URL update failed: " + err);
            });
        },
        update_data_time_threshold: function() {
            this.$http.post("/rest/server/time_threshold", {data_time_threshold: this.server.time_threshold}).then(() => {
                this.notification("DataServer time threshold updated");
            }).catch(err => {
                this.error("DataServer time threshold update failed: " + err);
            });
        },
        update_data_distance_threshold: function() {
            this.$http.post("/rest/server/distance_threshold", {data_distance_threshold: this.server.distance_threshold}).then(() => {
                this.notification("DataServer distance threshold updated");
            }).catch(err => {
                this.error("DataServer distance threshold update failed: " + err);
            });
        },
    },
    created: function () {
        this.$http.get("/rest/wifi/ssid").then((resp, err) => {
            if (resp && resp.ssid) {
                this.wifi.ssid = resp.ssid;
            }
        });
        this.$http.get("/rest/ota/url").then((resp, err) => {
            if (resp && resp.ota_url) {
                this.ota.url = resp.ota_url;
            }
        });
        this.$http.get("/rest/server/url").then((resp, err) => {
            if (resp && resp.data_url) {
                this.server.url = resp.data_url;
            }
        });
        this.$http.get("/rest/server/time_threshold").then((resp, err) => {
            if (resp && resp.time_trshld) {
                this.server.time_threshold = resp.time_trshld;
            }
        });
        this.$http.get("/rest/server/distance_threshold").then((resp, err) => {
            if (resp && resp.dist_trshld) {
                this.server.distance_threshold = resp.dist_trshld;
            }
        });
    },
};
</script>

<style lang="scss">
    :root {
        --app-bar-height: 48px;
        --main-height: calc(100vh - var(--app-bar-height));
    }

    #app {
        font-family: Helvetica, Arial, sans-serif;
        -webkit-font-smoothing: antialiased;
        -moz-osx-font-smoothing: grayscale;
        text-align: center;
        color: #2c3e50;
        height: 100vh;
    }
</style>

<style lang="scss" scoped>
@import "~vuetify/src/styles/main.sass";

    .v-app-bar {
        height: var(--app-bar-height);
        flex-grow: 0;
    }

    .main {
        flex-grow: 1;
        padding: 2rem 1rem;
    }

    .v-alert {
        margin: 1rem;
        padding: 1rem;
    }

</style>
// vim: set sw=4 ts=4 indk= et:
