<template>
    <div class="trainer-tracker fill-height">
        <l-map
            ref="trainerMap" @ready="map_is_ready"
            :zoom="zoom"
            :center="center"
            :options="mapOptions"
            style="height: 100%"
            @update:center="centerUpdate"
            @update:zoom="zoomUpdate"
        >
            <l-tile-layer :url="tile_url" :attribution="tile_attribution"/>
            <l-geo-json :geojson="shops" :optionsStyle="style_extractor"/>
            <l-marker ref="trainerPos" :lat-lng="trainer_latlng">
                <l-tooltip :options="{ permanent: true }">
                    Akeem
                </l-tooltip>
            </l-marker>
        </l-map>

    </div>
</template>

<script>
import S4Date from "@/plugins/S4Date";
// @ is an alias to /src
import { latLng } from "leaflet";
import { LMap, LTileLayer, LGeoJson, LMarker, LTooltip } from "vue2-leaflet";

import { Icon } from "leaflet";

delete Icon.Default.prototype._getIconUrl;
Icon.Default.mergeOptions({
    iconRetinaUrl: require("leaflet/dist/images/marker-icon-2x.png"),
    iconUrl: require("leaflet/dist/images/marker-icon.png"),
    shadowUrl: require("leaflet/dist/images/marker-shadow.png"),
});

const sim_route = [
    { latitude: 25.04334, longitude: 55.24618, time: 2.0 },
    { latitude: 25.04334, longitude: 55.24633, time: 2.0 },
    { latitude: 25.04342, longitude: 55.24647, time: 2.0 },
    { latitude: 25.04353, longitude: 55.24652, time: 2.0 },
    { latitude: 25.04369, longitude: 55.24649, time: 2.0 },
    { latitude: 25.04381, longitude: 55.24642, time: 2.0 },
    { latitude: 25.04398, longitude: 55.24646, time: 2.0 },
    { latitude: 25.04413, longitude: 55.24652, time: 2.0 },
    { latitude: 25.04442, longitude: 55.24666, time: 2.0 },
    { latitude: 25.04472, longitude: 55.24681, time: 2.0 },
    { latitude: 25.04494, longitude: 55.24691, time: 2.0 },
    { latitude: 25.04515, longitude: 55.24702, time: 2.0 },
    { latitude: 25.04531, longitude: 55.24710, time: 2.0 },
    { latitude: 25.04538, longitude: 55.24721, time: 2.0 },
    { latitude: 25.04542, longitude: 55.24731, time: 2.0 },
    { latitude: 25.04550, longitude: 55.24739, time: 2.0 },
    { latitude: 25.04562, longitude: 55.24743, time: 2.0 },
    { latitude: 25.04570, longitude: 55.24741, time: 2.0 },
    { latitude: 25.04575, longitude: 55.24732, time: 2.0 },
    { latitude: 25.04575, longitude: 55.24723, time: 2.0 },
    { latitude: 25.04571, longitude: 55.24716, time: 2.0 },
    { latitude: 25.04564, longitude: 55.24714, time: 2.0 },
    { latitude: 25.04558, longitude: 55.24713, time: 2.0 },
    { latitude: 25.04553, longitude: 55.24715, time: 2.0 },
    { latitude: 25.04544, longitude: 55.24713, time: 2.0 },
    { latitude: 25.04531, longitude: 55.24708, time: 2.0 },
    { latitude: 25.04511, longitude: 55.24699, time: 2.0 },
    { latitude: 25.04469, longitude: 55.24677, time: 2.0 },
    { latitude: 25.04455, longitude: 55.24671, time: 2.0 },
    { latitude: 25.04440, longitude: 55.24664, time: 2.0 },
    { latitude: 25.04426, longitude: 55.24656, time: 2.0 },
    { latitude: 25.04409, longitude: 55.24646, time: 2.0 },
    { latitude: 25.04399, longitude: 55.24628, time: 2.0 },
    { latitude: 25.04389, longitude: 55.24613, time: 2.0 },
    { latitude: 25.04374, longitude: 55.24602, time: 2.0 },
    { latitude: 25.04361, longitude: 55.24598, time: 2.0 },
];


import { transform_base, norm2latlng, shops } from "../modules/geoshapes";

var sim_route_idx = 0, sim_route_next_idx = 1;
const sim_route_delta_t = 0.05;
var sim_route_t = 0;
var sim_latitude = 0, sim_longitude = 0; // eslint-disable-line no-unused-vars

function sim_route_step() {
    sim_route_t += sim_route_delta_t;
    if (sim_route_t >= sim_route[sim_route_idx].time) {
        sim_route_t -= sim_route[sim_route_idx].time;
        sim_route_idx = sim_route_next_idx;
        sim_route_next_idx = (1 + sim_route_next_idx) % sim_route.length;
    }
    sim_latitude = (sim_route[sim_route_idx].latitude * (sim_route[sim_route_idx].time - sim_route_t) + sim_route[sim_route_next_idx].latitude * sim_route_t) / sim_route[sim_route_idx].time;
    sim_longitude = (sim_route[sim_route_idx].longitude * (sim_route[sim_route_idx].time - sim_route_t) + sim_route[sim_route_next_idx].longitude * sim_route_t) / sim_route[sim_route_idx].time;
}

setInterval(sim_route_step, sim_route_delta_t * 1000);

export default {
    name: "TrainerTracker",
    components: {
        LMap,
        LTileLayer,
        LGeoJson,
        LMarker,
        LTooltip,
    },
    data () {
        return {
            tile_url: "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
            tile_attribution: "&copy; <a href=\"http://osm.org/copyright\">OpenStreetMap</a> contributors",
            map: false,
            center: norm2latlng([0.5, 0.5]),
			zoom: 17,

            marker_updater_id: false,
            trainer_latlng: latLng(25.04460, 55.24686),
            currentZoom: false,
            currentCenter: false,
            showParagraph: false,
            mapOptions: {
                zoomSnap: 0.5
            },
            showMap: true,
            shops,
        };
    },
    computed: {
    },
    methods: {
        map_is_ready: function () {
            console.log("Map is ready");
            this.map = this.$refs.trainerMap.mapObject;
            this.marker_updater_id = setInterval(this.update_marker, 100);
        },
        zoomUpdate: function (zoom) {
            this.currentZoom = zoom;
        },
        centerUpdate: function (center) {
            this.currentCenter = center;
        },
        update_marker: function () {
            this.trainer_latlng = transform_base(latLng(sim_latitude, sim_longitude));
            const eta = (sim_route.length - sim_route_idx) * 2.0 - sim_route_t;
            this.$store.state.app_bar_info = "ETA: " + S4Date.twodigit(0 | (eta / 60)) + ":" + S4Date.twodigit(0 | (eta % 60));
            this.$refs.trainerPos.setLatLng(this.trainer_latlng);
        },
        style_extractor: function (feature) {
            return feature.properties;
        },
    },
    created: function() {
        console.log("TrainerTracker created");
        // this.$refs.trainerMap.mapObject.ANY_LEAFLET_MAP_METHOD();
    },
    beforeDestroy: function () {
        if (this.marker_updater_id) {
            clearInterval(this.marker_updater_id);
        }
        this.$store.state.app_bar_info = "..."
    },
}
</script>

<style lang="scss">
@import "~vuetify/src/styles/main.sass";

</style>

// vim: set sw=4 ts=4 indk= et:
