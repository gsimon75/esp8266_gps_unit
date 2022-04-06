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
            <l-control position="bottomleft" >
                <v-btn small color="primary" @click="nearest_station('take')">Nearest<br>to take</v-btn>
                <v-btn small color="error" @click="nearest_station('return')">Nearest<br>to return</v-btn>
            </l-control>
            <l-control position="bottomright" >
                <v-btn fab color="primary" @click="toggle_auto_center" :outlined="!auto_center"><v-icon>fas fa-crosshairs</v-icon></v-btn>
            </l-control>

            <v-marker-cluster>
            <template v-for="(st, id) in $store.state.data.stations">
                <l-marker :lat-lng="st.loc" :key="id" @click="station_clicked(st)">
                    <l-clickable-tooltip @click="station_clicked(st)">
                        {{ st.name }}<br>{{ st.ready }}/{{ st.charging }}/{{ st.free }}
                    </l-clickable-tooltip>
                    <!--l-icon :icon-size="[40, 36]">
                        <v-badge color="blue" :content="st.id" left>
                        <v-badge :value="st.ready" color="green" :content="st.ready" overlap>
                        <v-badge :value="st.free" color="red" :content="st.free" bottom overlap>
                            <v-icon large color="black">
                                fas fa-charging-station
                            </v-icon>
                        </v-badge>
                        </v-badge>
                        </v-badge>
                    </l-icon-->
                </l-marker>
            </template>
            </v-marker-cluster>

            <v-marker-cluster :options="{iconCreateFunction: function (c) { return unit_cluster_icon(c); } }">
            <template v-for="u in $store.getters['data/my_units']">
                <l-marker v-if="u.loc" :lat-lng="u.loc" :key="u.unit" :icon="icon_biking" @click="unit_clicked(u)" :zIndexOffset="1000">
                    <l-clickable-tooltip @click="unit_clicked(u)" :permanent="false">
                        {{ u.unit }}<br>{{ u.spdt }} kph
                    </l-clickable-tooltip>
                </l-marker>
            </template>
            </v-marker-cluster>

            <l-marker ref="current_pos" :icon="icon_selected_unit" :lat-lng="$store.state.current_location" :zIndexOffset="1001"/>

        </l-map>

    </div>
</template>

<script>
// @ is an alias to /src
import { EventBus } from "@/modules/event-bus";
import L from "leaflet";
import { latLng, latLngBounds } from "leaflet";
import { LMap, LControl, LControlScale, LTileLayer, LMarker } from "vue2-leaflet";
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

import { Icon } from "leaflet";

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

const icon_biking = L.icon({
    iconUrl: require("../assets/marker-biking.png"),
    //shadowUrl: require("../assets/marker-biking-shadow.png"),
    iconRetinaUrl: require("../assets/marker-biking-2x.png"),
    iconSize: [ 52, 41 ],
    shadowSize: [ 52, 41 ],
    iconAnchor: [ 26, 41 ],
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
            icon_biking,
            mapOptions: {
                zoomSnap: 0.5,
                zoomDelta: 0.5,
            },
            auto_center: true,
            centering_in_progress: true,
            center_timer: null,
        };
    },
    computed: {
        cssVars() {
            // https://www.telerik.com/blogs/passing-variables-to-css-on-a-vue-component
            return {
            };
        },
    },
    watch: {
        "$store.state.current_location": function (loc) {
            this.$store.state.app_bar_info = loc.lat.toFixed(4) + ", " + loc.lng.toFixed(4);
            if (this.auto_center && this.map) {
                console.log("Centering to " + loc);
                this.map.flyTo(loc);
            }
        },
    },
    methods: {
        map_is_ready: function () {
            console.log("Map is ready");
            this.map = this.$refs.site_map.mapObject;
        },
        zoomUpdate: function (zoom) {
            this.currentZoom = zoom;
            console.log("zoom=" + zoom);
        },
        centerUpdate: function (center) {
            this.currentCenter = center;
        },
        user_drag: function() {
            this.auto_center = false;
        },
        unit_cluster_icon: function (cluster) {
            var childCount = cluster.getChildCount();
            return new L.DivIcon({ html: "<div><span>" + childCount + "</span></div>", className: "marker-cluster unit-cluster", iconSize: new L.Point(40, 40) });
        },
        toggle_auto_center: function() {
            this.auto_center = !this.auto_center;
            if (this.auto_center) {
                this.map.flyTo(this.$store.state.current_location);
            }
        },
        nearest_station: function (_for) {
            const loc = this.$store.state.current_location;
            let bounds = latLngBounds([loc]);
            this.$store.state.ax.get("/v0/station?for=" + _for + "&num=1&lat=" + loc.lat + "&lon=" + loc.lng).then(response => {
                if (response.data && response.data[0] && response.data[0]._id) {
                    // although we got the whole station record, look it up in the store instead,
                    // first because it already has a .loc field, second because the reactivity is
                    // preserved if we only refer to the same object (instead of a new object with the same content)
                    const best_station = this.$store.state.data.stations[response.data[0]._id];
                    console.log("nearest: " + JSON.stringify(best_station));
                    this.auto_center = false;
                    bounds.extend(best_station.loc);

                    this.map.flyTo(best_station.loc, 16.5);

                    //this.map.fitBounds(bounds, {});
                }
            });
        },
        unit_clicked: function (u) {
            console.log("TODO: unit clicked: " + JSON.stringify(u));
        },
        station_clicked: function (st) {
            console.log("TODO: station clicked: " + JSON.stringify(st));
        },
    },
    created: function() {
        console.log("SiteMap created");
        EventBus.$on("keepalive", () => {
            console.log("keepalive...");
        });
        this.$store.state.app_bar_info = "Stations";
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
