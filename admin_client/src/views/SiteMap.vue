<template>
    <div class="site-map fill-height" :style="cssVars">
        <l-map
            ref="site_map" @ready="map_is_ready"
            :zoom="currentZoom"
            :center="currentCenter"
            :options="mapOptions"
            :noBlockingAnimations="true"
            style="height: 100%"
            @update:center="centerUpdate"
            @update:zoom="zoomUpdate"
            @dragstart="user_drag"
        >
            <l-control-scale position="topright" :imperial="false" :metric="true"/>
            <l-tile-layer :url="tile_url" :options="tileLayerOptions"/>

            <v-marker-cluster>
            <template v-for="(st, name) in stations">
                <l-marker :lat-lng="st.loc" :key="st.id" @click="station_clicked(name)">
                    <l-clickable-tooltip @click="station_clicked(name)">
                        {{ name }}<br>{{ st.ready }}/{{ st.charging }}/{{ st.free }}
                    </l-clickable-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>

            <v-marker-cluster :options="{iconCreateFunction: function (c) { return unit_cluster_icon(c); } }">
            <template v-for="(u, name) in units">
                <l-marker v-if="u.loc" :lat-lng="u.loc" :key="name" :icon="icon_unit" @click="unit_clicked(name)">
                    <l-clickable-tooltip @click="unit_clicked(name)">
                        {{ name }}<br>{{ u.spdt }} kph
                    </l-clickable-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>
        </l-map>

        <!--
        Dialog for station details
        -->
        <v-dialog v-model="showing_station_details" max-width="90vw" modal>
            <v-card v-if="!!selected_station">
                <v-card-title class="text-h4">
                    {{ selected_station.name }}
                </v-card-title>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Location</v-list-item-title>
                        <v-list-item-subtitle>{{ loc2str(selected_station.loc) }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Capacity</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_station.capacity }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Available units</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_station.ready }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Charging units</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_station.charging }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Free space</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_station.free }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item>
                    <v-list-item-content justify="center">
                        <v-btn color="error" @click="showing_station_details = false;" rounded>Close</v-btn>
                    </v-list-item-content>
                </v-list-item>

            </v-card>
        </v-dialog>

        <!--
        Dialog for unit details
        -->
        <v-dialog v-model="showing_unit_details" max-width="90vw" modal>
            <v-card v-if="!!selected_unit">
                <v-card-title class="text-h4">
                    {{ selected_unit.unit }}
                </v-card-title>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Location</v-list-item-title>
                        <v-list-item-subtitle>{{ loc2str(selected_unit.loc) }}</v-list-item-subtitle>
                        <v-list-item-subtitle>at {{ new Date(1000 * selected_unit.location_time).toLocaleString() }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Speed</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_unit.spdt }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line>
                    <v-list-item-content>
                        <v-list-item-title>Battery</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_unit.bat }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>

                <v-list-item two-line v-if="selected_unit.status == 'in_use'">
                    <v-list-item-content>
                        <v-list-item-title>In use by</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_unit.user }}</v-list-item-subtitle>
                        <v-list-item-subtitle>at {{ new Date(1000 * selected_unit.status_time).toLocaleString() }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>
                <v-list-item two-line v-else>
                    <v-list-item-content>
                        <v-list-item-title>Status</v-list-item-title>
                        <v-list-item-subtitle>{{ selected_unit.status }}</v-list-item-subtitle>
                        <v-list-item-subtitle>at {{ new Date(1000 * selected_unit.status_time).toLocaleString() }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>


                <v-list-item>
                    <v-list-item-content justify="center">
                        <v-btn color="error" @click="showing_unit_details = false;" rounded>Close</v-btn>
                    </v-list-item-content>
                </v-list-item>

            </v-card>
        </v-dialog>

    </div>
</template>

<script>
// @ is an alias to /src
import { EventBus } from "@/modules/event-bus";
import L from "leaflet";
import { LMap, LControlScale, LTileLayer, LMarker } from "vue2-leaflet";
import Vue2LeafletMarkerCluster from "vue2-leaflet-markercluster";
import LClickableTooltip from "@/components/LClickableTooltip";

/* NOTE: when navigating away from this view, exceptions will be thrown:
   leaflet-src.js:2449 Uncaught TypeError: Cannot read property '_leaflet_pos' of undefined
    at getPosition (leaflet-src.js:2449)
    at NewClass._getMapPanePos (leaflet-src.js:4438)
    at NewClass._moved (leaflet-src.js:4442)
    at NewClass.getCenter (leaflet-src.js:3797)
    at VueComponent.moveEndHandler (LMap.js:439)
    at eval (LMap.js:18)

   The issue has already been dealt with: https://github.com/vue-leaflet/Vue2Leaflet/issues/613
   */

import { Icon, latLng, latLngBounds } from "leaflet";

delete Icon.Default.prototype._getIconUrl;
Icon.Default.mergeOptions({
    iconUrl: require("leaflet/dist/images/marker-icon.png"),
    shadowUrl: require("leaflet/dist/images/marker-shadow.png"),
    iconRetinaUrl: require("leaflet/dist/images/marker-icon-2x.png"),
});

const icon_biking = L.icon({
    iconUrl: require("../assets/marker-biking.png"),
    shadowUrl: require("../assets/marker-biking-shadow.png"),
    iconRetinaUrl: require("../assets/marker-biking-2x.png"),
    iconSize: [ 52, 41 ],
    shadowSize: [ 52, 41 ],
    iconAnchor: [ 26, 41 ],
});

const icon_unit = L.icon({
    iconUrl: require("../assets/unit-icon.png"),
    shadowUrl: require("../assets/unit-shadow.png"),
    iconRetinaUrl: require("../assets/unit-icon-2x.png"),
    iconSize: [ 25, 41 ],
    shadowSize: [ 25, 41 ],
    iconAnchor: [ 13, 41 ],
    className: "icon-unit",
});

export default {
    name: "SiteMap",
    components: {
        LMap,
        LControlScale,
        LTileLayer,
        LMarker,
        LClickableTooltip,
        "v-marker-cluster": Vue2LeafletMarkerCluster,
    },
    data () {
        return {
            //tile_url: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
            //tile_attribution: "&copy; <a href=\"http://osm.org/copyright\">OpenStreetMap</a> contributors",
            tile_url: "https://api.mapbox.com/styles/v1/{id}/tiles/{z}/{x}/{y}?access_token={accessToken}",
            tileLayerOptions: {
                attribution: 'Map data &copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors, Imagery © <a href="https://www.mapbox.com/">Mapbox</a>',
                maxZoom: 23,
                zoomOffset: -1,
                id: "mapbox/streets-v11",
                tileSize: 512,
                accessToken: "pk.eyJ1IjoiZ3NpbW9uNzUiLCJhIjoiY2trc2xuczl3MGVwNzJ2cGx0bDdlZnRoNCJ9.ZGCgtuQhE9AaNvxHaAYD6w",
            },
            map: false,
            currentCenter: latLng(25.0, 55.0),
            currentZoom: 17,
            icon_biking,
            icon_unit,
            icon_unit_bounce_timer: null,
            icon_unit_delta_y: 0,
            mapOptions: {
                zoomSnap: 0.5
            },
            centering_in_progress: true,
            center_timer: null,
            stations: [],
            units: {},

            showing_unit_details: false,
            selected_unit: null,
            showing_station_details: false,
            selected_station: null,
        };
    },
    computed: {
        cssVars() {
            // https://www.telerik.com/blogs/passing-variables-to-css-on-a-vue-component
            return {
                "--icon-unit-y": (this.icon_unit_delta_y - 46) + "px",
                "--cluster-unit-y": (this.icon_unit_delta_y - 20) + "px",
            };
        },
    },
    methods: {
        map_is_ready: function () {
            console.log("Map is ready");
            this.map = this.$refs.site_map.mapObject;
        },
        zoomUpdate: function (zoom) {
            this.currentZoom = zoom;
        },
        centerUpdate: function (center) {
            this.currentCenter = center;
        },
        user_drag: function() {
        },
        style_extractor: function (feature) {
            return feature.properties;
        },
        unit_cluster_icon: function (cluster) {
            var childCount = cluster.getChildCount();
            return new L.DivIcon({ html: "<div><span>" + childCount + "</span></div>", className: "marker-cluster unit-cluster", iconSize: new L.Point(40, 40) });
        },
        loc2str: function (loc) {
            return "(" + loc.lat.toFixed(4) + ", " + loc.lng.toFixed(4) + ")";
        },
        fetch_locations: function () {
            console.log("Fetching location data");
            this.$store.state.ax.get("/v0/station").then(response => {
                // response.status == 200
                var newstations = {}
                for (var st of response.data) {
                    // {"_id":"6016822fcbe5bf1db53ae6c2","id":3825891566,"lat":25.1850197,"lon":55.2652917,"name":"The Health Spot Cafe","capacity":14,"in_use":0}
                    delete st._id;
                    st.free = st.capacity - st.in_use;
                    st.ready = st.in_use;
                    st.charging = 0; // TODO: distinguish charging vs. ready
                    st.loc = latLng(st.lat, st.lon);
                    newstations[st.name] = st;
                }
                this.stations = newstations;
            });

            let tmpunits = {}
            let bounds = latLngBounds([]);
            this.$store.state.ax.get("/v0/unit/trace").then(response => {
                // response.status == 200
                for (var u of response.data) {
                    // {"_id":"600ee41459fa9c4248ea72cb","unit":"gps_unit_0","time":1611588627,"lat":25.0458,"lon":55.2457,"azi":0,"spd":0}
                    let loc = latLng(u.lat, u.lon);
                    bounds.extend(loc);
                    tmpunits[u.unit] = {
                        unit: u.unit,
                        location_time: u.time,
                        loc,
                        azi: u.azi,
                        spd: u.spd,
                        spdt: (u.spd * 3.6).toFixed(1),
                    };
                }
                return this.$store.state.ax.get("/v0/unit/status");
            }).then(response => {
                // response.status == 200
                for (var u of response.data) {
                    // {"_id":"6016e31786ae8c98ba825c2d","unit":"Simulated","status":"in_use","user":"gabor.simon75@gmail.com","time":1612118833}
                    if (u.unit in tmpunits) {
                        tmpunits[u.unit].status_time = u.time;
                        tmpunits[u.unit].status = u.status;
                        tmpunits[u.unit].user = u.user;
                    }
                }
                // FIXME: merge battery and startup info in a similar manner
            }).then(() => {
                console.log("units=" + JSON.stringify(tmpunits));
                this.map.fitBounds(bounds, {});
                this.units = tmpunits;
            });

            // FIXME: fetch unit battery levels
        },

        unit_location_changed: function (u) {
            console.log("Updating unit location " + JSON.stringify(u));
            this.$set(this.units, u.unit, {
                ...this.units[u.unit],
                location_time: u.time,
                loc: latLng(u.lat, u.lon),
                spd: u.spd,
                spdt: (u.spd * 3.6).toFixed(1),
                azi: u.azi,
            });
        },
        unit_battery_changed: function (u) {
            console.log("Updating unit battery " + JSON.stringify(u));
            this.$set(this.units, u.unit, {
                ...this.units[u.unit],
                battery_time: u.time,
                bat: u.bat,
            });
        },
        unit_status_changed: function (u) {
            console.log("Updating unit status " + JSON.stringify(u));
            this.$set(this.units, u.unit, {
                ...this.units[u.unit],
                status_time: u.time,
                status: u.status,
                user: u.user,
            });
        },

        station_clicked: function (st) {
            this.selected_station = this.stations[st];
            this.showing_station_details = true;
        },
        unit_clicked: function (u) {
            this.selected_unit = this.units[u];
            this.showing_unit_details = true;
        },
    },
    created: function() {
        console.log("SiteMap created");
        // this.$refs.site_map.mapObject.ANY_LEAFLET_MAP_METHOD();
        if (!this.$store.getters.is_logged_in) {
            console.log("Not signed in, proceed to sign-in");
            this.$router.push("/signin");
        }
        EventBus.$on("signed-in", this.fetch_locations);
        if (this.$store.state.auth.sign_in_ready) {
            this.fetch_locations();
        }
        EventBus.$on("unit_location", this.unit_location_changed);
        EventBus.$on("unit_battery", this.unit_battery_changed);
        EventBus.$on("unit_status", this.unit_status_changed);
        this.$store.state.app_bar_info = "Stations & Units";

        let delta = 0;
        this.icon_unit_bounce_timer = setInterval(() => {
            delta = (delta + 1) % 15;
            this.icon_unit_delta_y = -1 * delta;
        }, 100);
    },
    beforeDestroy: function () {
        this.$store.state.app_bar_info = "..."
        clearInterval(this.icon_unit_bounce_timer);
    },
}
</script>

<style lang="scss">
@import "~vuetify/src/styles/main.sass";

.unit-cluster {
    background-color: #fe4;
    margin-top: var(--cluster-unit-y) !important;
}

.unit-cluster div {
    background-color: #e10;
    color: #fff;
    font-size: 1.5em;
}

.icon-unit {
    margin-top: var(--icon-unit-y) !important;
}

</style>

// vim: set sw=4 ts=4 indk= et:
