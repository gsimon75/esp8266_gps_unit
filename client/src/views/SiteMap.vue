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
            <l-control position="bottomleft" >
                <v-btn small color="primary" @click="nearest_to_take">Nearest<br>to take</v-btn>
                <v-btn small color="error" @click="nearest_to_return">Nearest<br>to return</v-btn>
            </l-control>
            <l-control position="bottomright" >
                <v-btn fab color="primary" @click="enable_auto_center"><v-icon>fas fa-crosshairs</v-icon></v-btn>
            </l-control>
            <l-tile-layer :url="tile_url" :attribution="tile_attribution"/>
            <l-geo-json :geojson="shops" :optionsStyle="style_extractor"/>
            <l-marker ref="current_pos" :icon="($store.getters.scooters_in_use.length > 0) ? icon_biking : null" :lat-lng="$store.state.current_location"/>

            <template v-for="st in stations">
                <l-marker :lat-lng="st.loc" :key="st.id">
                    <l-icon :icon-size="[40, 36]">
                        <v-badge color="blue" :content="st.id" left>
                        <v-badge :value="st.ready" color="green" :content="st.ready" overlap>
                        <v-badge :value="st.free" color="red" :content="st.free" bottom overlap>
                            <v-icon large color="black">
                                fas fa-charging-station
                            </v-icon>
                        </v-badge>
                        </v-badge>
                        </v-badge>
                    </l-icon>
                </l-marker>
            </template>
        </l-map>

    </div>
</template>

<script>
// @ is an alias to /src
import L from "leaflet";
import { LMap, LControl, LControlScale, LTileLayer, LGeoJson, LMarker, LIcon } from "vue2-leaflet";

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

import { norm2latlng, nearest_station, shops, stations } from "../modules/geoshapes";

export default {
    name: "SiteMap",
    components: {
        LMap,
        LControl,
        LControlScale,
        LTileLayer,
        LGeoJson,
        LMarker,
        LIcon,
    },
    data () {
        return {
            tile_url: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
            tile_attribution: "&copy; <a href=\"http://osm.org/copyright\">OpenStreetMap</a> contributors",
            map: false,
            currentCenter: norm2latlng([0.5, 0.5]),
            currentZoom: 17,
            icon_biking,
            mapOptions: {
                zoomSnap: 0.5
            },
            auto_center: true,
            centering_in_progress: true,
            center_timer: null,
            shops,
            stations,
        };
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
            this.auto_center = true;
        },
        style_extractor: function (feature) {
            return feature.properties;
        },
        nearest_to_take: function () {
            const best_station = nearest_station(stations.filter(st => st.ready > 0));
            if (best_station !== undefined) {
                console.log("nearest_to_take: " + JSON.stringify(best_station));
                this.auto_center = false;
                this.$refs.site_map.setCenter(best_station.loc);
            }
        },
        nearest_to_return: function () {
            const best_station = nearest_station(stations.filter(st => st.free > 0));
            if (best_station !== undefined) {
                console.log("nearest_to_take: " + JSON.stringify(best_station));
                this.auto_center = false;
                this.$refs.site_map.setCenter(best_station.loc);
            }
        },
    },
    created: function() {
        console.log("SiteMap created");
        // this.$refs.site_map.mapObject.ANY_LEAFLET_MAP_METHOD();
        if (!this.$store.getters.is_logged_in) {
            console.log("Not signed in, proceed to sign-in");
            this.$router.push("/signin");
        }
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
