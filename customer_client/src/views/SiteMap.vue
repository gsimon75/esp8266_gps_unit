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
                <v-btn small color="primary" @click="nearest_to_take">Nearest<br>to take</v-btn>
                <v-btn small color="error" @click="nearest_to_return">Nearest<br>to return</v-btn>
            </l-control>
            <l-control position="bottomright" >
                <v-btn fab color="primary" @click="enable_auto_center"><v-icon>fas fa-crosshairs</v-icon></v-btn>
            </l-control>

            <v-marker-cluster>
            <template v-for="(st, id) in $store.state.stations">
                <l-marker :lat-lng="st.loc" :key="id" @click="station_clicked(id)">
                    <l-clickable-tooltip @click="station_clicked(id)">
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

            <l-marker ref="current_pos" :icon="($store.state.scooters_in_use.length > 0) ? icon_biking : null" :lat-lng="$store.state.current_location"/>
        </l-map>

    </div>
</template>

<script>
// @ is an alias to /src
import { EventBus } from "@/modules/event-bus";
import L from "leaflet";
import { latLng } from "leaflet";
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

const icon_biking = L.icon({
    iconUrl: require("../assets/marker-biking.png"),
    shadowUrl: require("../assets/marker-biking-shadow.png"),
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
            icon_biking,
            mapOptions: {
                zoomSnap: 0.5
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
            if (this.auto_center) { // meters
                this.$refs.site_map.setCenter(loc);
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
        },
        centerUpdate: function (center) {
            this.currentCenter = center;
        },
        user_drag: function() {
            this.auto_center = false;
        },
        enable_auto_center: function() {
            this.$refs.site_map.setCenter(this.$store.state.current_location);
            this.auto_center = true;
        },
        nearest_to_take: function () {
            /*const best_station = nearest_station(this.stations.filter(st => st.ready > 0));
            if (best_station !== undefined) {
                console.log("nearest_to_take: " + JSON.stringify(best_station));
                this.auto_center = false;
                this.$refs.site_map.setCenter(best_station.loc);
            }*/
        },
        nearest_to_return: function () {
            /*const best_station = nearest_station(this.stations.filter(st => st.free > 0));
            if (best_station !== undefined) {
                console.log("nearest_to_take: " + JSON.stringify(best_station));
                this.auto_center = false;
                this.$refs.site_map.setCenter(best_station.loc);
            }*/
        },
    },
    created: function() {
        console.log("SiteMap created, store.state.sign_in_ready=" + this.$store.state.sign_in_ready);
        // this.$refs.site_map.mapObject.ANY_LEAFLET_MAP_METHOD();
        if (!this.$store.getters.is_logged_in) {
            console.log("Not signed in, proceed to sign-in");
            this.$router.push("/signin");
        }
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

</style>

// vim: set sw=4 ts=4 indk= et:
