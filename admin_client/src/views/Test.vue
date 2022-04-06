<template>
    <div class="test">
        <s4-infinite-list :supply-items="gen_items" :max-items="19">
            <template v-slot:header>
                <v-list-item>
                    <v-list-item-content class="text-center">
                        <span>No preceding entries</span>
                    </v-list-item-content>
                </v-list-item>
            </template>

            <template v-slot:item="{item, idx}">
                <v-list-item two-line :class="'bg-' + (idx % 2)">
                    <v-list-item-content>
                        <v-list-item-title>{{ new Date(item.time * 1000).toLocaleString() }}</v-list-item-title>
                        <v-list-item-subtitle>{{ loc2str(item.loc) }}</v-list-item-subtitle>
                    </v-list-item-content>
                </v-list-item>
            </template>

            <template v-slot:footer>
                <v-list-item>
                    <v-list-item-content class="text-center">
                        <span>No further entries</span>
                    </v-list-item-content>
                </v-list-item>
            </template>
        </s4-infinite-list>
    </div>
</template>

<script>
// @ is an alias to /src
import { latLng } from "leaflet";
import S4InfiniteList from "@/components/S4InfiniteList"

export default {
    name: "TestPage",
    components: {
        S4InfiniteList,
    },
    data () {
        return {
        };
    },
    methods: {
        loc2str: function (loc) {
            return "(" + loc.lat.toFixed(4) + ", " + loc.lng.toFixed(4) + ")";
        },
        gen_items: function (limit, num, before) {
            let limit_time = (limit === null) ? Math.round(Date.now() / 1000) : limit.time;
            //console.log("gen_items(" + limit + ", " + num + ", " + before + ")");
            let query = "/v0/unit/trace/Simulated?" + (before ? "until" : "from") + "=" + limit_time + "&num=8";
            return this.$store.state.ax.get(query).then(response => {
                // for backward search: result is expected in a latest-first order
                // for forward search: result is expected in an earliest-first order
                //console.log("response=" + JSON.stringify(response));
                let new_items = [];

                response.data.forEach(u => {
                    new_items.push({
                        time: u.time,
                        unit: u.unit,
                        loc: latLng(u.lat, u.lon),
                        azi: u.azi,
                        spd: u.spd,
                        spdt: (u.spd * 3.6).toFixed(1),
                    });
                });
                if (response.data.length < num) {
                    new_items.push(null);
                }

                return new_items;
            });
        },
    },
}
</script>

<style lang="scss" scoped>
    .test {
        height: 100%;
        display: flex;
        flex-direction: column;
    }

    .bg-0 {
        background-color: #ddf;
    }

    .bg-1, .bg--1 {
        background-color: #eef;
    }

</style>

// vim: set sw=4 ts=4 et indk= :
