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
            <l-control-scale position="topright" :imperial="false" :metric="true"/>
            <l-control position="bottomleft" >
                <v-btn small color="primary" @click="nearest_station">Nearest<br>Station</v-btn>
            </l-control>
            <l-tile-layer :url="tile_url" :attribution="tile_attribution"/>
            <l-geo-json :geojson="shops" :optionsStyle="style_extractor"/>
            <l-marker ref="trainerPos" :lat-lng="trainer_latlng"/>

            <template v-for="(st, st_id) in stations">
                <l-marker :lat-lng="st.latLng" :key="st_id">
                    <l-icon :icon-size="[40, 36]">
                        <v-badge :color="st.ready ? 'green' : 'red'" :content="st.ready + '/' + st.free">
                            <v-icon large>
                                fas fa-charging-station
                            </v-icon>
                        </v-badge>
                    </l-icon>
                </l-marker>
            </template>
        </l-map>

    </div>
</template>

<script>
// @ is an alias to /src
import { latLng } from "leaflet";
import { LMap, LControl, LControlScale, LTileLayer, LGeoJson, LMarker, LIcon } from "vue2-leaflet";

import { Icon } from "leaflet";

delete Icon.Default.prototype._getIconUrl;
Icon.Default.mergeOptions({
    iconRetinaUrl: require("leaflet/dist/images/marker-icon-2x.png"),
    iconUrl: require("leaflet/dist/images/marker-icon.png"),
    shadowUrl: require("leaflet/dist/images/marker-shadow.png"),
});

import { norm2latlng, shops, stations, current_location } from "../modules/geoshapes";

export default {
    name: "TrainerTracker",
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
            stations,
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
            this.trainer_latlng = current_location;
            this.$store.state.app_bar_info = current_location.lat + ", " + current_location.lng;
            this.$refs.trainerPos.setLatLng(this.trainer_latlng);
        },
        style_extractor: function (feature) {
            return feature.properties;
        },
        nearest_station: function () {
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
