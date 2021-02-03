<template>
    <div class="site-map fill-height">
        <l-map
            ref="site_map" @ready="map_is_ready"
            :zoom="currentZoom"
            :center="currentCenter"
            :options="mapOptions"
            :noBlockingAnimations="false"
            style="height: 100%"
            @update:center="centerUpdate"
            @update:zoom="zoomUpdate"
            @dragstart="user_drag"
        >
            <l-control-scale position="topright" :imperial="false" :metric="true"/>
            <l-tile-layer :url="tile_url" :attribution="tile_attribution"/>

            <v-marker-cluster>
            <template v-for="st in stations">
                <l-marker :lat-lng="st.loc" :key="st.id">
                    <l-tooltip :options="{ permanent: true }">
                    {{ st.name }}
                    </l-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>

            <v-marker-cluster :options="{iconCreateFunction: function (c) { return unit_cluster_icon(c); } }">
            <template v-for="(u, name) in units">
                <l-marker v-if="u.loc" :lat-lng="u.loc" :key="name" :icon="icon_unit">
                    <l-tooltip :options="{ permanent: true }">
                    {{ name }}<br>{{ u.spdt }} kph
                    </l-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>
        </l-map>

    </div>
</template>

<script>
// @ is an alias to /src
import { EventBus } from "../modules/event-bus";
import L from "leaflet";
import { LMap, LControlScale, LTileLayer, LMarker, LTooltip } from "vue2-leaflet";
import Vue2LeafletMarkerCluster from "vue2-leaflet-markercluster";

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

import { Icon, latLng } from "leaflet";

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
});

export default {
    name: "SiteMap",
    components: {
        LMap,
        LControlScale,
        LTileLayer,
        LMarker,
        LTooltip,
        "v-marker-cluster": Vue2LeafletMarkerCluster,
    },
    data () {
        return {
            tile_url: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
            tile_attribution: "&copy; <a href=\"http://osm.org/copyright\">OpenStreetMap</a> contributors",
            map: false,
            currentCenter: latLng(25.0, 55.0),
            currentZoom: 17,
            icon_biking,
            icon_unit,
            mapOptions: {
                zoomSnap: 0.5
            },
            centering_in_progress: true,
            center_timer: null,
            stations: [],
            units: {},
        };
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
        fetch_locations: function () {
            this.$store.state.ax.get("/v0/station").then(response => {
                // response.status == 200
                var newstations = [];
                for (var st of response.data) {
                    // {"_id":"6016822fcbe5bf1db53ae6c2","id":3825891566,"lat":25.1850197,"lon":55.2652917,"name":"The Health Spot Cafe","capacity":14,"in_use":0}
                    delete st._id;
                    st.free = st.capacity - st.in_use;
                    st.ready = st.in_use;
                    st.charging = 0; // TODO: distinguish charging vs. ready
                    st.loc = latLng(st.lat, st.lon);
                    newstations.push(st);
                }
                this.stations = newstations;
            });
            var tmpunits = {}
            this.$store.state.ax.get("/v0/unit/trace").then(response => {
                // response.status == 200
                for (var u of response.data) {
                    // {"_id":"600ee41459fa9c4248ea72cb","unit":"gps_unit_0","time":1611588627,"lat":25.0458,"lon":55.2457,"azi":0,"spd":0}
                    tmpunits[u.unit] = {
                        unit: u.unit,
                        location_time: u.time,
                        loc: latLng(u.lat, u.lon),
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
            });
        },
        unit_location_changed: function (u) {
            console.log("Updating unit " + JSON.stringify(u));
            this.$set(this.units, u.unit, {
                ...this.units[u.unit],
                location_time: u.time,
                loc: latLng(u.lat, u.lon),
                spd: u.spd,
                spdt: (u.spd * 3.6).toFixed(1),
                azi: u.azi,
                bat: u.bat,
            });
        }
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

</style>

// vim: set sw=4 ts=4 indk= et:
