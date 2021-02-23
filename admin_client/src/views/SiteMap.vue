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

            <l-control position="bottomright" >
                <v-btn fab color="primary" @click="showing_unit_track = !showing_unit_track" :outlined="!showing_unit_track"><v-icon>fas fa-route</v-icon></v-btn>
                <v-btn fab color="primary" @click="zoom_to_units"><v-icon>fas fa-crosshairs</v-icon></v-btn>
            </l-control>

            <v-marker-cluster>
            <template v-for="(st, id) in stations">
                <l-marker :lat-lng="st.loc" :key="id" @click="station_clicked(id)">
                    <l-clickable-tooltip @click="station_clicked(id)">
                        {{ st.name }}<br>{{ st.ready }}/{{ st.charging }}/{{ st.free }}
                    </l-clickable-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>

            <v-marker-cluster :options="{iconCreateFunction: function (c) { return unit_cluster_icon(c); } }">
            <template v-for="(u, name) in units">
                <l-marker v-if="u.loc" :lat-lng="u.loc" :key="name" :icon="(u == selected_unit) ? icon_selected_unit : icon_unselected_unit" @click="unit_clicked(name)" :zIndexOffset="1000">
                    <l-clickable-tooltip @click="unit_clicked(name)">
                        {{ name }}<br>{{ u.spdt }} kph
                    </l-clickable-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>

            <l-polyline v-if="showing_unit_track && selected_unit" :lat-lngs="this.selected_unit_track" className="trace-unit"/>
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
import { LMap, LControl, LControlScale, LTileLayer, LMarker, LPolyline } from "vue2-leaflet";
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

const icon_selected_unit = L.divIcon({
    html: '<img src="'+ require("../assets/selected-unit-icon.png") + '">',
    className: "icon-unit selected",
    iconSize: new L.Point(0, 0),
});

const icon_unselected_unit = L.divIcon({
    html: '<img src="'+ require("../assets/unit-icon.png") + '">',
    className: "icon-unit unselected",
    iconSize: new L.Point(0, 0),
});

export default {
    name: "SiteMap",
    components: {
        LMap,
        LControl,
        LControlScale,
        LTileLayer,
        LMarker,
        LClickableTooltip,
        LPolyline,
        "v-marker-cluster": Vue2LeafletMarkerCluster,
    },
    data () {
        return {
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
            currentCenter: latLng(25.1, 55.2),
            currentZoom: 11,
            icon_selected_unit,
            icon_unselected_unit,
            mapOptions: {
                zoomSnap: 0.5
            },
            centering_in_progress: true,
            center_timer: null,
            stations: [],
            units: {},

            showing_unit_details: false,
            selected_unit: null,
            showing_unit_track: false,
            selected_unit_track: [],
            showing_station_details: false,
            selected_station: null,
        };
    },
    computed: {
        cssVars() {
            // https://www.telerik.com/blogs/passing-variables-to-css-on-a-vue-component
            return {
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
                    newstations[st._id] = {
                        _id: st._id,
                        free: st.capacity - st.in_use,
                        ready: st.in_use,
                        charging: 0, // TODO: distinguish charging vs. ready
                        loc: latLng(st.lat, st.lon),
                    };
                }
                this.stations = newstations;
            });

            let tmpunits = {}
            this.$store.state.ax.get("/v0/unit/trace").then(response => {
                // response.status == 200
                for (var u of response.data) {
                    // {"_id":"600ee41459fa9c4248ea72cb","unit":"gps_unit_0","time":1611588627,"lat":25.0458,"lon":55.2457,"azi":0,"spd":0}
                    let loc = latLng(u.lat, u.lon);
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
                this.units = tmpunits;
                this.zoom_to_units();
            });

            // FIXME: fetch unit battery levels
        },

        zoom_to_units: function() {
            let bounds = latLngBounds([]);
            for (let u in this.units) {
                bounds.extend(this.units[u].loc);
            }
            if (bounds.length > 0) {
                this.map.fitBounds(bounds, {});
            }
        },

        unit_changed: function (u) {
            console.log("Updating unit " + JSON.stringify(u));
            if (!u.unit || !this.units[u.unit]) {
                return;
            }
            for (let field of ["spd", "azi", "location_time", "bat", "battery_time", "status", "user", "status_time"]) {
                if (u[field]) {
                    this.$set(this.units[u.unit], field, u[field]);
                }
            }
            if (u.spd) {
                this.$set(this.units[u.unit], "spdt", (u.spd * 3.6).toFixed(1));
            }
            if (u.lat && u.lon) {
                let loc = latLng(u.lat, u.lon);
                this.$set(this.units[u.unit], "loc", loc);
                if (this.selected_unit && (this.selected_unit.unit == u.unit) && this.showing_unit_track) {
                    this.selected_unit_track.shift(loc);
                    this.selected_unit_track.push(loc);
                }
            }
        },

        station_clicked: function (id) {
            this.selected_station = this.stations[id];
            this.showing_station_details = true;
        },
        unit_clicked: function (u) {
            if (this.selected_unit && (this.selected_unit.unit == u)) {
                return;
            }
            this.showing_unit_track = false;
            this.selected_unit = this.units[u];
            let new_track = [];
            this.$store.state.ax.get("/v0/unit/trace/" + encodeURIComponent(this.selected_unit.unit) + "?hours=24&num=100").then(response => {
                for (var u of response.data) {
                    //   { "_id": "601ed7c60aa99850a96e9a11", "unit": "Simulated", "time": 1612634054, "lat": 25.0869683, "lon": 55.2479817, "azi": 0, "spd": 15.127 }
                    new_track.unshift(latLng(u.lat, u.lon));
                }
            }).then(() => {
                this.selected_unit_track = new_track;
            });
            this.showing_unit_details = true;
        },
    },
    created: function() {
        console.log("SiteMap created, store.state.sign_in_ready=" + this.$store.state.sign_in_ready);
        // this._map.mapObject.ANY_LEAFLET_MAP_METHOD();
        if (!this.$store.getters.is_logged_in) {
            console.log("Not signed in, proceed to sign-in");
            this.$router.push("/signin");
        }
        EventBus.$on("signed-in", this.fetch_locations);
        if (this.$store.state.sign_in_ready) {
            this.fetch_locations();
        }
        EventBus.$on("unit", this.unit_changed);
        this.$store.state.app_bar_info = "Stations & Units";
    },
    beforeDestroy: function () {
        this.$store.state.app_bar_info = "..."
    },
}
</script>

<style lang="scss">
@import "~vuetify/src/styles/main.sass";

.unit-cluster {
    background-color: #fe4;
}

.unit-cluster div {
    background-color: #e10;
    color: #fff;
    font-size: 1.5em;
}

@keyframes icon-spin {
    from {
        transform: rotate3d(0, 1, 0, 0deg);
        opacity: 1;
    }
    to {
        transform: rotate3d(0, 1, 0, 360deg);
        opacity: 1;
    }
}

@keyframes trace-crawl {
    from {
        stroke-dashoffset: 15;
    }
    to {
        stroke-dashoffset: 0;
    }
}

.icon-unit img {
    animation: 5s infinite linear icon-spin;
    position: relative;
    left: -13px;
    top: -36px;
}

.icon-unit.selected img {
    animation: 1.5s infinite linear icon-spin;
}

.trace-unit {
    stroke: #e04;
    stroke-width: 6;
    stroke-dasharray: 1,15;
    animation: 2s infinite linear trace-crawl;
}

</style>

// vim: set sw=4 ts=4 indk= et:
